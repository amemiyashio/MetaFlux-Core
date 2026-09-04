#!/usr/bin/env python3
"""Run milestone-0.1.1.5 transport poll/block/NUMA profile and warm-path audit.

Binds to the frozen milestone-0.1.1.0 measurement contract. On hosts without
`/dev/metafluxN` the local-cdev and guest-vfio-user Add/Copy workloads remain
host-pending; the memfd client-fastpath ring still measures the contract clock,
poll-mode warm path, NUMA/affinity fingerprints, and allocation/lock audit.
"""

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
import subprocess
import sys
import time
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_CONTRACT = ROOT / "tests/performance/milestone-0.1.1.0-measurement.json"
ARCHIVE_TOOL = ROOT / "tools/archive-transport-measurement.py"


def read_text(path: Path) -> str | None:
    try:
        return path.read_text(encoding="utf-8").strip()
    except (OSError, UnicodeError):
        return None


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def percentile(sorted_values: list[int], quantile: float) -> int:
    if not sorted_values:
        raise ValueError("empty sample set")
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
    }


def parse_benchmark_output(stdout: str) -> tuple[list[dict[str, Any]], dict[str, str]]:
    rows: list[dict[str, Any]] = []
    metadata: dict[str, str] = {}
    for line in stdout.splitlines():
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
            if len(fields) != 3 or fields[1] in metadata:
                raise RuntimeError(f"malformed metadata: {line!r}")
            metadata[fields[1]] = fields[2]
    if not rows:
        raise RuntimeError("benchmark emitted no samples")
    return rows, metadata


def parse_cpu_list(text: str | None) -> list[int]:
    if not text:
        return []
    cpus: list[int] = []
    for part in text.split(","):
        part = part.strip()
        if not part:
            continue
        if "-" in part:
            start_text, end_text = part.split("-", 1)
            cpus.extend(range(int(start_text), int(end_text) + 1))
        else:
            cpus.append(int(part))
    return sorted(set(cpus))


def collect_host_fingerprints() -> dict[str, Any]:
    online_cpus = parse_cpu_list(read_text(Path("/sys/devices/system/cpu/online")))
    affinity = sorted(os.sched_getaffinity(0))
    client_cpu = min(affinity) if affinity else (online_cpus[0] if online_cpus else 0)
    numa_nodes = parse_cpu_list(read_text(Path("/sys/devices/system/node/online")))
    node_map: dict[str, Any] = {}
    for node in numa_nodes:
        node_path = Path(f"/sys/devices/system/node/node{node}")
        node_map[str(node)] = {
            "cpus": parse_cpu_list(read_text(node_path / "cpulist")),
            "meminfo": read_text(node_path / "meminfo"),
        }
    cpu_model = None
    for line in (read_text(Path("/proc/cpuinfo")) or "").splitlines():
        if line.lower().startswith("model name"):
            cpu_model = line.split(":", 1)[1].strip()
            break
    thp = read_text(Path("/sys/kernel/mm/transparent_hugepage/enabled"))
    hugetlb = read_text(Path("/proc/meminfo"))
    hugetlb_total = None
    if hugetlb:
        match = re.search(r"^HugePages_Total:\s+(\d+)", hugetlb, re.M)
        if match:
            hugetlb_total = int(match.group(1))
    return {
        "kernel_release": platform.release(),
        "kernel_version": platform.version(),
        "machine": platform.machine(),
        "platform": platform.platform(),
        "python": platform.python_version(),
        "cpu_model": cpu_model,
        "online_cpus": online_cpus,
        "effective_affinity_cpus": affinity,
        "selected_client_cpu": client_cpu,
        "numa_nodes_online": numa_nodes,
        "numa_topology": node_map,
        "cross_numa_eligible": False,
        "transparent_hugepage_enabled": thp,
        "hugepages_total": hugetlb_total,
        "metaflux_cdev_present": any(Path("/dev").glob("metaflux[0-9]*")),
        "metaflux_ctl_present": Path("/dev/metafluxctl").exists(),
    }


def pin_to_client_cpu(cpu: int) -> None:
    os.sched_setaffinity(0, {cpu})


