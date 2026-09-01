#!/usr/bin/env python3
"""Validate the canonical milestone-0.1.1.0 transport measurement contract."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any


class MeasurementError(ValueError):
    """A malformed or inconsistent measurement contract."""


def load_json(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise MeasurementError(f"cannot load {path}: {error}") from error
    if not isinstance(value, dict):
        raise MeasurementError(f"{path} must contain a JSON object")
    return value


def require_keys(value: dict[str, Any], expected: set[str], context: str) -> None:
    if set(value) != expected:
        missing = sorted(expected - set(value))
        extra = sorted(set(value) - expected)
        raise MeasurementError(
            f"{context}: fields differ (missing={missing}, extra={extra})"
        )


def require_string(value: Any, expected: str, context: str) -> None:
    if value != expected:
        raise MeasurementError(f"{context}: expected {expected!r}, got {value!r}")


def require_string_list(value: Any, expected: list[str], context: str) -> None:
    if value != expected or not all(isinstance(item, str) for item in value):
        raise MeasurementError(f"{context}: expected ordered string list {expected!r}")


def validate(document: dict[str, Any], path: Path) -> None:
    require_keys(
        document,
        {"id", "version", "scope", "clock", "queue", "affinity", "warmup", "samples", "workloads", "baselines"},
        str(path),
    )
    require_string(document["id"], "transport.measurement.v0", f"{path}:id")
    require_string(document["version"], "0.1", f"{path}:version")
    require_string(document["scope"], "milestone-0.1.1.0", f"{path}:scope")

    clock = document["clock"]
    if not isinstance(clock, dict):
        raise MeasurementError(f"{path}:clock must be an object")
    require_keys(clock, {"name", "unit", "timestamps"}, f"{path}:clock")
    require_string(clock["name"], "CLOCK_MONOTONIC_RAW", f"{path}:clock.name")
    require_string(clock["unit"], "ns", f"{path}:clock.unit")
    require_string_list(
        clock["timestamps"],
        ["submit_start", "submit_return", "completion_return"],
        f"{path}:clock.timestamps",
    )

    queue = document["queue"]
    if not isinstance(queue, dict):
        raise MeasurementError(f"{path}:queue must be an object")
    require_keys(
        queue,
        {"states", "active_modes", "producer", "consumer", "warm_path"},
        f"{path}:queue",
    )
    require_string_list(
        queue["states"],
        ["empty", "active", "polling", "armed", "sleeping", "draining", "lost"],
        f"{path}:queue.states",
    )
    require_string_list(queue["active_modes"], ["poll", "block"], f"{path}:queue.active_modes")
    require_string(queue["producer"], "single-producer-per-stream", f"{path}:queue.producer")
    require_string(queue["consumer"], "one-worker-per-lease", f"{path}:queue.consumer")
    warm_path = queue["warm_path"]
    if not isinstance(warm_path, dict):
        raise MeasurementError(f"{path}:queue.warm_path must be an object")
    require_keys(
        warm_path,
        {"syscalls", "allocations", "global_locks", "vfio_user_messages", "qemu_main_loop_writes", "per_command_interrupts"},
        f"{path}:queue.warm_path",
    )
    if any(value != 0 for value in warm_path.values()):
        raise MeasurementError(f"{path}:queue.warm_path must contain only zeroes")

    affinity = document["affinity"]
    if not isinstance(affinity, dict):
        raise MeasurementError(f"{path}:affinity must be an object")
    require_keys(affinity, {"client", "worker", "numa", "cross_numa"}, f"{path}:affinity")
    require_string(
        affinity["client"],
        "lowest-cpu-in-effective-sched-affinity-unless-explicit",
        f"{path}:affinity.client",
    )
    require_string(
        affinity["worker"],
        "lowest-effective-cpu-on-client-numa-node-without-shared-core",
        f"{path}:affinity.worker",
    )
    require_string(
        affinity["numa"], "record-effective-cpuset-and-memory-nodes", f"{path}:affinity.numa"
    )
    require_string(affinity["cross_numa"], "ineligible", f"{path}:affinity.cross_numa")

    warmup = document["warmup"]
    if not isinstance(warmup, dict):
        raise MeasurementError(f"{path}:warmup must be an object")
    require_keys(warmup, {"count", "excluded_from_summary", "must_complete"}, f"{path}:warmup")
    if warmup["count"] != 1000 or not warmup["excluded_from_summary"] or not warmup["must_complete"]:
        raise MeasurementError(f"{path}:warmup does not match the fixed contract")

    samples = document["samples"]
    if not isinstance(samples, dict):
        raise MeasurementError(f"{path}:samples must be an object")
    require_keys(samples, {"count", "unit", "positive", "retain_raw", "summaries"}, f"{path}:samples")
    if samples["count"] != 10000 or samples["unit"] != "ns":
        raise MeasurementError(f"{path}:samples count/unit does not match the fixed contract")
    if not samples["positive"] or not samples["retain_raw"]:
        raise MeasurementError(f"{path}:samples must retain positive raw samples")
    require_string_list(samples["summaries"], ["p50", "p90", "p99"], f"{path}:samples.summaries")

    workloads = document["workloads"]
    if not isinstance(workloads, list) or len(workloads) != 2:
        raise MeasurementError(f"{path}:workloads must contain exactly two entries")
    expected_workloads = [
        {"id": "local-cdev-add-copy", "transport": "cdev", "operations": ["add", "copy"], "baseline": "direct-cpu"},
        {"id": "guest-vfio-user-add-copy", "transport": "vfio-user", "operations": ["add", "copy"], "baseline": "direct-cpu"},
    ]
    for index, (workload, expected) in enumerate(zip(workloads, expected_workloads)):
        if not isinstance(workload, dict) or workload != expected:
            raise MeasurementError(f"{path}:workloads[{index}] does not match the fixed contract")

    baselines = document["baselines"]
    if not isinstance(baselines, dict) or set(baselines) != {"direct-cpu"}:
        raise MeasurementError(f"{path}:baselines must contain direct-cpu only")
    direct = baselines["direct-cpu"]
    if not isinstance(direct, dict):
        raise MeasurementError(f"{path}:baselines.direct-cpu must be an object")
    expected_direct = {
        "directions": ["h2d", "d2h", "d2d"],
        "copy_bytes": 16777216,
        "allocation": "same-pages-and-numa-policy",
        "completion_boundary": "operation-completion",
        "match_fields": [
            "cpu",
            "worker_cpu",
            "numa_node",
            "copy_bytes",
            "warmup_count",
            "sample_count",
            "clock",
        ],
    }
    if direct != expected_direct:
        raise MeasurementError(f"{path}:baselines.direct-cpu does not match the fixed contract")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument(
        "--contract",
        type=Path,
        default=Path("tests/performance/milestone-0.1.1.0-measurement.json"),
    )
    args = parser.parse_args()
    path = (args.root / args.contract).resolve() if not args.contract.is_absolute() else args.contract.resolve()
    try:
        validate(load_json(path), path)
    except (OSError, MeasurementError) as error:
        print(f"transport measurement: error: {error}", file=sys.stderr)
        return 1
    print("transport measurement: ok (milestone-0.1.1.0 fixed contract)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
