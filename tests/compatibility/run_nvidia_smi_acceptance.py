#!/usr/bin/env python3
"""Run the frozen stock nvidia-smi matrix against the managed NVML provider."""

from __future__ import annotations

import argparse
import csv
import datetime as dt
import hashlib
import io
import json
import os
import re
import select
import signal
import socket
import subprocess
import sys
import tempfile
import time
import xml.etree.ElementTree as ET
from pathlib import Path
from typing import Any


EXPECTED_FAMILIES = ("R535", "R550", "R570", "R580", "R610")
EXPECTED_NAME = "MetaFlux Virtual Compute Device"
EXPECTED_UUID = "GPU-4d465843-5055-0001-0000-000000000001"
EXPECTED_PCI_BUS_ID = "00000000:00:01.0"
EXPECTED_DRIVER_VERSION = "610.43.02"
EXPECTED_CUDA_VERSION = "13.3"
GPU_QUERY_FIELDS = (
    "index",
    "name",
    "uuid",
    "pci.bus_id",
    "memory.total",
    "memory.used",
    "memory.free",
    "utilization.gpu",
    "utilization.memory",
)
GPU_QUERY_HEADER = (
    "index",
    "name",
    "uuid",
    "pci.bus_id",
    "memory.total [MiB]",
    "memory.used [MiB]",
    "memory.free [MiB]",
    "utilization.gpu [%]",
    "utilization.memory [%]",
)
UNSUPPORTED_PHYSICAL_FIELDS = ("temperature.gpu", "power.draw", "fan.speed")
IDLE_UTILIZATION_SETTLE_SECONDS = 0.15
NVIDIA_SMI_TIMESTAMP_PATTERN = re.compile(
    r"(?:Mon|Tue|Wed|Thu|Fri|Sat|Sun)\s+"
    r"(?:Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec)\s+"
    r"\d{1,2}\s+\d{2}:\d{2}:\d{2}\s+\d{4}"
)


IDENTITY_PROBE = r"""
import ctypes
import json
import sys


class CudaUuid(ctypes.Structure):
    _fields_ = [("bytes", ctypes.c_ubyte * 16)]


def require(call, name):
    result = call()
    if result != 0:
        raise RuntimeError(f"{name} returned {result}")


cuda = ctypes.CDLL(sys.argv[1], mode=ctypes.RTLD_LOCAL)
nvml = ctypes.CDLL(sys.argv[2], mode=ctypes.RTLD_LOCAL)
cuda.cuInit.argtypes = [ctypes.c_uint]
cuda.cuInit.restype = ctypes.c_int
cuda.cuDeviceGetCount.argtypes = [ctypes.POINTER(ctypes.c_int)]
cuda.cuDeviceGetCount.restype = ctypes.c_int
cuda.cuDeviceGet.argtypes = [ctypes.POINTER(ctypes.c_int), ctypes.c_int]
cuda.cuDeviceGet.restype = ctypes.c_int
cuda.cuDeviceGetName.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_int]
cuda.cuDeviceGetName.restype = ctypes.c_int
cuda.cuDeviceGetUuid_v2.argtypes = [ctypes.POINTER(CudaUuid), ctypes.c_int]
cuda.cuDeviceGetUuid_v2.restype = ctypes.c_int
nvml.nvmlInit_v2.argtypes = []
nvml.nvmlInit_v2.restype = ctypes.c_int
nvml.nvmlShutdown.argtypes = []
nvml.nvmlShutdown.restype = ctypes.c_int
nvml.nvmlDeviceGetCount_v2.argtypes = [ctypes.POINTER(ctypes.c_uint)]
nvml.nvmlDeviceGetCount_v2.restype = ctypes.c_int
nvml.nvmlDeviceGetHandleByIndex_v2.argtypes = [ctypes.c_uint, ctypes.POINTER(ctypes.c_void_p)]
nvml.nvmlDeviceGetHandleByIndex_v2.restype = ctypes.c_int
nvml.nvmlDeviceGetName.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_uint]
nvml.nvmlDeviceGetName.restype = ctypes.c_int
nvml.nvmlDeviceGetUUID.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_uint]
nvml.nvmlDeviceGetUUID.restype = ctypes.c_int

require(lambda: cuda.cuInit(0), "cuInit")
require(lambda: nvml.nvmlInit_v2(), "nvmlInit_v2")
try:
    cuda_count = ctypes.c_int()
    nvml_count = ctypes.c_uint()
    require(lambda: cuda.cuDeviceGetCount(ctypes.byref(cuda_count)), "cuDeviceGetCount")
    require(lambda: nvml.nvmlDeviceGetCount_v2(ctypes.byref(nvml_count)), "nvmlDeviceGetCount_v2")
    cuda_devices = []
    for index in range(cuda_count.value):
        device = ctypes.c_int()
        name = ctypes.create_string_buffer(96)
        uuid = CudaUuid()
        require(lambda: cuda.cuDeviceGet(ctypes.byref(device), index), "cuDeviceGet")
        require(lambda: cuda.cuDeviceGetName(name, len(name), device.value), "cuDeviceGetName")
        require(lambda: cuda.cuDeviceGetUuid_v2(ctypes.byref(uuid), device.value), "cuDeviceGetUuid_v2")
        raw = bytes(uuid.bytes)
        formatted = (
            raw[0:4].hex()
            + "-"
            + raw[4:6].hex()
            + "-"
            + raw[6:8].hex()
            + "-"
            + raw[8:10].hex()
            + "-"
            + raw[10:16].hex()
        )
        cuda_devices.append(
            {
                "index": index,
                "name": name.value.decode("ascii"),
                "uuid": "GPU-" + formatted,
            }
        )
    nvml_devices = []
    for index in range(nvml_count.value):
        device = ctypes.c_void_p()
        name = ctypes.create_string_buffer(96)
        uuid = ctypes.create_string_buffer(96)
        require(
            lambda: nvml.nvmlDeviceGetHandleByIndex_v2(index, ctypes.byref(device)),
            "nvmlDeviceGetHandleByIndex_v2",
        )
        require(lambda: nvml.nvmlDeviceGetName(device, name, len(name)), "nvmlDeviceGetName")
        require(lambda: nvml.nvmlDeviceGetUUID(device, uuid, len(uuid)), "nvmlDeviceGetUUID")
        nvml_devices.append(
            {
                "index": index,
                "name": name.value.decode("ascii"),
                "uuid": uuid.value.decode("ascii"),
            }
        )
    print(
        json.dumps(
            {
                "cuda": {"count": cuda_count.value, "devices": cuda_devices},
                "nvml": {"count": nvml_count.value, "devices": nvml_devices},
            },
            sort_keys=True,
        )
    )
finally:
    require(lambda: nvml.nvmlShutdown(), "nvmlShutdown")
"""


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--daemon", required=True, type=Path)
    parser.add_argument("--provider-dir", required=True, type=Path)
    parser.add_argument("--holder", required=True, type=Path)
    parser.add_argument("--tools-root", required=True, type=Path)
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument(
        "--elf-loader",
        type=Path,
        help="invoke each frozen stock binary through this explicit ELF interpreter",
    )
    parser.add_argument("--source-date-epoch", type=int)
    parser.add_argument(
        "--synthetic-cpu-topology",
        action="store_true",
        help="create an explicit affinity-derived CPU topology fixture for isolated build sandboxes",
    )
    parser.add_argument(
        "--deterministic",
        action="store_true",
        help="omit timing and raw output while retaining stream hashes and semantic summaries",
    )
    return parser.parse_args()