def run_ring(
    executable: Path,
    warmup: int,
    samples: int,
    *,
    audit: bool,
    timeout: float,
) -> tuple[list[dict[str, Any]], dict[str, str], dict[str, Any]]:
    arguments = [str(warmup), str(samples)]
    if audit:
        arguments.append("--audit")
    start = time.monotonic_ns()
    process = subprocess.run(
        [str(executable), *arguments],
        check=False,
        capture_output=True,
        text=True,
        timeout=timeout,
    )
    wall = time.monotonic_ns() - start
    if process.returncode != 0:
        raise RuntimeError(
            f"ring benchmark failed rc={process.returncode} stderr={process.stderr!r}"
        )
    if audit:
        metadata: dict[str, str] = {}
        for line in process.stdout.splitlines():
            if line.startswith("METAFLUX_METADATA\t"):
                fields = line.split("\t", 2)
                if len(fields) == 3:
                    metadata[fields[1]] = fields[2]
        record = {
            "status": "measured",
            "mode": "audit",
            "returncode": process.returncode,
            "wall_time_ns": wall,
            "stderr_lines": process.stderr.splitlines(),
        }
        return [], metadata, record
    rows, metadata = parse_benchmark_output(process.stdout)
    record = {
        "status": "measured",
        "mode": "poll",
        "returncode": process.returncode,
        "wall_time_ns": wall,
        "stderr_lines": process.stderr.splitlines(),
    }
    return rows, metadata, record


