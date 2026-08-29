#!/usr/bin/env python3

from __future__ import annotations

import argparse
import csv
from datetime import datetime
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import re
import signal
import socket
import statistics
import subprocess
import tempfile
import time
from typing import Any


CORE_METRICS = (
    "monotonic_raw_pair_overhead_ns",
    "memfd_ring_submit_consume_ns",
    "cuda_warm_launch_submit_ns",
    "cuda_warm_launch_complete_ns",
    "cuda_warm_copy_submit_ns",
    "cuda_warm_copy_complete_ns",
    "cuda_direct_h2d_submit_ns",
    "cuda_direct_h2d_complete_ns",
    "cuda_direct_d2h_submit_ns",
    "cuda_direct_d2h_complete_ns",
    "nvml_hot_memory_getter_ns",
    "nvml_warm_init_ns",
)

DIRECT_COPY_METRICS = (
    "cuda_direct_h2d_submit_ns",
    "cuda_direct_h2d_complete_ns",
    "cuda_direct_d2h_submit_ns",
    "cuda_direct_d2h_complete_ns",
)

DAEMON_COPY_PATH_COUNTERS = (
    "host-address-space-registrations",
    "direct-host-source-operations",
    "direct-host-source-bytes",
    "direct-host-destination-operations",
    "direct-host-destination-bytes",
    "staged-host-source-operations",
    "staged-host-source-bytes",
    "staged-host-destination-operations",
    "staged-host-destination-bytes",
)

MINIMUM_DIRECT_COPY_BYTES = 16 * 1024 * 1024
RING_AUDIT_BEGIN_MARKER = "METAFLUX_RING_AUDIT_BEGIN_V1"
RING_AUDIT_END_MARKER = "METAFLUX_RING_AUDIT_END_V1"
NATIVE_COPY_BASELINE_SCHEMA_VERSION = 2
NATIVE_COPY_HOST_ALLOCATION = "malloc_pageable"
NATIVE_COPY_STOPPING_RULE = "exact_fixed_sample_count_no_deletion"
NATIVE_COPY_APIS = {
    "h2d": "cuMemcpyHtoDAsync",
    "d2h": "cuMemcpyDtoHAsync",
    "d2d": "cuMemcpyDtoDAsync",
}
COPY_COMPLETION_METRICS = {
    "h2d": "cuda_direct_h2d_complete_ns",
    "d2h": "cuda_direct_d2h_complete_ns",
    "d2d": "cuda_warm_copy_complete_ns",
}


def canonical_native_copy_command(
    native_reference: dict[str, Any],
    direction: str,
    warmup_count: int,
    sample_count: int,
    copy_bytes: int,
) -> list[str]:
    return [
        native_reference["benchmark"]["path"],
        "--direction",
        direction,
        "--warmup",
        str(warmup_count),
        "--samples",
        str(sample_count),
        "--copy-bytes",
        str(copy_bytes),
        "--device-bdf",
        native_reference["device"]["bdf"],
        "--cuda-library",
        native_reference["provider"]["path"],
        "--host-allocation",
        NATIVE_COPY_HOST_ALLOCATION,
    ]


def canonical_native_copy_environment(
    native_reference: dict[str, Any],
) -> dict[str, str]:
    return {
        "CUDA_DEVICE_ORDER": "PCI_BUS_ID",
        "CUDA_VISIBLE_DEVICES": native_reference["device"]["uuid"],
        "LD_LIBRARY_PATH": str(Path(native_reference["provider"]["path"]).parent),
    }


def read_text(path: Path) -> str | None:
    try:
        return path.read_text(encoding="utf-8").strip()
    except (FileNotFoundError, PermissionError, OSError, UnicodeError):
        return None


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def artifact_fingerprint(path: Path | None) -> dict[str, Any]:
    if path is None:
        return {"status": "skipped", "reason": "artifact_not_configured"}
    resolved = path.resolve()
    if not resolved.is_file():
        return {
            "status": "skipped",
            "reason": "artifact_not_found",
            "path": str(resolved),
        }
    result: dict[str, Any] = {
        "status": "measured",
        "path": str(resolved),
        "size_bytes": resolved.stat().st_size,
        "sha256": sha256_file(resolved),
    }
    if resolved.suffix == ".json":
        try:
            result["descriptor"] = json.loads(resolved.read_text(encoding="utf-8"))
        except (json.JSONDecodeError, UnicodeError, OSError) as error:
            result["descriptor_status"] = {
                "status": "skipped",
                "reason": "invalid_json",
                "detail": str(error),
            }
    return result


def artifact_is_executable(identity: dict[str, Any]) -> bool:
    path = identity.get("path")
    return (
        identity.get("status") == "measured"
        and isinstance(path, str)
        and os.access(path, os.X_OK)
    )


def elf_build_id(binary: Path, llvm_readobj: Path) -> str | None:
    try:
        completed = subprocess.run(
            [str(llvm_readobj.resolve()), "--notes", str(binary.resolve())],
            check=False,
            capture_output=True,
            text=True,
            errors="replace",
            timeout=30,
        )
    except (FileNotFoundError, PermissionError, OSError, subprocess.TimeoutExpired):
        return None
    if completed.returncode != 0:
        return None
    match = re.search(r"(?:Build ID|BuildId):\s*([0-9A-Fa-f]+)", completed.stdout)
    return match.group(1).lower() if match is not None else None


def collect_native_reference_identity(args: argparse.Namespace) -> dict[str, Any]:
    configured = {
        "native_device_bdf": args.native_device_bdf,
        "native_cuda_library": args.native_cuda_library,
        "native_copy_benchmark": args.native_copy_benchmark,
        "llvm_readobj": args.llvm_readobj,
    }
    missing_configuration = sorted(
        name for name, value in configured.items() if value is None
    )
    if missing_configuration:
        return skipped(
            "native_reference_configuration_missing",
            missing_fields=missing_configuration,
        )

    bdf = str(args.native_device_bdf).lower()
    if re.fullmatch(r"[0-9a-f]{4}:[0-9a-f]{2}:[0-9a-f]{2}\.[0-7]", bdf) is None:
        return skipped("native_device_bdf_invalid", observed=args.native_device_bdf)
    proc_information_path = Path("/proc/driver/nvidia/gpus") / bdf / "information"
    information_text = read_text(proc_information_path)
    sysfs_path = Path("/sys/bus/pci/devices") / bdf
    driver_version_path = Path("/proc/driver/nvidia/version")
    driver_version_text = read_text(driver_version_path)
    provider = artifact_fingerprint(args.native_cuda_library)
    benchmark = artifact_fingerprint(args.native_copy_benchmark)
    readobj = artifact_fingerprint(args.llvm_readobj)
    if information_text is None or driver_version_text is None or not sysfs_path.exists():
        return skipped(
            "native_nvidia_device_identity_unavailable",
            bdf=bdf,
            proc_information_path=str(proc_information_path),
            driver_version_path=str(driver_version_path),
            sysfs_path=str(sysfs_path),
        )
    if any(item.get("status") != "measured" for item in (provider, benchmark, readobj)):
        return skipped(
            "native_reference_artifact_unavailable",
            provider=provider,
            benchmark=benchmark,
            llvm_readobj=readobj,
        )
    non_executable_tools = sorted(
        name
        for name, item in (("native_copy_benchmark", benchmark), ("llvm_readobj", readobj))
        if not artifact_is_executable(item)
    )
    if non_executable_tools:
        return skipped(
            "native_reference_tool_not_executable",
            tools=non_executable_tools,
        )

    information: dict[str, str] = {}
    for line in information_text.splitlines():
        if ":" in line:
            key, value = line.split(":", 1)
            information[key.strip().lower().replace(" ", "_")] = value.strip()
    model = information.get("model")
    uuid = information.get("gpu_uuid")
    bus_location = information.get("bus_location", "").lower()
    try:
        pcie_path = str(sysfs_path.resolve(strict=True))
        numa_node = int((read_text(sysfs_path / "numa_node") or ""), 10)
    except (FileNotFoundError, OSError, ValueError):
        return skipped("native_pcie_numa_identity_unavailable", bdf=bdf)
    pcie_vendor_id = read_text(sysfs_path / "vendor")
    pcie_device_id = read_text(sysfs_path / "device")
    pcie_current_link_speed = read_text(sysfs_path / "current_link_speed")
    pcie_current_link_width = read_text(sysfs_path / "current_link_width")
    pcie_max_link_speed = read_text(sysfs_path / "max_link_speed")
    pcie_max_link_width = read_text(sysfs_path / "max_link_width")
    driver_module_version = read_text(Path("/sys/module/nvidia/version"))
    if driver_module_version is None:
        version_match = re.search(r"Kernel Module\s+([^\s]+)", driver_version_text)
        driver_module_version = version_match.group(1) if version_match is not None else None
    build_id = elf_build_id(
        Path(args.native_cuda_library), Path(args.llvm_readobj)
    )
    missing_identity = sorted(
        name
        for name, value in {
            "device_uuid": uuid,
            "device_model": model,
            "bus_location": bus_location if bus_location == bdf else None,
            "pcie_vendor_id": pcie_vendor_id,
            "pcie_device_id": pcie_device_id,
            "pcie_current_link_speed": pcie_current_link_speed,
            "pcie_current_link_width": pcie_current_link_width,
            "pcie_max_link_speed": pcie_max_link_speed,
            "pcie_max_link_width": pcie_max_link_width,
            "driver_module_version": driver_module_version,
            "provider_build_id": build_id,
        }.items()
        if not value
    )
    if numa_node < 0:
        missing_identity.append("device_numa_node")
    if missing_identity:
        return skipped(
            "native_reference_identity_incomplete",
            bdf=bdf,
            missing_fields=sorted(missing_identity),
        )
    return {
        "status": "measured",
        "device": {
            "uuid": uuid,
            "model": model,
            "bdf": bdf,
            "pcie_sysfs_path": pcie_path,
            "pcie_vendor_id": pcie_vendor_id,
            "pcie_device_id": pcie_device_id,
            "pcie_current_link_speed": pcie_current_link_speed,
            "pcie_current_link_width": pcie_current_link_width,
            "pcie_max_link_speed": pcie_max_link_speed,
            "pcie_max_link_width": pcie_max_link_width,
            "numa_node": numa_node,
        },
        "driver": {
            "module_version": driver_module_version,
            "version_text_sha256": hashlib.sha256(
                driver_version_text.encode("utf-8")
            ).hexdigest(),
        },
        "provider": {
            "path": provider["path"],
            "size_bytes": provider["size_bytes"],
            "sha256": provider["sha256"],
            "build_id": build_id,
        },
        "benchmark": {
            "path": benchmark["path"],
            "size_bytes": benchmark["size_bytes"],
            "sha256": benchmark["sha256"],
        },
        "host_kernel": {
            "release": platform.release(),
            "version": platform.version(),
        },
        "llvm_readobj": readobj,
    }