def create_synthetic_cpu_topology(root: Path) -> None:
    affinity_cpus = sorted(os.sched_getaffinity(0))
    if not affinity_cpus:
        raise RuntimeError("the current process has no CPUs in its affinity mask")

    cpu_list = ",".join(str(cpu) for cpu in affinity_cpus) + "\n"
    cpu_root = root / "cpu"
    node_root = root / "node"
    cgroup_root = root / "cgroup"
    (node_root / "node0").mkdir(parents=True)
    cgroup_root.mkdir(parents=True)
    (cpu_root / "online").parent.mkdir(parents=True)
    (cpu_root / "online").write_text(cpu_list, encoding="ascii")
    (node_root / "online").write_text("0\n", encoding="ascii")
    (node_root / "node0" / "cpulist").write_text(cpu_list, encoding="ascii")
    (cgroup_root / "cpuset.cpus.effective").write_text(cpu_list, encoding="ascii")
    (cgroup_root / "cpuset.mems.effective").write_text("0\n", encoding="ascii")

    for cpu in affinity_cpus:
        topology = cpu_root / f"cpu{cpu}" / "topology"
        topology.mkdir(parents=True)
        (topology / "physical_package_id").write_text("0\n", encoding="ascii")
        (topology / "core_id").write_text(f"{cpu}\n", encoding="ascii")
        (topology / "thread_siblings_list").write_text(f"{cpu}\n", encoding="ascii")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def sha256_text(value: str) -> str:
    return hashlib.sha256(value.encode("utf-8")).hexdigest()


def artifact_record(path: Path) -> dict[str, Any]:
    if not path.is_file():
        raise FileNotFoundError(path)
    return {"path": str(path), "size": path.stat().st_size, "sha256": sha256(path)}


def provider_artifacts(provider_dir: Path) -> dict[str, dict[str, Any]]:
    provider_dir = provider_dir.resolve()
    package_root = provider_dir.parents[2]
    return {
        "cuda_provider": artifact_record(provider_dir / "libcuda.so.1.0.0"),
        "nvml_provider": artifact_record(provider_dir / "libnvidia-ml.so.1.0.0"),
        "build_manifest": artifact_record(
            package_root / "share" / "metaflux" / "metaflux-build-manifest.json"
        ),
    }


def wait_for_socket(path: Path, daemon: subprocess.Popen[str]) -> None:
    deadline = time.monotonic() + 10.0
    while time.monotonic() < deadline:
        if daemon.poll() is not None:
            stdout, stderr = daemon.communicate()
            raise RuntimeError(
                f"metafluxd exited before publishing its socket ({daemon.returncode})\n"
                f"stdout:\n{stdout}\nstderr:\n{stderr}"
            )
        try:
            with socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET) as probe:
                probe.settimeout(0.1)
                probe.connect(str(path))
            return
        except (FileNotFoundError, ConnectionRefusedError, TimeoutError, OSError):
            time.sleep(0.01)
    raise TimeoutError(f"metafluxd did not publish {path}")