def write_raw_csv(path: Path, rows: list[dict[str, Any]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(
            handle, fieldnames=["metric", "sample_index", "value", "unit", "mode"]
        )
        writer.writeheader()
        for row in rows:
            writer.writerow(row)


def load_contract(path: Path) -> dict[str, Any]:
    document = json.loads(path.read_text(encoding="utf-8"))
    if document.get("id") != "transport.measurement.v0":
        raise RuntimeError(f"unexpected contract id: {document.get('id')}")
    return document


def build_report(
    *,
    contract: dict[str, Any],
    contract_path: Path,
    host: dict[str, Any],
    poll_rows: list[dict[str, Any]],
    poll_metadata: dict[str, str],
    poll_record: dict[str, Any],
    audit_metadata: dict[str, str],
    audit_record: dict[str, Any],
    warmup: int,
    samples: int,
    ring_executable: Path,
) -> dict[str, Any]:
    by_metric: dict[str, list[int]] = {}
    for row in poll_rows:
        by_metric.setdefault(row["metric"], []).append(int(row["value"]))
    metric_summaries = {
        name: {**summarize(values), "non_positive_count": sum(1 for value in values if value <= 0)}
        for name, values in sorted(by_metric.items())
    }
    require_positive = bool(contract.get("samples", {}).get("positive", True))
    positive_ok = all(summary["non_positive_count"] == 0 for summary in metric_summaries.values())
    if require_positive and not positive_ok:
        # CLOCK_MONOTONIC_RAW can return equal timestamps when work is faster than
        # timer resolution; retain raw zeros and flag contract pressure.
        positive_compliance = {
            "required": True,
            "status": "violated",
            "reason": "zero_or_negative_sample_observed",
        }
    else:
        positive_compliance = {"required": require_positive, "status": "ok"}

    warm_path = contract["queue"]["warm_path"]
    audit_heap = int(audit_metadata.get("audit_heap_allocation_attempts", "1"))
    audit_locks = int(audit_metadata.get("audit_global_lock_acquisitions", "1"))
    audit_pass = (
        audit_metadata.get("workload") == "active_memfd_ring_audit"
        and audit_heap == warm_path["allocations"]
        and audit_locks == warm_path["global_locks"]
        and int(audit_metadata.get("audit_dispatches", "0")) == samples
    )

    cdev_present = bool(host["metaflux_cdev_present"])
    if not cdev_present:
        status = "measured-partial"
    elif positive_compliance["status"] != "ok":
        status = "measured-with-timer-resolution-pressure"
    else:
        status = "measured"
    return {
        "id": "transport.profile.milestone-0.1.1.5.v0",
        "status": status,
        "sample_positive_compliance": positive_compliance,
        "scope": "milestone-0.1.1.5-transport-performance",
        "contract": {
            "path": contract_path.relative_to(ROOT).as_posix(),
            "sha256": sha256_file(contract_path),
            "id": contract["id"],
            "version": contract["version"],
            "warmup_count": warmup,
            "sample_count": samples,
            "clock": contract["clock"],
            "active_modes": contract["queue"]["active_modes"],
            "warm_path": warm_path,
        },
        "host": host,
        "artifacts": {
            "ring_benchmark": {
                "path": str(ring_executable),
                "sha256": sha256_file(ring_executable),
            }
        },
        "modes": {
            "poll": {
                "status": "measured",
                "implementation": "memfd_client_fastpath_try_submit_try_consume",
                "metadata": poll_metadata,
                "execution": poll_record,
                "metrics": metric_summaries,
            },
            "block": {
                "status": "host_pending" if not cdev_present else "not_run",
                "reason": (
                    "requires_local_cdev_or_vfio_user_worker_arm_path"
                    if not cdev_present
                    else "block_mode_runner_not_wired_for_live_cdev_on_this_pass"
                ),
            },
            "interrupt_moderation": {
                "status": "host_pending",
                "reason": "requires_msi_x_eventfd_path_under_qemu_or_live_cdev",
            },
            "batching": {
                "status": "host_pending",
                "reason": "requires_multi_descriptor_worker_batch_path_on_cdev_or_guest",
            },
            "huge_pages": {
                "status": "fingerprinted",
                "transparent_hugepage_enabled": host.get("transparent_hugepage_enabled"),
                "hugepages_total": host.get("hugepages_total"),
                "workload_binding": "not_forced_on_memfd_ring_path",
            },
            "numa": {
                "status": "measured",
                "selected_client_cpu": host["selected_client_cpu"],
                "nodes_online": host["numa_nodes_online"],
                "cross_numa_eligible": host["cross_numa_eligible"],
                "topology": host["numa_topology"],
            },
        },
        "warm_path_audit": {
            "status": "pass" if audit_pass else "fail",
            "metadata": audit_metadata,
            "execution": audit_record,
            "expected": {
                "allocations": warm_path["allocations"],
                "global_locks": warm_path["global_locks"],
                "syscalls": warm_path["syscalls"],
            },
            "observed": {
                "audit_heap_allocation_attempts": audit_heap,
                "audit_global_lock_acquisitions": audit_locks,
                "audit_dispatches": int(audit_metadata.get("audit_dispatches", "0")),
                "audit_consumer_doorbells": int(
                    audit_metadata.get("audit_consumer_doorbells", "0")
                ),
                "audit_producer_doorbells": int(
                    audit_metadata.get("audit_producer_doorbells", "0")
                ),
            },
        },
        "workloads": {
            "memfd_ring_submit_consume_poll": {
                "status": "measured",
                "maps_to_contract_workloads": [
                    "local-cdev-add-copy-warm-path-proxy",
                    "guest-vfio-user-add-copy-warm-path-proxy",
                ],
                "note": (
                    "memfd client fastpath is the host-available warm-path proxy when "
                    "cdev/vfio-user devices are absent"
                ),
            },
            "local-cdev-add-copy": {
                "status": "host_pending" if not cdev_present else "not_run",
                "reason": "missing_/dev/metafluxN" if not cdev_present else "not_run",
            },
            "guest-vfio-user-add-copy": {
                "status": "host_pending",
                "reason": "requires_qemu_libvfio_user_fixture",
            },
        },
    }


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ring-benchmark", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--contract", type=Path, default=DEFAULT_CONTRACT)
    parser.add_argument(
        "--warmup",
        type=int,
        default=None,
        help="override contract warmup count (self-test only)",
    )
    parser.add_argument(
        "--samples",
        type=int,
        default=None,
        help="override contract sample count (self-test only)",
    )
    parser.add_argument("--timeout-seconds", type=float, default=600.0)
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    contract = load_contract(arguments.contract.resolve())
    warmup = arguments.warmup if arguments.warmup is not None else int(contract["warmup"]["count"])
    samples = (
        arguments.samples if arguments.samples is not None else int(contract["samples"]["count"])
    )
    if warmup <= 0 or samples <= 0:
        raise SystemExit("warmup and samples must be positive")

    host = collect_host_fingerprints()
    pin_to_client_cpu(int(host["selected_client_cpu"]))
    host["effective_affinity_cpus_after_pin"] = sorted(os.sched_getaffinity(0))

    ring = arguments.ring_benchmark.resolve()
    if not ring.is_file():
        raise SystemExit(f"ring benchmark missing: {ring}")

    poll_rows, poll_metadata, poll_record = run_ring(
        ring, warmup, samples, audit=False, timeout=arguments.timeout_seconds
    )
    for row in poll_rows:
        row["mode"] = "poll"
    _, audit_metadata, audit_record = run_ring(
        ring, warmup, samples, audit=True, timeout=arguments.timeout_seconds
    )

    output_dir = arguments.output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    raw_csv = output_dir / "raw-samples.csv"
    write_raw_csv(raw_csv, poll_rows)

    report = build_report(
        contract=contract,
        contract_path=arguments.contract.resolve(),
        host=host,
        poll_rows=poll_rows,
        poll_metadata=poll_metadata,
        poll_record=poll_record,
        audit_metadata=audit_metadata,
        audit_record=audit_record,
        warmup=warmup,
        samples=samples,
        ring_executable=ring,
    )
    report_path = output_dir / "transport-profile-report.json"
    report_path.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    archive_path = output_dir / "measurement-archive.json"
    archive_command = [
        sys.executable,
        "-B",
        str(ARCHIVE_TOOL),
        "--contract",
        str(arguments.contract.resolve()),
        "--output",
        str(archive_path),
        "--note",
        f"profile_report={report_path}",
        "--note",
        f"raw_samples={raw_csv}",
        "--note",
        f"host_kernel={host['kernel_release']}",
        "--note",
        f"client_cpu={host['selected_client_cpu']}",
    ]
    archive_result = subprocess.run(archive_command, check=False, capture_output=True, text=True)
    if archive_result.returncode != 0:
        raise SystemExit(archive_result.stderr or archive_result.stdout)

    # Fold live fingerprints into the archive skeleton.
    archive = json.loads(archive_path.read_text(encoding="utf-8"))
    archive["status"] = "partial-live" if report["status"] == "measured-partial" else "live"
    archive["fingerprints"].update(
        {
            "kernel_release": host["kernel_release"],
            "machine": host["machine"],
            "python": host["python"],
            "platform": host["platform"],
            "cpu_model": host["cpu_model"],
            "topology": {
                "online_cpus": host["online_cpus"],
                "affinity": host["effective_affinity_cpus_after_pin"],
                "numa_nodes": host["numa_nodes_online"],
                "selected_client_cpu": host["selected_client_cpu"],
            },
            "compiler": None,
            "qemu": None,
        }
    )
    archive["samples"] = {
        "status": "collected-poll-memfd-ring",
        "warmup_count": warmup,
        "sample_count": samples,
        "retain_raw": True,
        "raw_samples_csv": raw_csv.name,
        "raw_samples_sha256": sha256_file(raw_csv),
        "summaries": {
            name: {
                "p50": values["p50"],
                "p90": values["p90"],
                "p99": values["p99"],
            }
            for name, values in report["modes"]["poll"]["metrics"].items()
        },
    }
    archive["profile_report"] = report_path.name
    archive["warm_path_audit"] = report["warm_path_audit"]
    archive_path.write_text(json.dumps(archive, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    if report["warm_path_audit"]["status"] != "pass":
        raise SystemExit("warm-path audit failed")

    print(
        json.dumps(
            {
                "status": report["status"],
                "report": str(report_path),
                "archive": str(archive_path),
                "raw_samples": str(raw_csv),
                "audit": report["warm_path_audit"]["status"],
                "poll_metrics": sorted(report["modes"]["poll"]["metrics"]),
            },
            sort_keys=True,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