def milestone_budget_status(path: Path | None) -> dict[str, Any]:
    if path is None:
        return skipped("milestone_plan_not_configured")
    resolved = path.resolve()
    text = read_text(resolved)
    if text is None:
        return skipped("milestone_plan_unavailable", path=str(resolved))
    lines = text.splitlines()
    if not lines or lines[0].strip() != "---":
        return skipped("milestone_plan_front_matter_missing", path=str(resolved))
    for line in lines[1:]:
        if line.strip() == "---":
            break
        if line.startswith("budgets:"):
            value = line.split(":", 1)[1].strip()
            if value in {"provisional", "binding"}:
                return {
                    "status": "measured",
                    "path": str(resolved),
                    "sha256": sha256_file(resolved),
                    "budget_status": value,
                }
            return skipped(
                "milestone_budget_status_invalid", path=str(resolved), observed=value
            )
    return skipped("milestone_budget_status_missing", path=str(resolved))


def parse_cpuinfo(cpu: int) -> dict[str, Any]:
    text = read_text(Path("/proc/cpuinfo")) or ""
    records = [record for record in text.split("\n\n") if record.strip()]
    selected: dict[str, str] = {}
    for record in records:
        fields: dict[str, str] = {}
        for line in record.splitlines():
            if ":" in line:
                key, value = line.split(":", 1)
                fields[key.strip()] = value.strip()
        if fields.get("processor") == str(cpu):
            selected = fields
            break
    keys = (
        "vendor_id",
        "model name",
        "cpu family",
        "model",
        "stepping",
        "microcode",
        "flags",
    )
    return {key.replace(" ", "_"): selected.get(key) for key in keys}


def parse_cpu_list(text: str | None) -> set[int]:
    result: set[int] = set()
    if not text:
        return result
    for field in text.split(","):
        bounds = field.strip().split("-", 1)
        try:
            start = int(bounds[0])
            end = int(bounds[1]) if len(bounds) == 2 else start
        except ValueError:
            return set()
        if start < 0 or end < start:
            return set()
        result.update(range(start, end + 1))
    return result


def cpu_numa_node(cpu: int) -> int | None:
    cpu_path = Path(f"/sys/devices/system/cpu/cpu{cpu}")
    nodes = sorted(cpu_path.glob("node[0-9]*"))
    if not nodes:
        return None
    try:
        return int(nodes[0].name.removeprefix("node"))
    except ValueError:
        return None


def choose_worker_cpu(effective_cpus: list[int], client_cpu: int) -> int:
    client_siblings = parse_cpu_list(
        read_text(Path(f"/sys/devices/system/cpu/cpu{client_cpu}/topology/thread_siblings_list"))
    )
    client_node = cpu_numa_node(client_cpu)
    same_node = [
        cpu
        for cpu in effective_cpus
        if cpu not in client_siblings and cpu_numa_node(cpu) == client_node
    ]
    if same_node:
        return same_node[0]
    separate_core = [cpu for cpu in effective_cpus if cpu not in client_siblings]
    if separate_core:
        return separate_core[0]
    return client_cpu


def proc_status_fields() -> dict[str, str]:
    result: dict[str, str] = {}
    text = read_text(Path("/proc/self/status")) or ""
    for line in text.splitlines():
        if ":" in line:
            key, value = line.split(":", 1)
            result[key.strip()] = value.strip()
    return result


def collect_cpuset() -> dict[str, Any]:
    result: dict[str, Any] = {
        "proc_self_cgroup": (read_text(Path("/proc/self/cgroup")) or "").splitlines()
    }
    cgroup_lines = result["proc_self_cgroup"]
    unified_path: str | None = None
    legacy_path: str | None = None
    for line in cgroup_lines:
        parts = line.split(":", 2)
        if len(parts) != 3:
            continue
        if parts[0] == "0" and parts[1] == "":
            unified_path = parts[2]
        elif "cpuset" in parts[1].split(","):
            legacy_path = parts[2]
    candidates: list[Path] = []
    if unified_path is not None:
        candidates.append(Path("/sys/fs/cgroup") / unified_path.lstrip("/"))
    if legacy_path is not None:
        candidates.append(Path("/sys/fs/cgroup/cpuset") / legacy_path.lstrip("/"))
    for root in candidates:
        cpu_text = read_text(root / "cpuset.cpus.effective") or read_text(root / "cpuset.cpus")
        mem_text = read_text(root / "cpuset.mems.effective") or read_text(root / "cpuset.mems")
        if cpu_text is not None or mem_text is not None:
            result["mount_path"] = str(root)
            result["effective_cpus"] = cpu_text
            result["effective_mems"] = mem_text
            break
    status = proc_status_fields()
    result["status_cpus_allowed_list"] = status.get("Cpus_allowed_list")
    result["status_mems_allowed_list"] = status.get("Mems_allowed_list")
    return result


def collect_numa_policy() -> dict[str, Any]:
    text = read_text(Path("/proc/self/numa_maps"))
    if text is None:
        return {"status": "skipped", "reason": "proc_numa_maps_unavailable"}
    policies: set[str] = set()
    for line in text.splitlines():
        fields = line.split()
        if len(fields) >= 2:
            policies.add(fields[1])
    return {
        "status": "measured",
        "mapping_policies": sorted(policies),
        "interpretation": "per-mapping policy snapshot; allocations use selected-CPU first touch",
    }


def collect_frequency(cpu: int) -> dict[str, Any]:
    root = Path(f"/sys/devices/system/cpu/cpu{cpu}/cpufreq")
    fields = (
        "scaling_driver",
        "scaling_governor",
        "energy_performance_preference",
        "scaling_min_freq",
        "scaling_max_freq",
        "cpuinfo_min_freq",
        "cpuinfo_max_freq",
    )
    values = {field: read_text(root / field) for field in fields}
    if all(value is None for value in values.values()):
        return {"status": "skipped", "reason": "cpufreq_sysfs_unavailable"}
    return {"status": "measured", **values}


def collect_process_thread_placement(process_id: int) -> dict[str, Any]:
    rows: list[dict[str, Any]] = []
    for task_path in sorted(Path(f"/proc/{process_id}/task").glob("[0-9]*")):
        fields: dict[str, str] = {}
        text = read_text(task_path / "status") or ""
        for line in text.splitlines():
            if ":" in line:
                key, value = line.split(":", 1)
                fields[key.strip()] = value.strip()
        rows.append(
            {
                "tid": int(task_path.name),
                "name": fields.get("Name"),
                "cpus_allowed_list": fields.get("Cpus_allowed_list"),
                "mems_allowed_list": fields.get("Mems_allowed_list"),
            }
        )
    if not rows:
        return {"status": "skipped", "reason": "process_task_status_unavailable"}
    return {"status": "measured", "threads": rows}


def git_fingerprint(repository: Path) -> dict[str, Any]:
    try:
        revision = subprocess.run(
            ["git", "rev-parse", "HEAD"],
            cwd=repository,
            check=True,
            capture_output=True,
            text=True,
            timeout=5,
        ).stdout.strip()
        dirty = subprocess.run(
            ["git", "status", "--porcelain"],
            cwd=repository,
            check=True,
            capture_output=True,
            text=True,
            timeout=5,
        ).stdout
        return {"status": "measured", "revision": revision, "dirty": bool(dirty.strip())}
    except (FileNotFoundError, subprocess.SubprocessError, OSError) as error:
        return {"status": "skipped", "reason": "git_identity_unavailable", "detail": str(error)}