def stop_daemon(daemon: subprocess.Popen[str]) -> tuple[str, str]:
    if daemon.poll() is None:
        daemon.send_signal(signal.SIGTERM)
    try:
        return daemon.communicate(timeout=5)
    except subprocess.TimeoutExpired:
        daemon.kill()
        return daemon.communicate(timeout=5)


def start_holder(
    binary: Path, environment: dict[str, str]
) -> subprocess.Popen[str]:
    holder = subprocess.Popen(
        [str(binary)],
        env=environment,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        bufsize=1,
    )
    if holder.stdout is None:
        raise RuntimeError("holder stdout pipe was not created")
    deadline = time.monotonic() + 10.0
    while time.monotonic() < deadline:
        if holder.poll() is not None:
            stdout, stderr = holder.communicate()
            raise RuntimeError(
                f"CUDA process holder exited before READY ({holder.returncode})\n"
                f"stdout:\n{stdout}\nstderr:\n{stderr}"
            )
        readable, _, _ = select.select([holder.stdout], [], [], 0.1)
        if readable:
            line = holder.stdout.readline().strip()
            if line == "READY 4194304":
                return holder
            raise RuntimeError(f"unexpected CUDA process holder handshake: {line!r}")
    holder.kill()
    stdout, stderr = holder.communicate(timeout=5)
    raise TimeoutError(
        f"CUDA process holder did not become ready\nstdout:\n{stdout}\nstderr:\n{stderr}"
    )


def stop_holder(holder: subprocess.Popen[str]) -> tuple[str, str]:
    if holder.poll() is None:
        if holder.stdin is None:
            holder.kill()
        else:
            holder.stdin.write("\n")
            holder.stdin.flush()
    try:
        return holder.communicate(timeout=10)
    except subprocess.TimeoutExpired:
        holder.kill()
        return holder.communicate(timeout=5)


def run_case(
    binary: Path,
    arguments: list[str],
    environment: dict[str, str],
    elf_loader: Path | None,
) -> dict[str, Any]:
    command = [str(binary), *arguments]
    if elf_loader is not None:
        command.insert(0, str(elf_loader))
    started = time.monotonic_ns()
    result = subprocess.run(
        command,
        env=environment,
        capture_output=True,
        text=True,
        timeout=30,
        check=False,
    )
    record = {
        "command": command,
        "returncode": result.returncode,
        "duration_ns": time.monotonic_ns() - started,
        "stdout": result.stdout,
        "stderr": result.stderr,
        "stdout_sha256": sha256_text(result.stdout),
        "stderr_sha256": sha256_text(result.stderr),
    }
    if result.returncode != 0:
        raise RuntimeError(
            f"stock command failed ({result.returncode}): {' '.join(command)}\n"
            f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}"
        )
    if result.stderr != "":
        raise RuntimeError(
            f"stock command wrote unexpected stderr: {' '.join(command)}\n"
            f"stderr:\n{result.stderr}\nstdout:\n{result.stdout}"
        )
    return record


def case_error(record: dict[str, Any], message: str) -> RuntimeError:
    return RuntimeError(
        f"{message}: {' '.join(record['command'])}\n"
        f"stdout:\n{record['stdout']}\nstderr:\n{record['stderr']}"
    )


def finish_case(
    record: dict[str, Any], semantic: dict[str, Any], deterministic: bool
) -> dict[str, Any]:
    del deterministic
    record["semantic"] = semantic
    return record


def normalize_deterministic_text(value: str, holder_pid: int) -> str:
    value = re.sub(rf"(?<!\d){holder_pid}(?!\d)", "<holder-pid>", value)
    return NVIDIA_SMI_TIMESTAMP_PATTERN.sub("<nvidia-smi-timestamp>", value)


def deterministic_evidence_projection(value: Any, holder_pid: int) -> Any:
    if isinstance(value, dict):
        projected = {
            key: deterministic_evidence_projection(item, holder_pid)
            for key, item in value.items()
            if key
            not in {
                "duration_ns",
                "stdout",
                "stderr",
                "stdout_sha256",
                "stderr_sha256",
            }
        }
        if "stdout" in value and "stderr" in value:
            normalized_stdout = normalize_deterministic_text(str(value["stdout"]), holder_pid)
            normalized_stderr = normalize_deterministic_text(str(value["stderr"]), holder_pid)
            projected["stdout_sha256"] = sha256_text(normalized_stdout)
            projected["stderr_sha256"] = sha256_text(normalized_stderr)
            projected["stream_hash_basis"] = "normalized-dynamic-fields-v1"
        return projected
    if isinstance(value, list):
        return [deterministic_evidence_projection(item, holder_pid) for item in value]
    if isinstance(value, str):
        return normalize_deterministic_text(value, holder_pid)
    return value


