#!/usr/bin/env python3
"""Archive live cdev poll/block/batch/IRQ samples into a 0.1.1.5 measurement report."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import statistics
import sys
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_CONTRACT = ROOT / "tests/performance/milestone-0.1.1.0-measurement.json"


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


def parse_live_output(text: str) -> tuple[list[dict[str, Any]], dict[str, str], list[str]]:
    rows: list[dict[str, Any]] = []
    metadata: dict[str, str] = {}
    notes: list[str] = []
    for line in text.splitlines():
        if line.startswith("METAFLUX_SAMPLE\t"):
            fields = line.split("\t")
            if len(fields) != 5:
                raise RuntimeError(f"malformed sample: {line!r}")
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
        elif line.strip():
            notes.append(line.strip())
    if not rows:
        raise RuntimeError("no METAFLUX_SAMPLE rows in live output")
    return rows, metadata, notes


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--live-stdout", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--contract", type=Path, default=DEFAULT_CONTRACT)
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    live_text = arguments.live_stdout.read_text(encoding="utf-8")
    rows, metadata, notes = parse_live_output(live_text)
    contract = json.loads(arguments.contract.read_text(encoding="utf-8"))

    by_metric: dict[str, list[int]] = {}
    for row in rows:
        by_metric.setdefault(row["metric"], []).append(int(row["value"]))
    metrics = {name: summarize(values) for name, values in sorted(by_metric.items())}

    required = {
        "cdev_poll_online_ns",
        "cdev_block_wait_empty_ns",
        "cdev_batch_submit_ns",
        "cdev_irq_eventfd_roundtrip_ns",
    }
    missing = sorted(required - set(metrics))
    if missing:
        raise SystemExit(f"missing live metrics: {missing}")

    sample_count = int(metadata.get("sample_count", "0"))
    warmup_count = int(metadata.get("warmup_count", "0"))
    expected = int(contract["samples"]["count"])
    if sample_count != expected:
        raise SystemExit(f"sample_count {sample_count} != contract {expected}")
    for name, summary in metrics.items():
        if summary["count"] != expected:
            raise SystemExit(f"{name} count {summary['count']} != {expected}")

    passed = any("cdev qualification: PASS" in note for note in notes)
    if not passed:
        raise SystemExit("live stdout missing cdev qualification PASS")

    output_dir = arguments.output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    raw_csv = output_dir / "raw-samples.csv"
    with raw_csv.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=["metric", "sample_index", "value", "unit"])
        writer.writeheader()
        for row in rows:
            writer.writerow(row)

    affinity = sorted(os.sched_getaffinity(0))
    report = {
        "id": "transport.profile.milestone-0.1.1.5.live-cdev.v0",
        "status": "measured-live-cdev",
        "contract": {
            "path": arguments.contract.resolve().relative_to(ROOT).as_posix()
            if arguments.contract.resolve().is_relative_to(ROOT)
            else str(arguments.contract),
            "sha256": sha256_file(arguments.contract.resolve()),
            "id": contract["id"],
            "warmup_count": warmup_count,
            "sample_count": sample_count,
        },
        "host": {
            "kernel_release": platform.release(),
            "machine": platform.machine(),
            "platform": platform.platform(),
            "affinity": affinity,
            "selected_client_cpu": min(affinity) if affinity else 0,
            "metaflux_cdev_present": True,
        },
        "metadata": metadata,
        "qualification_notes": notes,
        "metrics": metrics,
        "modes": {
            "poll": {"metric": "cdev_poll_online_ns", "status": "measured"},
            "block": {"metric": "cdev_block_wait_empty_ns", "status": "measured"},
            "batch": {"metric": "cdev_batch_submit_ns", "status": "measured", "batch_size": 8},
            "irq": {
                "metric": "cdev_irq_eventfd_roundtrip_ns",
                "status": "measured",
                "path": "leased_completion_eventfd",
            },
        },
        "artifacts": {
            "live_stdout": str(arguments.live_stdout.resolve()),
            "live_stdout_sha256": sha256_file(arguments.live_stdout.resolve()),
            "raw_samples_csv": raw_csv.name,
            "raw_samples_sha256": sha256_file(raw_csv),
        },
        "still_host_pending": [
            "guest-vfio-user-add-copy under pinned QEMU/libvfio-user",
            "forced huge-page binding differential",
            "interrupt-moderation soak under MSI-X storm",
        ],
    }
    report_path = output_dir / "live-cdev-profile-report.json"
    report_path.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(
        json.dumps(
            {
                "status": report["status"],
                "report": str(report_path),
                "raw_samples": str(raw_csv),
                "metrics": sorted(metrics),
                "sample_count": sample_count,
            },
            sort_keys=True,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