def collect_fingerprint(
    args: argparse.Namespace, original_affinity: list[int], client_cpu: int, worker_cpu: int
) -> dict[str, Any]:
    cpu_root = Path(f"/sys/devices/system/cpu/cpu{client_cpu}/topology")
    worker_root = Path(f"/sys/devices/system/cpu/cpu{worker_cpu}/topology")
    timezone_file = read_text(Path("/etc/timezone"))
    timezone_now = datetime.now().astimezone()
    binaries = {
        "harness_driver": artifact_fingerprint(Path(__file__)),
        "ring_benchmark": artifact_fingerprint(args.ring_benchmark),
        "cuda_benchmark": artifact_fingerprint(args.cuda_benchmark),
        "nvml_benchmark": artifact_fingerprint(args.nvml_benchmark),
        "daemon": artifact_fingerprint(args.daemon),
        "cuda_provider": artifact_fingerprint(args.cuda_provider),
        "nvml_provider": artifact_fingerprint(args.nvml_provider),
    }
    canonical_budget = milestone_budget_status(args.milestone_plan)
    native_reference = collect_native_reference_identity(args)
    return {
        "schema_version": 2,
        "captured_at": datetime.now().astimezone().isoformat(),
        "run_contract": {
            "mode": args.mode,
            "declared_budget_status": args.budget_status,
            "binding_pgo_mode": args.binding_pgo_mode,
            "binding_pgo_profile_sha256": args.binding_pgo_profile_sha256,
            "canonical_milestone_budget": canonical_budget,
            "reference_host_role": args.reference_host_role,
            "controlled_host": args.controlled_host,
            "clock": "CLOCK_MONOTONIC_RAW in benchmark executables",
            "warmup_count": args.warmup,
            "sample_count": args.samples,
            "stopping_rule": "exact fixed sample count; no sample or outlier deletion",
            "copy_bytes": args.copy_bytes,
            "execution_mode": args.execution_mode,
            "provider_mode": "managed",
            "affinity_policy": "client and daemon pinned to recorded logical CPUs",
            "native_copy_baselines": {
                "h2d": artifact_fingerprint(args.native_h2d_baseline_json),
                "d2h": artifact_fingerprint(args.native_d2h_baseline_json),
                "d2d": artifact_fingerprint(args.native_d2d_baseline_json),
            },
            "ring_audit_strace": artifact_fingerprint(args.strace),
        },
        "runtime_environment": {
            "provider_dir": str(args.provider_dir.resolve())
            if args.provider_dir is not None
            else None,
            "inherited_ld_library_path": os.environ.get("LD_LIBRARY_PATH"),
            "asan_options": os.environ.get("ASAN_OPTIONS"),
            "ubsan_options": os.environ.get("UBSAN_OPTIONS"),
        },
        "host": {
            "system": platform.system(),
            "machine": platform.machine(),
            "kernel_release": platform.release(),
            "kernel_version": platform.version(),
            "libc": list(platform.libc_ver()),
            "python": platform.python_version(),
            "timezone": timezone_file or str(timezone_now.tzinfo),
            "utc_offset_seconds": int((timezone_now.utcoffset() or timezone_now - timezone_now).total_seconds()),
            "online_cpus": read_text(Path("/sys/devices/system/cpu/online")),
            "online_numa_nodes": read_text(Path("/sys/devices/system/node/online")),
        },
        "cpu": {
            "selected_cpu": client_cpu,
            "client_cpu": client_cpu,
            "worker_cpu": worker_cpu,
            "original_sched_affinity": original_affinity,
            "pinned_sched_affinity": sorted(os.sched_getaffinity(0)),
            "numa_node": cpu_numa_node(client_cpu),
            "worker_numa_node": cpu_numa_node(worker_cpu),
            "thread_siblings": read_text(cpu_root / "thread_siblings_list"),
            "worker_thread_siblings": read_text(worker_root / "thread_siblings_list"),
            "core_siblings": read_text(cpu_root / "core_siblings_list"),
            "core_id": read_text(cpu_root / "core_id"),
            "physical_package_id": read_text(cpu_root / "physical_package_id"),
            "identity": parse_cpuinfo(client_cpu),
            "worker_identity": parse_cpuinfo(worker_cpu),
            "frequency": collect_frequency(client_cpu),
            "worker_frequency": collect_frequency(worker_cpu),
            "xcr0_os_state": {
                "status": "skipped",
                "reason": "xcr0_probe_not_part_of_measurement_harness",
            },
        },
        "placement": {
            "cpuset": collect_cpuset(),
            "numa_policy": collect_numa_policy(),
            "service_or_container_limits": {
                "cgroup": (read_text(Path("/proc/self/cgroup")) or "").splitlines(),
                "scheduler_policy": os.sched_getscheduler(0),
            },
        },
        "toolchain": {
            "compiler_epoch": artifact_fingerprint(args.compiler_epoch),
            "build_manifest": artifact_fingerprint(args.build_manifest),
            "binaries": binaries,
        },
        "native_reference": native_reference,
        "source": git_fingerprint(args.repository.resolve()),
    }


def wait_for_socket(path: Path, daemon: subprocess.Popen[str], timeout: float) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if daemon.poll() is not None:
            stdout, stderr = daemon.communicate()
            raise RuntimeError(
                f"metafluxd exited before publishing its socket ({daemon.returncode}); "
                f"stdout={stdout!r} stderr={stderr!r}"
            )
        try:
            with socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET) as probe:
                probe.settimeout(0.1)
                probe.connect(str(path))
            return
        except (FileNotFoundError, ConnectionRefusedError, TimeoutError, OSError):
            time.sleep(0.01)
    raise TimeoutError(f"metafluxd did not publish {path} within {timeout:.1f}s")


def stop_daemon(daemon: subprocess.Popen[str]) -> tuple[str, str]:
    if daemon.poll() is None:
        daemon.send_signal(signal.SIGTERM)
    try:
        return daemon.communicate(timeout=5)
    except subprocess.TimeoutExpired:
        daemon.kill()
        return daemon.communicate(timeout=5)


def parse_daemon_copy_path_counters(stderr: str) -> dict[str, Any]:
    required = set(DAEMON_COPY_PATH_COUNTERS)
    candidates: list[tuple[str, list[tuple[str, str]]]] = []
    for line in stderr.splitlines():
        pairs = re.findall(r"(?:^|\s)([a-z][a-z0-9-]*)=([^\s]+)", line)
        relevant = [(name, value) for name, value in pairs if name in required]
        if relevant:
            candidates.append((line, relevant))
    if len(candidates) != 1:
        return {
            "status": "invalid",
            "reason": "daemon_copy_path_counter_line_count",
            "observed_line_count": len(candidates),
            "expected_line_count": 1,
        }

    line, pairs = candidates[0]
    names = [name for name, _ in pairs]
    if len(names) != len(set(names)):
        return {
            "status": "invalid",
            "reason": "daemon_copy_path_counter_duplicate",
            "duplicate_fields": sorted(name for name in set(names) if names.count(name) > 1),
            "line_sha256": hashlib.sha256(line.encode("utf-8")).hexdigest(),
        }
    missing = sorted(required - set(names))
    if missing:
        return {
            "status": "invalid",
            "reason": "daemon_copy_path_counter_missing",
            "missing_fields": missing,
            "line_sha256": hashlib.sha256(line.encode("utf-8")).hexdigest(),
        }
    malformed = sorted(name for name, value in pairs if re.fullmatch(r"[0-9]+", value) is None)
    if malformed:
        return {
            "status": "invalid",
            "reason": "daemon_copy_path_counter_non_integer",
            "malformed_fields": malformed,
            "line_sha256": hashlib.sha256(line.encode("utf-8")).hexdigest(),
        }
    return {
        "status": "measured",
        "values": {name: int(value) for name, value in pairs},
        "line_sha256": hashlib.sha256(line.encode("utf-8")).hexdigest(),
    }


def parse_benchmark_output(
    source: str, stdout: str
) -> tuple[list[dict[str, Any]], dict[str, str], list[str]]:
    rows: list[dict[str, Any]] = []
    metadata: dict[str, str] = {}
    unparsed: list[str] = []
    for line in stdout.splitlines():
        if line.startswith("METAFLUX_SAMPLE\t"):
            fields = line.split("\t")
            if len(fields) != 5:
                raise RuntimeError(f"{source}: malformed sample row: {line!r}")
            try:
                sample_index = int(fields[2])
                value = int(fields[3])
            except ValueError as error:
                raise RuntimeError(f"{source}: non-integer sample row: {line!r}") from error
            if sample_index < 0 or value < 0:
                raise RuntimeError(f"{source}: negative sample row: {line!r}")
            rows.append(
                {
                    "source": source,
                    "metric": fields[1],
                    "sample_index": sample_index,
                    "value": value,
                    "unit": fields[4],
                }
            )
        elif line.startswith("METAFLUX_METADATA\t"):
            fields = line.split("\t", 2)
            if len(fields) != 3 or fields[1] in metadata:
                raise RuntimeError(f"{source}: malformed or duplicate metadata row: {line!r}")
            metadata[fields[1]] = fields[2]
        elif line.strip():
            unparsed.append(line)
    return rows, metadata, unparsed


def run_benchmark(
    source: str,
    executable: Path,
    arguments: list[str],
    environment: dict[str, str],
    timeout: float,
) -> tuple[list[dict[str, Any]], dict[str, Any]]:
    resolved = executable.resolve()
    if not resolved.is_file():
        raise RuntimeError(f"{source}: benchmark executable not found: {resolved}")
    start_ns = time.monotonic_ns()
    process = subprocess.run(
        [str(resolved), *arguments],
        env=environment,
        capture_output=True,
        text=True,
        timeout=timeout,
        check=False,
    )
    elapsed_ns = time.monotonic_ns() - start_ns
    rows, metadata, unparsed = parse_benchmark_output(source, process.stdout)
    record = {
        "status": "measured" if process.returncode == 0 else "failed",
        "executable": str(resolved),
        "arguments": arguments,
        "returncode": process.returncode,
        "wall_time_ns": elapsed_ns,
        "metadata": metadata,
        "unparsed_stdout": unparsed,
        "stderr": process.stderr.splitlines(),
    }
    if process.returncode != 0:
        raise RuntimeError(
            f"{source}: benchmark exited {process.returncode}; "
            f"stdout={process.stdout!r} stderr={process.stderr!r}"
        )
    if not rows:
        raise RuntimeError(f"{source}: benchmark emitted no samples")
    return rows, record