def project_top_level_streams(evidence: dict[str, Any], holder_pid: int) -> None:
    for process in ("holder", "daemon"):
        for stream in ("stdout", "stderr"):
            key = f"{process}_{stream}"
            normalized = normalize_deterministic_text(str(evidence.pop(key)), holder_pid)
            evidence[f"{key}_sha256"] = sha256_text(normalized)


def parse_csv_rows(record: dict[str, Any]) -> list[tuple[str, ...]]:
    stdout = str(record["stdout"])
    try:
        rows = [
            tuple(row)
            for row in csv.reader(io.StringIO(stdout), skipinitialspace=True, strict=True)
        ]
    except csv.Error as error:
        raise case_error(record, f"malformed CSV output ({error})") from error
    if rows and not stdout.endswith("\n"):
        raise case_error(record, "CSV output is missing its final newline")
    if any(not row for row in rows):
        raise case_error(record, "CSV output contains an empty record")
    return rows


def validate_csv_case(
    record: dict[str, Any],
    expected_rows: list[tuple[str, ...]],
    deterministic: bool,
    view: str,
) -> dict[str, Any]:
    rows = parse_csv_rows(record)
    if rows != expected_rows:
        raise case_error(record, f"{view} CSV rows differ: expected {expected_rows}, got {rows}")
    return finish_case(
        record,
        {
            "view": view,
            "row_count": len(rows),
            "column_count": len(rows[0]) if rows else 0,
            "rows": [list(row) for row in rows],
        },
        deterministic,
    )


def validate_list_case(record: dict[str, Any], deterministic: bool) -> dict[str, Any]:
    expected = f"GPU 0: {EXPECTED_NAME} (UUID: {EXPECTED_UUID})\n"
    if record["stdout"] != expected:
        raise case_error(record, f"-L output differs from the one-device canonical view {expected!r}")
    return finish_case(
        record,
        {
            "view": "list",
            "device_count": 1,
            "devices": [{"index": 0, "name": EXPECTED_NAME, "uuid": EXPECTED_UUID}],
        },
        deterministic,
    )


def validate_summary_case(
    record: dict[str, Any],
    tool_version: str,
    holder_pid: int,
    holder_name: str,
    deterministic: bool,
) -> dict[str, Any]:
    lines = str(record["stdout"]).splitlines()
    header_pattern = re.compile(rf"^\| NVIDIA-SMI {re.escape(tool_version)}\b.*\|$")
    device_pattern = re.compile(
        rf"^\|\s+0\s+MetaFlux Virtual Compute\.\.\..*\|\s*{re.escape(EXPECTED_PCI_BUS_ID)}\s+Off\s*\|.*\|$"
    )
    memory_pattern = re.compile(
        r"^\|\s*N/A\s+N/A\s+N/A.*\|\s*4MiB\s*/\s*256MiB\s*\|\s*0%\s+Default\s*\|$"
    )
    process_names = (holder_name, holder_name[len("metaflux") :])
    process_pattern = re.compile(
        rf"^\|\s+0\s+N/A\s+N/A\s+{holder_pid}\s+C\s+"
        rf"(?:{'|'.join(re.escape(name) for name in process_names)})\s+4MiB\s*\|$"
    )
    required = {
        "header": [line for line in lines if header_pattern.fullmatch(line)],
        "device": [line for line in lines if device_pattern.fullmatch(line)],
        "memory": [line for line in lines if memory_pattern.fullmatch(line)],
        "process": [line for line in lines if process_pattern.fullmatch(line)],
        "process_heading": [line for line in lines if line.startswith("| Processes:")],
    }
    invalid = {name: matches for name, matches in required.items() if len(matches) != 1}
    if invalid:
        raise case_error(record, f"summary structural rows differ: {invalid}")
    return finish_case(
        record,
        {
            "view": "summary",
            "tool_version": tool_version,
            "device_count": 1,
            "process_count": 1,
            "memory": {"used_mib": 4, "total_mib": 256},
            "gpu_utilization_percent": 0,
        },
        deterministic,
    )


