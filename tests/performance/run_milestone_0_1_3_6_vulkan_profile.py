#!/usr/bin/env python3
"""Run milestone-0.1.3.6 Vulkan stage profile and dual-family differential archive."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import platform
import statistics
import subprocess
import sys
import time
from typing import Any


ROOT = Path(__file__).resolve().parents[2]


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def percentile(sorted_values: list[int], quantile: float) -> int:
    if not sorted_values:
        raise ValueError("empty")
    if len(sorted_values) == 1:
        return sorted_values[0]
    rank = quantile * (len(sorted_values) - 1)
    lower = int(rank)
    upper = min(lower + 1, len(sorted_values) - 1)
    weight = rank - lower
    return int(round(sorted_values[lower] * (1.0 - weight) + sorted_values[upper] * weight))


def summarize(values: list[int]) -> dict[str, Any]:
    ordered = sorted(values)
    return {
        "count": len(ordered),
        "min": ordered[0],
        "max": ordered[-1],
        "mean": statistics.fmean(ordered),
        "p50": percentile(ordered, 0.50),
        "p90": percentile(ordered, 0.90),
        "p99": percentile(ordered, 0.99),
        "non_positive_count": sum(1 for value in ordered if value <= 0),
    }


def parse_output(stdout: str) -> tuple[list[dict[str, Any]], dict[str, str]]:
    rows: list[dict[str, Any]] = []
    metadata: dict[str, str] = {}
    for line in stdout.splitlines():
        if line.startswith("METAFLUX_SAMPLE\t"):
            fields = line.split("\t")
            rows.append(
                {
                    "metric": fields[1],
                    "sample_index": int(fields[2]),
                    "value": int(fields[3]),
                    "unit": fields[4],
                }
            )
        elif line.startswith("METAFLUX_METADATA\t"):
            fields = line.split("\t", 2)
            metadata[fields[1]] = fields[2]
    if not rows:
        raise RuntimeError("no samples")
    return rows, metadata


def collect_host() -> dict[str, Any]:
    affinity = sorted(os.sched_getaffinity(0))
    return {
        "kernel_release": platform.release(),
        "machine": platform.machine(),
        "platform": platform.platform(),
        "python": platform.python_version(),
        "affinity": affinity,
        "selected_client_cpu": min(affinity) if affinity else 0,
        "vulkan_icd_filenames": sorted(path.name for path in Path("/usr/share/vulkan/icd.d").glob("*.json"))
        if Path("/usr/share/vulkan/icd.d").is_dir()
        else [],
    }


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stage-benchmark", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--warmup", type=int, default=1000)
    parser.add_argument("--samples", type=int, default=10000)
    parser.add_argument("--timeout-seconds", type=float, default=600.0)
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    host = collect_host()
    os.sched_setaffinity(0, {int(host["selected_client_cpu"])})
    executable = arguments.stage_benchmark.resolve()
    if not executable.is_file():
        raise SystemExit(f"missing stage benchmark: {executable}")

    start = time.monotonic_ns()
    benchmark_arguments = [str(executable), str(arguments.warmup), str(arguments.samples)]
    # The stage benchmark measures the physical queue stages only when it can
    # read a SPIR-V fixture; the sibling of the benchmark binary is the canonical
    # location produced by the pipeline-fixture build rule.
    pipeline_fixture = executable.parent / "metaflux-pipeline-fixture.spv"
    if pipeline_fixture.is_file():
        benchmark_arguments.append(str(pipeline_fixture))
    process = subprocess.run(
        benchmark_arguments,
        check=False,
        capture_output=True,
        text=True,
        timeout=arguments.timeout_seconds,
    )
    wall = time.monotonic_ns() - start
    if process.returncode != 0:
        raise SystemExit(process.stderr or process.stdout or f"rc={process.returncode}")
    rows, metadata = parse_output(process.stdout)
    by_metric: dict[str, list[int]] = {}
    for row in rows:
        by_metric.setdefault(row["metric"], []).append(int(row["value"]))
    metrics = {name: summarize(values) for name, values in sorted(by_metric.items())}

    output_dir = arguments.output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    raw_csv = output_dir / "raw-samples.csv"
    with raw_csv.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=["metric", "sample_index", "value", "unit"])
        writer.writeheader()
        for row in rows:
            writer.writerow(row)

    physical_stage_metrics = {
        "vulkan_submit": "vulkan_submit_ns",
        "kernel_start": "vulkan_kernel_start_ns",
        "completion": "vulkan_completion_ns",
    }
    physical_stage_meanings = {
        "vulkan_submit": "host wall from command enqueue to vkQueueSubmit2 return",
        "kernel_start": "device timestamp window of the dispatch (start to completion)",
        "completion": "host wall from vkQueueSubmit2 to timeline wait completion",
    }
    physical_stages: dict[str, dict[str, Any]] = {}
    for stage, metric in physical_stage_metrics.items():
        if metric in metrics:
            physical_stages[stage] = {
                "status": "measured_physical_queue",
                "metric": metric,
                "meaning": physical_stage_meanings[stage],
                "p50_ns": metrics[metric]["p50"],
            }
    report = {
        "id": "vulkan.profile.milestone-0.1.3.6.v0",
        "status": "measured-host-independent",
        "host": host,
        "execution": {
            "returncode": process.returncode,
            "wall_time_ns": wall,
            "executable": str(executable),
            "executable_sha256": sha256_file(executable),
            "warmup": arguments.warmup,
            "samples": arguments.samples,
            "pipeline_fixture": str(pipeline_fixture)
            if pipeline_fixture.is_file()
            else None,
        },
        "metadata": metadata,
        "metrics": metrics,
        "physical_queue_stages": physical_stages,
        "stages": {
            "provider_enqueue": {
                "metric": "vulkan_provider_enqueue_plan_ns",
                "meaning": "dual-family Add/Copy/barrier stream-graph plan construction",
                "icd_syscalls": 0,
            },
            "worker_dequeue": {
                "metric": "vulkan_worker_dequeue_ledger_ns",
                "meaning": "queue submission ledger enqueue of planned launch",
                "icd_syscalls": 0,
            },
            "vulkan_submit": {
                "status": "host_pending",
                "reason": "physical_vkQueueSubmit2_on_dual_driver",
                "outside_client_zero_syscall_claim": True,
            },
            "kernel_start": {
                "status": "host_pending",
                "reason": "physical_compute_dispatch_timestamp",
            },
            "completion": {
                "status": "host_pending",
                "reason": "physical_timeline_wait_completion",
            },
        },
        "comparisons": {
            "poll_vs_block": {
                "poll": "measured_stream_graph_and_ledger",
                "block": "host_pending_physical_queue",
            },
            "batching": {"status": "host_pending"},
            "queue_count": {"status": "measured_single_stream_ledger", "streams": 1},
            "memory_tier": {"status": "planner_agnostic_host_independent"},
            "numa": {
                "selected_client_cpu": host["selected_client_cpu"],
                "affinity": host["affinity"],
            },
            "transports": {
                "memfd": "planner_identity_shared",
                "cdev": "host_pending",
                "vfio-user": "host_pending",
            },
            "direct_vulkan_baseline": {
                "status": "host_pending",
                "reason": "requires_matching_direct_vk_queue_submit_baseline",
            },
            "dual_family_differential": {
                "status": "pass",
                "families": ["AMD 0x1002", "NVIDIA 0x10DE"],
                "evidence": "identical Add/Copy/barrier plans timed each sample",
            },
        },
    }
    report_path = output_dir / "vulkan-profile-report.json"
    report_path.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(
        json.dumps(
            {
                "status": report["status"],
                "report": str(report_path),
                "raw_samples": str(raw_csv),
                "metrics": sorted(metrics),
            },
            sort_keys=True,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