def parse_ring_audit_trace(path: Path) -> dict[str, Any]:
    try:
        lines = path.read_text(encoding="utf-8").splitlines()
    except (FileNotFoundError, PermissionError, OSError, UnicodeError) as error:
        return {
            "status": "invalid",
            "reason": "ring_audit_trace_unavailable",
            "detail": str(error),
        }
    begin = [index for index, line in enumerate(lines) if RING_AUDIT_BEGIN_MARKER in line]
    end = [index for index, line in enumerate(lines) if RING_AUDIT_END_MARKER in line]
    if len(begin) != 1 or len(end) != 1 or begin[0] >= end[0]:
        return {
            "status": "invalid",
            "reason": "ring_audit_trace_marker_mismatch",
            "begin_marker_count": len(begin),
            "end_marker_count": len(end),
        }
    syscalls: list[str] = []
    unparsed: list[str] = []
    pattern = re.compile(
        r"^(?:(?:\[pid\s+\d+\]|\d+)\s+)?([A-Za-z_][A-Za-z0-9_]*)\("
    )
    for line in lines[begin[0] + 1 : end[0]]:
        if not line.strip():
            continue
        match = pattern.match(line.strip())
        if match is None:
            unparsed.append(line)
        else:
            syscalls.append(match.group(1))
    counts: dict[str, int] = {}
    for syscall in syscalls:
        counts[syscall] = counts.get(syscall, 0) + 1
    if unparsed:
        return {
            "status": "invalid",
            "reason": "ring_audit_trace_unparsed_window_line",
            "unparsed_lines": unparsed,
        }
    return {
        "status": "measured",
        "line_count": len(lines),
        "window_line_count": end[0] - begin[0] - 1,
        "syscall_count": len(syscalls),
        "syscall_counts": counts,
        "wake_syscall_count": sum(
            count for name, count in counts.items() if name in {"futex", "futex_waitv"}
        ),
    }


def ring_audit_metadata(metadata: dict[str, str], args: argparse.Namespace) -> dict[str, Any]:
    required_text = {"workload": "active_memfd_ring_audit"}
    required_integers = (
        "warmup_count",
        "sample_count",
        "audit_dispatches",
        "audit_heap_allocation_attempts",
        "audit_global_lock_acquisitions",
        "audit_consumer_doorbells",
        "audit_producer_doorbells",
    )
    mismatches = {
        key: {"expected": expected, "observed": metadata.get(key)}
        for key, expected in required_text.items()
        if metadata.get(key) != expected
    }
    values: dict[str, int] = {}
    for key in required_integers:
        value = metadata.get(key)
        if value is None or re.fullmatch(r"[0-9]+", value) is None:
            mismatches[key] = {"expected": "nonnegative integer", "observed": value}
        else:
            values[key] = int(value)
    expected_counts = {
        "warmup_count": args.warmup,
        "sample_count": args.samples,
        "audit_dispatches": args.samples,
    }
    for key, expected in expected_counts.items():
        if values.get(key) != expected:
            mismatches[key] = {"expected": expected, "observed": values.get(key)}
    if mismatches:
        return {
            "status": "invalid",
            "reason": "ring_audit_metadata_mismatch",
            "mismatches": mismatches,
        }
    return {"status": "measured", "values": values}


def run_ring_audit(
    args: argparse.Namespace,
    executable: Path,
    environment: dict[str, str],
) -> dict[str, Any]:
    if args.strace is None:
        return skipped("strace_not_configured")
    strace_path = args.strace.resolve()
    benchmark_path = executable.resolve()
    strace_identity = artifact_fingerprint(strace_path)
    if not artifact_is_executable(strace_identity):
        return {
            "status": "invalid",
            "reason": "strace_not_executable",
            "strace": strace_identity,
        }
    trace_path = args.output_dir / "ring-audit.strace"
    command = [
        str(strace_path),
        "-f",
        "-qq",
        "-s",
        "256",
        "-o",
        str(trace_path),
        "-e",
        "trace=all",
        "--",
        str(benchmark_path),
        str(args.warmup),
        str(args.samples),
        "--audit",
    ]
    audit_environment = environment.copy()
    inherited_lsan_options = audit_environment.get("LSAN_OPTIONS")
    audit_environment["LSAN_OPTIONS"] = (
        f"{inherited_lsan_options}:detect_leaks=0"
        if inherited_lsan_options
        else "detect_leaks=0"
    )
    start_ns = time.monotonic_ns()
    try:
        process = subprocess.run(
            command,
            env=audit_environment,
            capture_output=True,
            text=True,
            errors="replace",
            timeout=args.timeout,
            check=False,
        )
    except (OSError, subprocess.TimeoutExpired) as error:
        return {
            "status": "failed",
            "reason": "ring_audit_execution_failed",
            "detail": str(error),
            "command": command,
            "strace": strace_identity,
        }
    elapsed_ns = time.monotonic_ns() - start_ns
    rows, metadata, unparsed = parse_benchmark_output("ring_audit", process.stdout)
    trace = parse_ring_audit_trace(trace_path)
    parsed_metadata = ring_audit_metadata(metadata, args)
    status = (
        "measured"
        if process.returncode == 0
        and not rows
        and not unparsed
        and trace.get("status") == "measured"
        and parsed_metadata.get("status") == "measured"
        else "invalid"
    )
    return {
        "status": status,
        "returncode": process.returncode,
        "wall_time_ns": elapsed_ns,
        "command": command,
        "environment": {
            "LSAN_OPTIONS": audit_environment["LSAN_OPTIONS"],
            "rationale": "LeakSanitizer exit scanning is incompatible with ptrace; active-window allocation counters and ASan/UBSan remain enabled",
        },
        "effective_affinity": sorted(os.sched_getaffinity(0)),
        "strace": strace_identity,
        "benchmark": artifact_fingerprint(benchmark_path),
        "raw_trace": {
            "path": trace_path.name,
            "sha256": sha256_file(trace_path) if trace_path.is_file() else None,
        },
        "trace": trace,
        "metadata": parsed_metadata,
        "unexpected_sample_count": len(rows),
        "unparsed_stdout": unparsed,
        "stdout_sha256": hashlib.sha256(process.stdout.encode("utf-8")).hexdigest(),
        "stderr_sha256": hashlib.sha256(process.stderr.encode("utf-8")).hexdigest(),
        "stderr": process.stderr.splitlines(),
    }


def percentile(sorted_values: list[int], quantile: float) -> int:
    rank = max(1, math.ceil(quantile * len(sorted_values)))
    return sorted_values[rank - 1]


def summarize_metrics(rows: list[dict[str, Any]], sample_count: int) -> dict[str, Any]:
    grouped: dict[str, list[dict[str, Any]]] = {}
    for row in rows:
        grouped.setdefault(row["metric"], []).append(row)
    summaries: dict[str, Any] = {}
    for metric, metric_rows in sorted(grouped.items()):
        units = {row["unit"] for row in metric_rows}
        indexes = sorted(row["sample_index"] for row in metric_rows)
        if len(units) != 1:
            raise RuntimeError(f"{metric}: mixed units: {sorted(units)}")
        if indexes != list(range(len(metric_rows))):
            raise RuntimeError(f"{metric}: sample indexes are not contiguous from zero")
        if len(metric_rows) != sample_count:
            raise RuntimeError(
                f"{metric}: observed {len(metric_rows)} samples, expected {sample_count}"
            )
        values = sorted(row["value"] for row in metric_rows)
        summaries[metric] = {
            "status": "measured",
            "unit": next(iter(units)),
            "sample_count": len(values),
            "min": values[0],
            "max": values[-1],
            "mean": statistics.fmean(values),
            "p50": percentile(values, 0.50),
            "p90": percentile(values, 0.90),
            "p99": percentile(values, 0.99),
            "raw_samples": "raw-samples.csv",
        }
    return summaries


def skipped(reason: str, **detail: Any) -> dict[str, Any]:
    return {"status": "skipped", "reason": reason, **detail}


def check_limit(
    metrics: dict[str, Any], metric: str, percentile_name: str, limit: int, eligible: bool,
    ineligible_reason: str,
) -> dict[str, Any]:
    measured = metrics.get(metric)
    if measured is None or measured.get("status") != "measured":
        return skipped("metric_not_measured", metric=metric)
    observed = measured[percentile_name]
    if not eligible:
        return {
            "status": "not_evaluated",
            "reason": ineligible_reason,
            "metric": metric,
            "percentile": percentile_name,
            "observed": observed,
            "limit": limit,
            "unit": measured["unit"],
        }
    return {
        "status": "pass" if observed <= limit else "fail",
        "metric": metric,
        "percentile": percentile_name,
        "observed": observed,
        "limit": limit,
        "unit": measured["unit"],
    }