def validate_query_case(
    record: dict[str, Any], holder_pid: int, holder_name: str, deterministic: bool
) -> dict[str, Any]:
    lines = [line.strip() for line in str(record["stdout"]).splitlines()]
    pairs = []
    for line in lines:
        if ":" in line:
            key, value = line.split(":", maxsplit=1)
            pairs.append((key.strip(), value.strip()))

    def exactly_one(key: str, value: str) -> None:
        if pairs.count((key, value)) != 1:
            raise case_error(record, f"-q expected exactly one {(key, value)!r}")

    exactly_one("Attached GPUs", "1")
    driver_values = [value for key, value in pairs if key == "Driver Version"]
    cuda_values = [value for key, value in pairs if key == "CUDA Version"]
    accepted_driver_values = {
        EXPECTED_DRIVER_VERSION,
        EXPECTED_DRIVER_VERSION
        + " [Deprecated; will be removed in CUDA 14.0. Use KMD Version instead]",
    }
    accepted_cuda_values = {
        EXPECTED_CUDA_VERSION,
        EXPECTED_CUDA_VERSION
        + " [Deprecated; will be removed in CUDA 14.0. Use CUDA UMD Version instead]",
    }
    if (
        len(driver_values) != 1
        or driver_values[0] not in accepted_driver_values
        or len(cuda_values) != 1
        or cuda_values[0] not in accepted_cuda_values
    ):
        raise case_error(record, "-q driver or CUDA version differs")
    if any(key in {"KMD Version", "CUDA UMD Version"} for key, _ in pairs):
        exactly_one("KMD Version", EXPECTED_DRIVER_VERSION)
        exactly_one("CUDA UMD Version", EXPECTED_CUDA_VERSION)
    if lines.count(f"GPU {EXPECTED_PCI_BUS_ID}") != 1:
        raise case_error(record, f"-q expected exactly one GPU {EXPECTED_PCI_BUS_ID!r}")
    exactly_one("Product Name", EXPECTED_NAME)
    exactly_one("GPU UUID", EXPECTED_UUID)
    if lines.count("FB Memory Usage") != 1:
        raise case_error(record, "-q expected exactly one FB Memory Usage section")
    memory_index = lines.index("FB Memory Usage")
    memory_pairs = {}
    for line in lines[memory_index + 1 : memory_index + 5]:
        if ":" in line:
            key, value = line.split(":", maxsplit=1)
            memory_pairs[key.strip()] = value.strip()
    if memory_pairs != {
        "Total": "256 MiB",
        "Reserved": "0 MiB",
        "Used": "4 MiB",
        "Free": "252 MiB",
    }:
        raise case_error(record, f"-q FB memory block differs: {memory_pairs}")
    exactly_one("Process ID", str(holder_pid))
    process_line = next(line for line in lines if line.startswith("Process ID"))
    process_index = lines.index(process_line)
    process_block = lines[process_index : process_index + 5]
    process_pairs = {}
    for line in process_block:
        if ":" in line:
            key, value = line.split(":", maxsplit=1)
            process_pairs[key.strip()] = value.strip()
    accepted_names = {holder_name, holder_name[len("metaflux") :]}
    if (
        process_pairs.get("Type") != "C"
        or process_pairs.get("Name") not in accepted_names
        or process_pairs.get("Used GPU Memory") != "4 MiB"
    ):
        raise case_error(record, f"-q process block differs: {process_block}")
    return finish_case(
        record,
        {
            "view": "query",
            "attached_gpus": 1,
            "device": {
                "name": EXPECTED_NAME,
                "uuid": EXPECTED_UUID,
                "pci_bus_id": EXPECTED_PCI_BUS_ID,
            },
            "process_count": 1,
            "memory": {"total_mib": 256, "used_mib": 4, "free_mib": 252},
        },
        deterministic,
    )


def validate_xml_case(
    record: dict[str, Any], holder_pid: int, holder_name: str, deterministic: bool
) -> dict[str, Any]:
    stdout = str(record["stdout"])
    doctype = re.findall(
        r'<!DOCTYPE nvidia_smi_log SYSTEM "(nvsmi_device_v\d+\.dtd)">', stdout
    )
    if len(doctype) != 1:
        raise case_error(record, f"XML has an unexpected doctype list: {doctype}")
    try:
        root = ET.fromstring(stdout)
    except ET.ParseError as error:
        raise case_error(record, f"malformed XML output ({error})") from error
    if (
        root.tag != "nvidia_smi_log"
        or root.findtext("driver_version") != EXPECTED_DRIVER_VERSION
        or root.findtext("cuda_version") != EXPECTED_CUDA_VERSION
        or root.findtext("attached_gpus") != "1"
    ):
        raise case_error(record, "XML root, versions, or attached_gpus differs")
    gpus = root.findall("gpu")
    if len(gpus) != 1:
        raise case_error(record, f"XML expected one GPU, got {len(gpus)}")
    gpu = gpus[0]
    expected_values = {
        "@id": EXPECTED_PCI_BUS_ID,
        "product_name": EXPECTED_NAME,
        "uuid": EXPECTED_UUID,
        "fb_memory_usage/total": "256 MiB",
        "fb_memory_usage/used": "4 MiB",
        "fb_memory_usage/free": "252 MiB",
        "utilization/gpu_util": "0 %",
        "utilization/memory_util": "0 %",
    }
    actual_values = {"@id": gpu.get("id")}
    actual_values.update({path: gpu.findtext(path) for path in expected_values if path != "@id"})
    if actual_values != expected_values:
        raise case_error(record, f"XML device values differ: {actual_values}")
    unsupported_paths = (
        "product_brand",
        "serial",
        "fan_speed",
        "bar1_memory_usage/total",
        "temperature/gpu_temp",
        "clocks/graphics_clock",
    )
    unsupported_values = {path: gpu.findtext(path) for path in unsupported_paths}
    if any(value != "N/A" for value in unsupported_values.values()):
        raise case_error(record, f"XML unsupported fields differ: {unsupported_values}")
    processes = gpu.findall("processes/process_info")
    if len(processes) != 1:
        raise case_error(record, f"XML expected one process, got {len(processes)}")
    process = processes[0]
    accepted_names = {holder_name, holder_name[len("metaflux") :]}
    if (
        process.findtext("pid") != str(holder_pid)
        or process.findtext("type") != "C"
        or process.findtext("process_name") not in accepted_names
        or process.findtext("used_memory") != "4 MiB"
    ):
        raise case_error(record, "XML process values differ")
    return finish_case(
        record,
        {
            "view": "xml-query",
            "doctype": doctype[0],
            "attached_gpus": 1,
            "device": actual_values,
            "process_count": 1,
            "unsupported_fields": unsupported_values,
            "memory_utilization_percent": 0,
        },
        deterministic,
    )