def benchmark_metadata(processes: dict[str, Any], source: str, key: str) -> int | None:
    try:
        return int(processes[source]["metadata"][key])
    except (KeyError, TypeError, ValueError):
        return None


def active_ring_audit_check(processes: dict[str, Any]) -> dict[str, Any]:
    audit = processes.get("ring_audit")
    if not isinstance(audit, dict):
        return skipped("ring_audit_not_recorded")
    if audit.get("status") == "skipped":
        return audit
    if audit.get("status") != "measured":
        return {
            "status": "fail",
            "reason": "ring_audit_not_measured",
            "audit": audit,
        }
    trace = audit.get("trace", {})
    metadata = audit.get("metadata", {}).get("values", {})
    dispatches = metadata.get("audit_dispatches")
    syscall_count = trace.get("syscall_count")
    wake_syscall_count = trace.get("wake_syscall_count")
    requirements = {
        "dispatches_measured": isinstance(dispatches, int) and dispatches > 0,
        "no_syscalls_in_active_window": syscall_count == 0,
        "at_most_one_wake_syscall_per_dispatch": isinstance(dispatches, int)
        and dispatches > 0
        and isinstance(wake_syscall_count, int)
        and wake_syscall_count <= dispatches,
        "no_heap_allocation_attempts": metadata.get("audit_heap_allocation_attempts") == 0,
        "no_global_lock_acquisitions": metadata.get("audit_global_lock_acquisitions") == 0,
        "no_consumer_doorbells": metadata.get("audit_consumer_doorbells") == 0,
        "no_producer_doorbells": metadata.get("audit_producer_doorbells") == 0,
        "single_cpu_affinity": len(audit.get("effective_affinity", [])) == 1,
    }
    return {
        "status": "pass" if all(requirements.values()) else "fail",
        "requirements": requirements,
        "dispatches": dispatches,
        "syscall_count": syscall_count,
        "wake_syscall_count": wake_syscall_count,
        "syscall_counts": trace.get("syscall_counts"),
        "heap_allocation_attempts": metadata.get("audit_heap_allocation_attempts"),
        "global_lock_acquisitions": metadata.get("audit_global_lock_acquisitions"),
        "consumer_doorbells": metadata.get("audit_consumer_doorbells"),
        "producer_doorbells": metadata.get("audit_producer_doorbells"),
        "evidence": {
            "benchmark": audit.get("benchmark"),
            "strace": audit.get("strace"),
            "raw_trace": audit.get("raw_trace"),
            "stdout_sha256": audit.get("stdout_sha256"),
        },
    }


def direct_host_copy_path_check(
    metrics: dict[str, Any], processes: dict[str, Any]
) -> dict[str, Any]:
    cuda = processes.get("cuda")
    if not isinstance(cuda, dict) or cuda.get("status") != "measured":
        if isinstance(cuda, dict) and cuda.get("reason") == "cuda_benchmark_not_configured":
            return skipped("cuda_benchmark_not_configured")
        return {
            "status": "fail",
            "reason": "cuda_benchmark_not_measured",
            "cuda_process": cuda,
        }

    metadata = cuda.get("metadata") if isinstance(cuda.get("metadata"), dict) else {}
    direct_copy_bytes = benchmark_metadata(processes, "cuda", "direct_copy_bytes")
    correctness = benchmark_metadata(processes, "cuda", "direct_copy_correctness")
    warmup_count = benchmark_metadata(processes, "cuda", "warmup_count")
    sample_count = benchmark_metadata(processes, "cuda", "sample_count")
    workload_counts_valid = (
        warmup_count is not None
        and warmup_count > 0
        and sample_count is not None
        and sample_count > 0
    )
    # Each direction has one full-size setup/correctness transfer in addition
    # to one transfer per warm-up and measured sample.
    required_large_copy_operations = (
        warmup_count + sample_count + 1 if workload_counts_valid else None
    )
    required_direction_bytes = (
        direct_copy_bytes * required_large_copy_operations
        if direct_copy_bytes is not None
        and direct_copy_bytes > 0
        and required_large_copy_operations is not None
        else None
    )
    daemon = processes.get("daemon")
    counter_evidence = (
        daemon.get("copy_path_counters") if isinstance(daemon, dict) else None
    )
    counters = (
        counter_evidence.get("values")
        if isinstance(counter_evidence, dict) and counter_evidence.get("status") == "measured"
        else {}
    )
    metric_evidence = {
        name: {
            "status": metrics.get(name, {}).get("status"),
            "unit": metrics.get(name, {}).get("unit"),
            "sample_count": metrics.get(name, {}).get("sample_count"),
        }
        for name in DIRECT_COPY_METRICS
    }
    requirements = {
        "metrics_measured_in_ns": all(
            evidence["status"] == "measured" and evidence["unit"] == "ns"
            for evidence in metric_evidence.values()
        ),
        "metric_sample_counts_match_workload": sample_count is not None
        and all(
            evidence["sample_count"] == sample_count
            for evidence in metric_evidence.values()
        ),
        "copy_size_at_least_16_mib": direct_copy_bytes is not None
        and direct_copy_bytes >= MINIMUM_DIRECT_COPY_BYTES,
        "workload_counts_measured": workload_counts_valid,
        "benchmark_correctness": correctness == 1,
        "h2d_submit_boundary": metadata.get("direct_h2d_submit_boundary")
        == "cuMemcpyHtoDAsync_api_return",
        "d2h_submit_boundary": metadata.get("direct_d2h_submit_boundary")
        == "cuMemcpyDtoHAsync_api_return",
        "daemon_counters_measured": isinstance(counter_evidence, dict)
        and counter_evidence.get("status") == "measured",
        "host_address_space_registered": counters.get("host-address-space-registrations", 0) >= 1,
        "direct_host_source_operations_cover_workload": required_large_copy_operations
        is not None
        and counters.get("direct-host-source-operations", -1)
        >= required_large_copy_operations,
        "direct_host_source_bytes_cover_workload": required_direction_bytes is not None
        and counters.get("direct-host-source-bytes", -1) >= required_direction_bytes,
        "direct_host_destination_operations_cover_workload": required_large_copy_operations
        is not None
        and counters.get("direct-host-destination-operations", -1)
        >= required_large_copy_operations,
        "direct_host_destination_bytes_cover_workload": required_direction_bytes is not None
        and counters.get("direct-host-destination-bytes", -1) >= required_direction_bytes,
        "no_staged_host_source": counters.get("staged-host-source-operations", -1) == 0
        and counters.get("staged-host-source-bytes", -1) == 0,
        "no_staged_host_destination": counters.get("staged-host-destination-operations", -1) == 0
        and counters.get("staged-host-destination-bytes", -1) == 0,
    }
    return {
        "status": "pass" if all(requirements.values()) else "fail",
        "minimum_direct_copy_bytes": MINIMUM_DIRECT_COPY_BYTES,
        "direct_copy_bytes": direct_copy_bytes,
        "direct_copy_correctness": correctness,
        "warmup_count": warmup_count,
        "sample_count": sample_count,
        "required_large_copy_operations_per_direction": required_large_copy_operations,
        "required_direct_bytes_per_direction": required_direction_bytes,
        "metric_evidence": metric_evidence,
        "daemon_counter_evidence": counter_evidence,
        "requirements": requirements,
    }