def qualify_identity_parity(
    provider_dir: Path, environment: dict[str, str], deterministic: bool
) -> dict[str, dict[str, Any]]:
    cuda_provider = provider_dir / "libcuda.so.1.0.0"
    nvml_provider = provider_dir / "libnvidia-ml.so.1.0.0"

    def probe(name: str, cuda_visible_devices: str | None) -> dict[str, Any]:
        probe_environment = environment.copy()
        if cuda_visible_devices is None:
            probe_environment.pop("CUDA_VISIBLE_DEVICES", None)
        else:
            probe_environment["CUDA_VISIBLE_DEVICES"] = cuda_visible_devices
        started = time.monotonic_ns()
        result = subprocess.run(
            [sys.executable, "-c", IDENTITY_PROBE, str(cuda_provider), str(nvml_provider)],
            env=probe_environment,
            capture_output=True,
            text=True,
            timeout=30,
            check=False,
        )
        record: dict[str, Any] = {
            "command": [
                sys.executable,
                "<cuda-nvml-identity-probe>",
                str(cuda_provider),
                str(nvml_provider),
            ],
            "returncode": result.returncode,
            "duration_ns": time.monotonic_ns() - started,
            "stdout": result.stdout,
            "stderr": result.stderr,
            "stdout_sha256": sha256_text(result.stdout),
            "stderr_sha256": sha256_text(result.stderr),
        }
        if result.returncode != 0 or result.stderr != "":
            raise case_error(record, f"{name} CUDA/NVML identity probe failed")
        try:
            payload = json.loads(result.stdout)
        except json.JSONDecodeError as error:
            raise case_error(record, f"{name} identity probe returned malformed JSON") from error
        expected_device = {"index": 0, "name": EXPECTED_NAME, "uuid": EXPECTED_UUID}
        if cuda_visible_devices is None:
            if payload != {
                "cuda": {"count": 1, "devices": [expected_device]},
                "nvml": {"count": 1, "devices": [expected_device]},
            }:
                raise case_error(record, f"default CUDA/NVML identity parity differs: {payload}")
            semantic = {
                "view": "default-unfiltered-same-revision",
                "cuda_count": 1,
                "nvml_count": 1,
                "ordered_uuid_name_parity": True,
                "devices": [expected_device],
            }
        else:
            if payload != {
                "cuda": {"count": 0, "devices": []},
                "nvml": {"count": 1, "devices": [expected_device]},
            }:
                raise case_error(record, f"filtered CUDA/canonical NVML views differ: {payload}")
            semantic = {
                "view": "empty-cuda-filter-canonical-nvml",
                "cuda_count": 0,
                "nvml_count": 1,
                "nvml_ignores_cuda_visible_devices": True,
                "nvml_devices": [expected_device],
            }
        return finish_case(record, semantic, deterministic)

    return {
        "default_unfiltered": probe("default-unfiltered", None),
        "empty_cuda_filter": probe("empty-cuda-filter", ""),
    }


def qualify_family(
    family: str,
    binary: Path,
    manifest_row: dict[str, Any],
    environment: dict[str, str],
    holder_pid: int,
    holder_name: str,
    deterministic: bool,
    elf_loader: Path | None,
) -> dict[str, Any]:
    expected_binary = manifest_row["binary"]
    if binary.stat().st_size != expected_binary["size"]:
        raise RuntimeError(f"{family} nvidia-smi size differs from the frozen manifest")
    binary_hash = sha256(binary)
    if binary_hash != expected_binary["sha256"]:
        raise RuntimeError(f"{family} nvidia-smi hash differs from the frozen manifest")

    cases: dict[str, dict[str, Any]] = {}
    cases["list"] = validate_list_case(
        run_case(binary, ["-L"], environment, elf_loader), deterministic
    )
    cases["summary"] = validate_summary_case(
        run_case(binary, [], environment, elf_loader),
        manifest_row["driver_version"],
        holder_pid,
        holder_name,
        deterministic,
    )

    query = "--query-gpu=" + ",".join(GPU_QUERY_FIELDS)
    row_with_units = (
        "0",
        EXPECTED_NAME,
        EXPECTED_UUID,
        EXPECTED_PCI_BUS_ID,
        "256 MiB",
        "4 MiB",
        "252 MiB",
        "0 %",
        "0 %",
    )
    row_without_units = (
        "0",
        EXPECTED_NAME,
        EXPECTED_UUID,
        EXPECTED_PCI_BUS_ID,
        "256",
        "4",
        "252",
        "0",
        "0",
    )
    csv_matrix = (
        ("csv", "csv", [GPU_QUERY_HEADER, row_with_units]),
        ("csv_noheader", "csv,noheader", [row_with_units]),
        ("csv_nounits", "csv,nounits", [GPU_QUERY_HEADER, row_without_units]),
        ("csv_noheader_nounits", "csv,noheader,nounits", [row_without_units]),
    )
    for case_name, output_format, expected_rows in csv_matrix:
        cases[case_name] = validate_csv_case(
            run_case(binary, [query, "--format=" + output_format], environment, elf_loader),
            expected_rows,
            deterministic,
            "gpu-" + output_format,
        )

    unsupported_query = "--query-gpu=" + ",".join(UNSUPPORTED_PHYSICAL_FIELDS)
    cases["unsupported_physical_fields"] = validate_csv_case(
        run_case(
            binary,
            [unsupported_query, "--format=csv,noheader,nounits"],
            environment,
            elf_loader,
        ),
        [("[N/A]", "[N/A]", "[N/A]")],
        deterministic,
        "unsupported-physical-fields",
    )

    cases["compute_apps"] = validate_csv_case(
        run_case(
            binary,
            ["--query-compute-apps=pid,process_name,used_memory", "--format=csv,noheader"],
            environment,
            elf_loader,
        ),
        [(str(holder_pid), holder_name, "4 MiB")],
        deterministic,
        "compute-apps",
    )
    cases["query"] = validate_query_case(
        run_case(binary, ["-q"], environment, elf_loader), holder_pid, holder_name, deterministic
    )
    cases["xml"] = validate_xml_case(
        run_case(binary, ["-q", "-x"], environment, elf_loader),
        holder_pid,
        holder_name,
        deterministic,
    )

    return {
        "driver_family": family,
        "driver_version": manifest_row["driver_version"],
        "binary": str(binary),
        "binary_sha256": binary_hash,
        "cases": cases,
    }


def qualify_holder_exit(
    binary: Path,
    environment: dict[str, str],
    deterministic: bool,
    elf_loader: Path | None,
) -> dict[str, Any]:
    record = validate_csv_case(
        run_case(
            binary,
            ["--query-compute-apps=pid,process_name,used_memory", "--format=csv,noheader"],
            environment,
            elf_loader,
        ),
        [],
        deterministic,
        "compute-apps-after-holder-exit",
    )
    memory = validate_csv_case(
        run_case(
            binary,
            [
                "--query-gpu=memory.total,memory.used,memory.free",
                "--format=csv,noheader",
            ],
            environment,
            elf_loader,
        ),
        [("256 MiB", "0 MiB", "256 MiB")],
        deterministic,
        "memory-after-holder-exit",
    )
    return {"compute_apps": record, "memory": memory}


def main() -> int:
    args = parse_arguments()
    for required in (args.daemon, args.provider_dir, args.holder, args.tools_root, args.manifest):
        if not required.exists():
            raise FileNotFoundError(required)
    if args.elf_loader is not None:
        if not args.elf_loader.is_absolute() or not args.elf_loader.is_file():
            raise FileNotFoundError(args.elf_loader)

    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    manifest_tools = manifest["tools"]
    rows = {row["driver_family"]: row for row in manifest_tools}
    if (
        manifest.get("schema_version") != 1
        or tuple(manifest["required_driver_families"]) != EXPECTED_FAMILIES
        or len(manifest_tools) != len(EXPECTED_FAMILIES)
        or len(rows) != len(EXPECTED_FAMILIES)
        or set(rows) != set(EXPECTED_FAMILIES)
    ):
        raise RuntimeError("stock-tool manifest family set differs from the frozen M0100 matrix")
    artifacts = provider_artifacts(args.provider_dir)

    with tempfile.TemporaryDirectory(prefix="metaflux-nvidia-smi-") as temporary:
        socket_path = Path(temporary) / "metafluxd.sock"
        environment = os.environ.copy()
        environment["METAFLUX_SOCKET"] = str(socket_path)
        environment["METAFLUX_MODE"] = "managed"
        environment["METAFLUX_CPU_EXECUTION_MODE"] = "interpreter"
        environment["LC_ALL"] = "C"
        environment["LANG"] = "C"
        environment["TZ"] = "UTC"
        environment.pop("LANGUAGE", None)
        environment.pop("CUDA_VISIBLE_DEVICES", None)
        environment.pop("METAFLUX_CPU_TOPOLOGY_ROOT", None)
        if args.synthetic_cpu_topology:
            topology_root = Path(temporary) / "cpu-topology"
            create_synthetic_cpu_topology(topology_root)
            environment["METAFLUX_CPU_TOPOLOGY_ROOT"] = str(topology_root)
        previous_library_path = environment.get("LD_LIBRARY_PATH")
        environment["LD_LIBRARY_PATH"] = str(args.provider_dir)
        if previous_library_path:
            environment["LD_LIBRARY_PATH"] += os.pathsep + previous_library_path

        daemon = subprocess.Popen(
            [str(args.daemon), "--socket", str(socket_path)],
            env=environment,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        daemon_stdout = ""
        daemon_stderr = ""
        holder: subprocess.Popen[str] | None = None
        holder_stdout = ""
        holder_stderr = ""
        identity_parity: dict[str, dict[str, Any]] = {}
        try:
            wait_for_socket(socket_path, daemon)
            identity_parity = qualify_identity_parity(
                args.provider_dir, environment, args.deterministic
            )
            holder = start_holder(args.holder, environment)
            time.sleep(IDLE_UTILIZATION_SETTLE_SECONDS)
            results = []
            for family in EXPECTED_FAMILIES:
                binary = args.tools_root / "families" / family / "bin" / "nvidia-smi"
                results.append(
                    qualify_family(
                        family,
                        binary,
                        rows[family],
                        environment,
                        holder.pid,
                        args.holder.name,
                        args.deterministic,
                        args.elf_loader,
                    )
                )
            holder_pid = holder.pid
            holder_stdout, holder_stderr = stop_holder(holder)
            if holder.returncode != 0:
                raise RuntimeError(
                    f"CUDA process holder exited with {holder.returncode}\n"
                    f"stdout:\n{holder_stdout}\nstderr:\n{holder_stderr}"
                )
            holder = None
            for family, result in zip(EXPECTED_FAMILIES, results, strict=True):
                binary = args.tools_root / "families" / family / "bin" / "nvidia-smi"
                after_exit = qualify_holder_exit(
                    binary,
                    environment,
                    args.deterministic,
                    args.elf_loader,
                )
                result["after_holder_exit"] = after_exit
        finally:
            if holder is not None:
                holder_stdout, holder_stderr = stop_holder(holder)
            daemon_stdout, daemon_stderr = stop_daemon(daemon)

    if daemon.returncode != 0:
        raise RuntimeError(
            f"metafluxd exited with {daemon.returncode}\n"
            f"stdout:\n{daemon_stdout}\nstderr:\n{daemon_stderr}"
        )
    if args.source_date_epoch is not None:
        captured_at = dt.datetime.fromtimestamp(args.source_date_epoch, dt.UTC).isoformat()
    else:
        captured_at = dt.datetime.now(dt.UTC).isoformat()
    evidence = {
        "schema_version": 3,
        "captured_at": captured_at,
        "deterministic_projection": {
            "enabled": args.deterministic,
            "omitted_case_fields": (
                ["duration_ns", "stdout", "stderr"] if args.deterministic else []
            ),
            "omitted_top_level_fields": (
                [
                    "captured_at",
                    "holder_stdout",
                    "holder_stderr",
                    "daemon_stdout",
                    "daemon_stderr",
                ]
                if args.deterministic
                else []
            ),
            "omitted_time_fields": (
                ["captured_at", "duration_ns"] if args.deterministic else []
            ),
            "stream_hash_basis": (
                "holder-pid-and-nvidia-smi-timestamp-normalized-v1"
                if args.deterministic
                else "raw"
            ),
        },
        "case_matrix": {
            "per_family_active_holder": 10,
            "per_family_after_holder_exit": 2,
            "per_family_total": 12,
            "global_identity_parity": 2,
        },
        "locale": {"LC_ALL": "C", "LANG": "C", "timezone": "UTC"},
        "registry_fixture": {
            "kind": "metafluxd-default-m0100-single-device",
            "device_count": 1,
            "device": {
                "index": 0,
                "name": EXPECTED_NAME,
                "uuid": EXPECTED_UUID,
                "pci_bus_id": EXPECTED_PCI_BUS_ID,
                "memory_total_mib": 256,
            },
            "holder_allocation_mib": 4,
        },
        "manifest": str(args.manifest),
        "manifest_sha256": sha256(args.manifest),
        "daemon": str(args.daemon),
        "daemon_sha256": sha256(args.daemon),
        "provider_dir": str(args.provider_dir),
        "provider_artifacts": artifacts,
        "holder": str(args.holder),
        "holder_sha256": sha256(args.holder),
        "stock_elf_loader": (
            None
            if args.elf_loader is None
            else {"path": str(args.elf_loader), "sha256": sha256(args.elf_loader)}
        ),
        "cpu_topology": {
            "kind": "synthetic-test-fixture" if args.synthetic_cpu_topology else "host",
            "cpu_model": (
                "one-physical-core-per-affinity-cpu" if args.synthetic_cpu_topology else None
            ),
            "numa_model": "single-node-0" if args.synthetic_cpu_topology else None,
        },
        "identity_parity": identity_parity,
        "holder_stdout": holder_stdout,
        "holder_stderr": holder_stderr,
        "holder_stdout_sha256": sha256_text(holder_stdout),
        "holder_stderr_sha256": sha256_text(holder_stderr),
        "families": results,
        "daemon_stdout": daemon_stdout,
        "daemon_stderr": daemon_stderr,
        "daemon_stdout_sha256": sha256_text(daemon_stdout),
        "daemon_stderr_sha256": sha256_text(daemon_stderr),
    }
    if args.deterministic:
        evidence.pop("captured_at")
        project_top_level_streams(evidence, holder_pid)
        evidence = deterministic_evidence_projection(evidence, holder_pid)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    temporary_output = args.output.with_suffix(args.output.suffix + ".tmp")
    temporary_output.write_text(json.dumps(evidence, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    temporary_output.replace(args.output)
    print(f"stock nvidia-smi acceptance: {len(results)}/{len(EXPECTED_FAMILIES)} families passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