def load_native_copy_baseline(
    path: Path | None,
    fingerprint: dict[str, Any],
    copy_bytes: int,
    direction: str,
    warmup_count: int,
    sample_count: int,
) -> dict[str, Any]:
    if direction not in NATIVE_COPY_APIS:
        raise ValueError(f"unsupported native Copy direction: {direction}")
    if path is None:
        return skipped(
            "same_path_native_baseline_not_configured", direction=direction
        )
    resolved = path.resolve()
    try:
        descriptor = json.loads(resolved.read_text(encoding="utf-8"))
    except (FileNotFoundError, PermissionError, OSError, UnicodeError, json.JSONDecodeError) as error:
        return skipped(
            "native_baseline_invalid_json",
            path=str(resolved),
            direction=direction,
            detail=str(error),
        )
    native_reference = fingerprint.get("native_reference")
    if (
        not isinstance(native_reference, dict)
        or native_reference.get("status") != "measured"
    ):
        return skipped(
            "native_reference_identity_unavailable",
            path=str(resolved),
            direction=direction,
            native_reference=native_reference,
        )
    required = {
        "schema_version": NATIVE_COPY_BASELINE_SCHEMA_VERSION,
        "workload": "same_path_native_copy",
        "direction": direction,
        "api": NATIVE_COPY_APIS[direction],
        "completion_boundary": "cuStreamSynchronize_return",
        "provider_mode": "native",
        "host_allocation": NATIVE_COPY_HOST_ALLOCATION,
        "clock": "CLOCK_MONOTONIC_RAW",
        "copy_bytes": copy_bytes,
        "warmup_count": warmup_count,
        "sample_count": sample_count,
        "stopping_rule": NATIVE_COPY_STOPPING_RULE,
        "selected_cpu": fingerprint["cpu"]["selected_cpu"],
        "worker_cpu": fingerprint["cpu"]["worker_cpu"],
        "numa_node": fingerprint["cpu"]["numa_node"],
        "cpu_vendor": fingerprint["cpu"]["identity"].get("vendor_id"),
        "cpu_model": fingerprint["cpu"]["identity"].get("model_name"),
        "microcode": fingerprint["cpu"]["identity"].get("microcode"),
    }
    mismatches = {
        key: {"expected": expected, "observed": descriptor.get(key)}
        for key, expected in required.items()
        if descriptor.get(key) != expected
    }
    for identity_name in ("device", "driver", "provider", "host_kernel"):
        expected_identity = native_reference[identity_name]
        if descriptor.get(identity_name) != expected_identity:
            mismatches[identity_name] = {
                "expected": expected_identity,
                "observed": descriptor.get(identity_name),
            }
    expected_benchmark = native_reference["benchmark"]
    observed_benchmark = descriptor.get("benchmark")
    benchmark_command = (
        observed_benchmark.get("command")
        if isinstance(observed_benchmark, dict)
        else None
    )
    observed_benchmark_identity = (
        {key: observed_benchmark.get(key) for key in expected_benchmark}
        if isinstance(observed_benchmark, dict)
        else None
    )
    if observed_benchmark_identity != expected_benchmark:
        mismatches["benchmark_identity"] = {
            "expected": expected_benchmark,
            "observed": observed_benchmark_identity,
        }
    expected_command = canonical_native_copy_command(
        native_reference,
        direction,
        warmup_count,
        sample_count,
        copy_bytes,
    )
    if benchmark_command != expected_command:
        mismatches["benchmark_command"] = {
            "expected": expected_command,
            "observed": benchmark_command,
        }
    expected_environment = canonical_native_copy_environment(native_reference)
    observed_environment = descriptor.get("environment")
    if observed_environment != expected_environment:
        mismatches["environment"] = {
            "expected": expected_environment,
            "observed": observed_environment,
        }
    if mismatches:
        return skipped(
            "native_baseline_environment_mismatch",
            path=str(resolved),
            direction=direction,
            mismatches=mismatches,
        )
    samples = descriptor.get("samples_ns")
    if (
        not isinstance(samples, list)
        or len(samples) < 30
        or len(samples) != sample_count
        or any(
            not isinstance(value, int) or isinstance(value, bool) or value <= 0
            for value in samples
        )
    ):
        return skipped(
            "native_baseline_samples_invalid",
            path=str(resolved),
            direction=direction,
            minimum_sample_count=30,
        )
    sorted_samples = sorted(samples)
    p50_ns = percentile(sorted_samples, 0.50)
    return {
        "status": "measured",
        "schema_version": NATIVE_COPY_BASELINE_SCHEMA_VERSION,
        "direction": direction,
        "api": NATIVE_COPY_APIS[direction],
        "completion_boundary": "cuStreamSynchronize_return",
        "host_allocation": NATIVE_COPY_HOST_ALLOCATION,
        "path": str(resolved),
        "sha256": sha256_file(resolved),
        "sample_count": len(samples),
        "samples_ns_sha256": hashlib.sha256(
            json.dumps(samples, separators=(",", ":")).encode("utf-8")
        ).hexdigest(),
        "minimum_ns": sorted_samples[0],
        "maximum_ns": sorted_samples[-1],
        "p50_ns": p50_ns,
        "bytes_per_second_p50": float(copy_bytes) * 1_000_000_000.0 / float(p50_ns),
        "native_reference": {
            "device": native_reference["device"],
            "driver": native_reference["driver"],
            "provider": native_reference["provider"],
            "benchmark": expected_benchmark,
            "host_kernel": native_reference["host_kernel"],
        },
        "benchmark_command": benchmark_command,
        "benchmark_command_sha256": hashlib.sha256(
            json.dumps(benchmark_command, separators=(",", ":")).encode("utf-8")
        ).hexdigest(),
        "environment": expected_environment,
        "environment_sha256": hashlib.sha256(
            json.dumps(
                expected_environment, sort_keys=True, separators=(",", ":")
            ).encode("utf-8")
        ).hexdigest(),
    }


def copy_throughput_check(
    *,
    metrics: dict[str, Any],
    fingerprint: dict[str, Any],
    direction: str,
    copy_bytes: int | None,
    warmup_count: int | None,
    sample_count: int | None,
    baseline_path: Path | None,
    eligible: bool,
    ineligible_reason: str,
) -> dict[str, Any]:
    metric_name = COPY_COMPLETION_METRICS[direction]
    metric = metrics.get(metric_name)
    if (
        metric is None
        or metric.get("status") != "measured"
        or metric.get("unit") != "ns"
        or not isinstance(metric.get("p50"), (int, float))
        or isinstance(metric.get("p50"), bool)
        or metric["p50"] <= 0
        or copy_bytes is None
        or copy_bytes <= 0
    ):
        return skipped(
            "managed_copy_metric_not_measured",
            direction=direction,
            metric=metric_name,
        )
    if (
        warmup_count is None
        or warmup_count <= 0
        or sample_count is None
        or sample_count <= 0
    ):
        return skipped(
            "managed_copy_workload_counts_not_measured",
            direction=direction,
            metric=metric_name,
        )
    if metric.get("sample_count") != sample_count:
        return skipped(
            "managed_copy_sample_count_mismatch",
            direction=direction,
            metric=metric_name,
            expected_sample_count=sample_count,
            observed_sample_count=metric.get("sample_count"),
        )
    observed_bps = float(copy_bytes) * 1_000_000_000.0 / float(metric["p50"])
    baseline = load_native_copy_baseline(
        baseline_path,
        fingerprint,
        copy_bytes,
        direction,
        warmup_count,
        sample_count,
    )
    if baseline["status"] != "measured":
        return {
            **baseline,
            "metric": metric_name,
            "observed_bytes_per_second_p50": observed_bps,
            "copy_bytes": copy_bytes,
        }
    if not eligible:
        return {
            "status": "not_evaluated",
            "reason": ineligible_reason,
            "direction": direction,
            "metric": metric_name,
            "observed_bytes_per_second_p50": observed_bps,
            "native_baseline": baseline,
            "copy_bytes": copy_bytes,
        }
    ratio = observed_bps / baseline["bytes_per_second_p50"]
    return {
        "status": "pass" if ratio >= 0.90 else "fail",
        "direction": direction,
        "metric": metric_name,
        "observed_bytes_per_second_p50": observed_bps,
        "native_baseline": baseline,
        "ratio": ratio,
        "limit": 0.90,
        "copy_bytes": copy_bytes,
    }


def binding_build_policy_check(
    args: argparse.Namespace, fingerprint: dict[str, Any]
) -> dict[str, Any]:
    manifest_artifact = (
        fingerprint.get("toolchain", {}).get("build_manifest", {})
        if isinstance(fingerprint, dict)
        else {}
    )
    descriptor = manifest_artifact.get("descriptor")
    expected_pgo_mode = getattr(args, "binding_pgo_mode", None)
    expected_profile = getattr(args, "binding_pgo_profile_sha256", None)
    expected = {
        "build_type": "Release",
        "lto": "ON",
        "pgo_mode": expected_pgo_mode,
        "pgo_profile_sha256": (
            expected_profile if expected_pgo_mode == "USE" else "none"
        ),
    }
    mismatches: dict[str, Any] = {}
    if manifest_artifact.get("status") != "measured" or not isinstance(descriptor, dict):
        mismatches["build_manifest"] = {
            "expected": "measured JSON descriptor",
            "observed": manifest_artifact,
        }
    else:
        for key, value in expected.items():
            if descriptor.get(key) != value:
                mismatches[key] = {
                    "expected": value,
                    "observed": descriptor.get(key),
                }
    if expected_pgo_mode not in {"OFF", "USE"}:
        mismatches["binding_pgo_mode"] = {
            "expected": "explicit OFF or USE",
            "observed": expected_pgo_mode,
        }
    if expected_pgo_mode == "USE" and (
        not isinstance(expected_profile, str)
        or re.fullmatch(r"[0-9a-f]{64}", expected_profile) is None
    ):
        mismatches["binding_pgo_profile_sha256"] = {
            "expected": "64 lowercase hexadecimal characters",
            "observed": expected_profile,
        }

    build_root = None
    if manifest_artifact.get("status") == "measured":
        build_root = Path(manifest_artifact["path"]).parent.resolve()
    binary_artifacts = fingerprint.get("toolchain", {}).get("binaries", {})
    required_binaries = (
        "ring_benchmark",
        "cuda_benchmark",
        "nvml_benchmark",
        "daemon",
        "cuda_provider",
        "nvml_provider",
    )
    for name in required_binaries:
        artifact = binary_artifacts.get(name, {})
        in_build_root = (
            build_root is not None
            and artifact.get("status") == "measured"
            and Path(artifact["path"]).resolve().is_relative_to(build_root)
        )
        if not in_build_root:
            mismatches[f"{name}_build_root"] = {
                "expected": str(build_root) if build_root is not None else None,
                "observed": artifact,
            }
    return {
        "status": "pass" if not mismatches else "fail",
        "expected": expected,
        "manifest": manifest_artifact,
        "managed_build_root": str(build_root) if build_root is not None else None,
        "mismatches": mismatches,
    }


def qualify(
    args: argparse.Namespace,
    fingerprint: dict[str, Any],
    metrics: dict[str, Any],
    processes: dict[str, Any],
) -> dict[str, Any]:
    vendor = fingerprint["cpu"]["identity"].get("vendor_id")
    expected_vendor = {"amd": "AuthenticAMD", "intel": "GenuineIntel"}.get(
        args.reference_host_role
    )
    eligibility_reasons: list[str] = []
    build_policy = binding_build_policy_check(args, fingerprint)
    if args.mode != "binding-reference":
        eligibility_reasons.append("run_mode_is_smoke")
    elif build_policy["status"] != "pass":
        eligibility_reasons.append("binding_build_policy_mismatch")
    canonical_budget = fingerprint["run_contract"]["canonical_milestone_budget"]
    if canonical_budget.get("status") != "measured":
        eligibility_reasons.append("canonical_milestone_budget_status_unavailable")
    elif canonical_budget.get("budget_status") != "binding":
        eligibility_reasons.append("canonical_milestone_budget_status_is_provisional")
    if args.budget_status != canonical_budget.get("budget_status"):
        eligibility_reasons.append("declared_budget_status_mismatch")
    if not args.controlled_host:
        eligibility_reasons.append("controlled_host_not_declared")
    if expected_vendor is None:
        eligibility_reasons.append("reference_host_role_not_declared")
    elif vendor != expected_vendor:
        eligibility_reasons.append("reference_host_cpu_vendor_mismatch")
    if fingerprint["cpu"]["worker_cpu"] in parse_cpu_list(
        fingerprint["cpu"]["thread_siblings"]
    ):
        eligibility_reasons.append("client_and_worker_share_physical_core")
    if fingerprint["cpu"]["numa_node"] != fingerprint["cpu"]["worker_numa_node"]:
        eligibility_reasons.append("client_and_worker_numa_node_mismatch")
    timer_metric = metrics.get("monotonic_raw_pair_overhead_ns")
    if timer_metric is None or timer_metric.get("status") != "measured":
        eligibility_reasons.append("clock_pair_overhead_not_measured")
    elif timer_metric["p99"] > 100:
        eligibility_reasons.append("clock_pair_overhead_exceeds_100ns")
    eligible = not eligibility_reasons
    ineligible_reason = eligibility_reasons[0] if eligibility_reasons else "eligible"

    checks = {
        "binding_build_policy": (
            build_policy
            if args.mode == "binding-reference"
            else {
                "status": "not_evaluated",
                "reason": "run_mode_is_smoke",
                "observed": build_policy,
            }
        ),
        "warm_launch_p50": check_limit(
            metrics,
            "cuda_warm_launch_submit_ns",
            "p50",
            1_000,
            eligible,
            ineligible_reason,
        ),
        "warm_launch_p99": check_limit(
            metrics,
            "cuda_warm_launch_submit_ns",
            "p99",
            3_000,
            eligible,
            ineligible_reason,
        ),
        "nvml_hot_getter_p99": check_limit(
            metrics,
            "nvml_hot_memory_getter_ns",
            "p99",
            5_000,
            eligible,
            ineligible_reason,
        ),
        "nvml_warm_init_p99": check_limit(
            metrics,
            "nvml_warm_init_ns",
            "p99",
            5_000_000,
            eligible,
            ineligible_reason,
        ),
    }

    if timer_metric is None or timer_metric.get("status") != "measured":
        checks["clock_pair_overhead_suitability"] = skipped(
            "clock_pair_overhead_not_measured"
        )
    elif args.mode != "binding-reference":
        checks["clock_pair_overhead_suitability"] = {
            "status": "not_evaluated",
            "reason": "run_mode_is_smoke",
            "observed_p99_ns": timer_metric["p99"],
            "limit_ns": 100,
        }
    elif timer_metric["p99"] > 100:
        checks["clock_pair_overhead_suitability"] = skipped(
            "clock_pair_overhead_exceeds_100ns",
            observed_p99_ns=timer_metric["p99"],
            limit_ns=100,
        )
    else:
        checks["clock_pair_overhead_suitability"] = {
            "status": "pass",
            "observed_p99_ns": timer_metric["p99"],
            "limit_ns": 100,
        }

    consumer_doorbells = benchmark_metadata(processes, "ring", "consumer_doorbells")
    producer_doorbells = benchmark_metadata(processes, "ring", "producer_doorbells")
    if consumer_doorbells is None or producer_doorbells is None:
        checks["active_ring_doorbells"] = skipped("ring_doorbell_counters_not_measured")
    elif not eligible:
        checks["active_ring_doorbells"] = {
            "status": "not_evaluated",
            "reason": ineligible_reason,
            "consumer_doorbells": consumer_doorbells,
            "producer_doorbells": producer_doorbells,
        }
    else:
        checks["active_ring_doorbells"] = {
            "status": "pass"
            if consumer_doorbells == 0 and producer_doorbells == 0
            else "fail",
            "consumer_doorbells": consumer_doorbells,
            "producer_doorbells": producer_doorbells,
            "limit": 0,
            "interpretation": "uncontended active ring observed no waiter wake path",
        }

    copy_bytes = benchmark_metadata(processes, "cuda", "copy_bytes")
    direct_copy_bytes = benchmark_metadata(processes, "cuda", "direct_copy_bytes")
    warmup_count = benchmark_metadata(processes, "cuda", "warmup_count")
    sample_count = benchmark_metadata(processes, "cuda", "sample_count")
    checks["managed_d2d_copy_throughput"] = copy_throughput_check(
        metrics=metrics,
        fingerprint=fingerprint,
        direction="d2d",
        copy_bytes=copy_bytes,
        warmup_count=warmup_count,
        sample_count=sample_count,
        baseline_path=args.native_d2d_baseline_json,
        eligible=eligible,
        ineligible_reason=ineligible_reason,
    )
    checks["direct_h2d_copy_throughput"] = copy_throughput_check(
        metrics=metrics,
        fingerprint=fingerprint,
        direction="h2d",
        copy_bytes=direct_copy_bytes,
        warmup_count=warmup_count,
        sample_count=sample_count,
        baseline_path=args.native_h2d_baseline_json,
        eligible=eligible,
        ineligible_reason=ineligible_reason,
    )
    checks["direct_d2h_copy_throughput"] = copy_throughput_check(
        metrics=metrics,
        fingerprint=fingerprint,
        direction="d2h",
        copy_bytes=direct_copy_bytes,
        warmup_count=warmup_count,
        sample_count=sample_count,
        baseline_path=args.native_d2h_baseline_json,
        eligible=eligible,
        ineligible_reason=ineligible_reason,
    )

    checks["direct_host_copy_path"] = direct_host_copy_path_check(metrics, processes)

    checks["active_ring_syscall_allocation_lock_audit"] = active_ring_audit_check(
        processes
    )
    checks["contended_case_coverage"] = skipped("contended_workloads_not_configured")
    checks["kernel_scheduling_overhead"] = skipped("100us_kernel_baseline_not_configured")
    checks["nvidia_smi_1hz_interference"] = skipped("stock_tool_interference_run_not_configured")
    checks["passthrough_throughput_loss"] = skipped("passthrough_baseline_not_configured")

    if args.mode == "smoke":
        subset_status = "smoke_only"
        full_status = "smoke_only"
    else:
        statuses = {check["status"] for check in checks.values()}
        subset_status = (
            "fail" if "fail" in statuses else "incomplete" if statuses - {"pass"} else "pass"
        )
        full_status = subset_status

    physical_vendor_available = Path("/proc/driver/nvidia/version").is_file() and Path(
        "/dev/nvidiactl"
    ).exists()
    if physical_vendor_available:
        physical_vendor = skipped("physical_vendor_baseline_not_configured")
    else:
        physical_vendor = skipped("no_physical_nvidia_driver_or_device")
    host_coverage: dict[str, Any] = {}
    for role, role_vendor in (("amd", "AuthenticAMD"), ("intel", "GenuineIntel")):
        if vendor != role_vendor:
            host_coverage[role] = skipped("host_cpu_vendor_mismatch", observed_vendor=vendor)
        elif eligible and args.reference_host_role == role:
            host_coverage[role] = {"status": "measured", "gate_status": subset_status}
        else:
            host_coverage[role] = skipped(
                "smoke_or_nonbinding_reference_run", observed_vendor=vendor
            )

    return {
        "scope": "M0001 core performance subset; full W06 acceptance requires every listed check",
        "run_classification": args.mode,
        "binding_eligibility": {
            "eligible": eligible,
            "reasons": eligibility_reasons,
        },
        "subset_gate_status": subset_status,
        "m0001_performance_gate_status": full_status,
        "checks": checks,
        "reference_host_coverage": host_coverage,
        "physical_vendor_coverage": physical_vendor,
    }


def write_json(path: Path, value: Any) -> None:
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    os.replace(temporary, path)


def write_raw_samples(path: Path, rows: list[dict[str, Any]]) -> None:
    temporary = path.with_name(path.name + ".tmp")
    with temporary.open("w", encoding="utf-8", newline="") as destination:
        writer = csv.DictWriter(
            destination,
            fieldnames=("source", "metric", "sample_index", "value", "unit"),
        )
        writer.writeheader()
        writer.writerows(rows)
    os.replace(temporary, path)


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Archive reproducible M0001 CPU-backed performance evidence"
    )
    parser.add_argument("--mode", choices=("smoke", "binding-reference"), default="smoke")
    parser.add_argument("--budget-status", choices=("provisional", "binding"), default="provisional")
    parser.add_argument("--binding-pgo-mode", choices=("OFF", "USE"))
    parser.add_argument("--binding-pgo-profile-sha256")
    parser.add_argument("--reference-host-role", choices=("amd", "intel"))
    parser.add_argument("--controlled-host", action="store_true")
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--repository", required=True, type=Path)
    parser.add_argument("--milestone-plan", type=Path)
    parser.add_argument("--ring-benchmark", type=Path)
    parser.add_argument("--cuda-benchmark", type=Path)
    parser.add_argument("--nvml-benchmark", type=Path)
    parser.add_argument("--daemon", type=Path)
    parser.add_argument("--provider-dir", type=Path)
    parser.add_argument("--cuda-provider", type=Path)
    parser.add_argument("--nvml-provider", type=Path)
    parser.add_argument("--compiler-epoch", type=Path)
    parser.add_argument("--build-manifest", type=Path)
    parser.add_argument("--execution-mode", default="interpreter")
    parser.add_argument("--warmup", type=int, default=100)
    parser.add_argument("--samples", type=int, default=1_000)
    parser.add_argument("--copy-bytes", type=int, default=16 * 1024 * 1024)
    parser.add_argument("--cpu", type=int)
    parser.add_argument("--worker-cpu", type=int)
    parser.add_argument("--timeout", type=float, default=120.0)
    parser.add_argument("--strace", type=Path)
    parser.add_argument("--native-h2d-baseline-json", type=Path)
    parser.add_argument("--native-d2h-baseline-json", type=Path)
    parser.add_argument(
        "--native-d2d-baseline-json",
        "--native-copy-baseline-json",
        dest="native_d2d_baseline_json",
        type=Path,
    )
    parser.add_argument("--native-device-bdf")
    parser.add_argument("--native-cuda-library", type=Path)
    parser.add_argument("--native-copy-benchmark", type=Path)
    parser.add_argument("--llvm-readobj", type=Path)
    parser.add_argument("--require-all-core-metrics", action="store_true")
    args = parser.parse_args()
    if args.warmup <= 0 or args.samples <= 0 or args.copy_bytes <= 0 or args.timeout <= 0:
        parser.error("warmup, samples, copy-bytes, and timeout must be positive")
    if args.mode == "binding-reference" and args.binding_pgo_mode is None:
        parser.error("binding-reference requires --binding-pgo-mode OFF or USE")
    if args.binding_pgo_mode == "USE" and (
        args.binding_pgo_profile_sha256 is None
        or re.fullmatch(r"[0-9a-f]{64}", args.binding_pgo_profile_sha256) is None
    ):
        parser.error("PGO USE requires --binding-pgo-profile-sha256 with 64 lowercase hex digits")
    if args.binding_pgo_mode != "USE" and args.binding_pgo_profile_sha256 is not None:
        parser.error("--binding-pgo-profile-sha256 is valid only with --binding-pgo-mode USE")
    return args


def main() -> int:
    args = parse_arguments()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    original_affinity = sorted(os.sched_getaffinity(0))
    if not original_affinity:
        raise RuntimeError("sched_getaffinity returned an empty effective CPU set")
    selected_cpu = args.cpu if args.cpu is not None else original_affinity[0]
    if selected_cpu not in original_affinity:
        raise RuntimeError(
            f"requested CPU {selected_cpu} is outside effective affinity {original_affinity}"
        )
    os.sched_setaffinity(0, {selected_cpu})
    worker_cpu = (
        args.worker_cpu
        if args.worker_cpu is not None
        else choose_worker_cpu(original_affinity, selected_cpu)
    )
    if worker_cpu not in original_affinity:
        raise RuntimeError(
            f"requested worker CPU {worker_cpu} is outside effective affinity {original_affinity}"
        )
    fingerprint = collect_fingerprint(args, original_affinity, selected_cpu, worker_cpu)
    environment = os.environ.copy()
    rows: list[dict[str, Any]] = []
    processes: dict[str, Any] = {}
    fatal_error: str | None = None

    try:
        if args.ring_benchmark is not None:
            new_rows, processes["ring"] = run_benchmark(
                "ring",
                args.ring_benchmark,
                [str(args.warmup), str(args.samples)],
                environment,
                args.timeout,
            )
            rows.extend(new_rows)
            processes["ring_audit"] = run_ring_audit(
                args, args.ring_benchmark, environment
            )
        else:
            processes["ring"] = skipped("ring_benchmark_not_configured")
            processes["ring_audit"] = skipped("ring_benchmark_not_configured")

        managed_requested = args.cuda_benchmark is not None or args.nvml_benchmark is not None
        if managed_requested:
            if args.daemon is None or args.provider_dir is None:
                raise RuntimeError(
                    "managed benchmarks require both --daemon and --provider-dir"
                )
            with tempfile.TemporaryDirectory(prefix="metaflux-m0001-performance-") as temporary:
                root = Path(temporary)
                socket_path = root / "metafluxd.sock"
                environment["METAFLUX_MODE"] = "managed"
                environment["METAFLUX_SOCKET"] = str(socket_path)
                environment["METAFLUX_CPU_EXECUTION_MODE"] = args.execution_mode
                environment["METAFLUX_COMPILER_CACHE"] = str(root / "compiler-cache")
                previous_library_path = environment.get("LD_LIBRARY_PATH")
                environment["LD_LIBRARY_PATH"] = str(args.provider_dir.resolve())
                if previous_library_path:
                    environment["LD_LIBRARY_PATH"] += os.pathsep + previous_library_path
                daemon = subprocess.Popen(
                    [str(args.daemon.resolve()), "--socket", str(socket_path)],
                    env=environment,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    text=True,
                    preexec_fn=lambda: os.sched_setaffinity(0, {worker_cpu}),
                )
                daemon_stdout = ""
                daemon_stderr = ""
                daemon_placement: dict[str, Any] = skipped(
                    "daemon_placement_not_captured"
                )
                try:
                    wait_for_socket(socket_path, daemon, min(10.0, args.timeout))
                    if args.cuda_benchmark is not None:
                        new_rows, processes["cuda"] = run_benchmark(
                            "cuda",
                            args.cuda_benchmark,
                            [str(args.warmup), str(args.samples), str(args.copy_bytes)],
                            environment,
                            args.timeout,
                        )
                        rows.extend(new_rows)
                    else:
                        processes["cuda"] = skipped("cuda_benchmark_not_configured")
                    if args.nvml_benchmark is not None:
                        new_rows, processes["nvml"] = run_benchmark(
                            "nvml",
                            args.nvml_benchmark,
                            [str(args.warmup), str(args.samples)],
                            environment,
                            args.timeout,
                        )
                        rows.extend(new_rows)
                    else:
                        processes["nvml"] = skipped("nvml_benchmark_not_configured")
                    daemon_placement = collect_process_thread_placement(daemon.pid)
                finally:
                    daemon_stdout, daemon_stderr = stop_daemon(daemon)
                    processes["daemon"] = {
                        "status": "measured" if daemon.returncode == 0 else "failed",
                        "returncode": daemon.returncode,
                        "stdout": daemon_stdout.splitlines(),
                        "stderr": daemon_stderr.splitlines(),
                        "copy_path_counters": parse_daemon_copy_path_counters(daemon_stderr),
                        "thread_placement": daemon_placement,
                    }
                if daemon.returncode != 0:
                    raise RuntimeError(f"metafluxd exited with {daemon.returncode}")
        else:
            processes["cuda"] = skipped("cuda_benchmark_not_configured")
            processes["nvml"] = skipped("nvml_benchmark_not_configured")
            processes["daemon"] = skipped("managed_benchmarks_not_configured")
    except (RuntimeError, TimeoutError, subprocess.TimeoutExpired, OSError) as error:
        fatal_error = str(error)

    raw_path = args.output_dir / "raw-samples.csv"
    write_raw_samples(raw_path, rows)
    metrics = summarize_metrics(rows, args.samples) if rows else {}
    for metric in CORE_METRICS:
        if metric not in metrics:
            metrics[metric] = skipped("benchmark_metric_not_emitted")
    if args.require_all_core_metrics:
        missing = [metric for metric in CORE_METRICS if metrics[metric]["status"] != "measured"]
        if missing and fatal_error is None:
            fatal_error = f"required core metrics were not measured: {', '.join(missing)}"

    fingerprint_path = args.output_dir / "fingerprint.json"
    write_json(fingerprint_path, fingerprint)
    qualification = qualify(args, fingerprint, metrics, processes)
    direct_path = qualification["checks"]["direct_host_copy_path"]
    if args.cuda_benchmark is not None and direct_path["status"] != "pass" and fatal_error is None:
        fatal_error = "direct host-copy path qualification failed"
    ring_audit = qualification["checks"][
        "active_ring_syscall_allocation_lock_audit"
    ]
    if (
        args.ring_benchmark is not None
        and args.strace is not None
        and ring_audit["status"] != "pass"
        and fatal_error is None
    ):
        fatal_error = "active ring syscall/allocation/lock audit failed"
    results = {
        "schema_version": 2,
        "status": "failed" if fatal_error else "measured",
        "fatal_error": fatal_error,
        "fingerprint": {
            "path": fingerprint_path.name,
            "sha256": sha256_file(fingerprint_path),
        },
        "raw_samples": {
            "path": raw_path.name,
            "sha256": sha256_file(raw_path),
            "row_count": len(rows),
        },
        "processes": processes,
        "metrics": metrics,
        "qualification": qualification,
    }
    results_path = args.output_dir / "results.json"
    write_json(results_path, results)
    print(
        f"M0001 performance evidence: {results_path} "
        f"gate={qualification['m0001_performance_gate_status']}"
    )
    if fatal_error is not None:
        print(f"M0001 performance error: {fatal_error}")
        return 1
    if args.mode == "binding-reference" and qualification["m0001_performance_gate_status"] != "pass":
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
