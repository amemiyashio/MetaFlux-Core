#!/usr/bin/env python3
"""Run the pinned stock PyTorch CUDA baseline through the MetaFlux daemon.

Invoke this script from ``nix develop .#pytorch-baseline``. It is intentionally
outside the default development shell so ordinary CTest runs neither download
nor activate the stock PyTorch wheel.
"""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import time
from typing import Any


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_CLIENT_MANIFEST = REPOSITORY_ROOT / "toolchains" / "pytorch-cuda-clients-1.json"
DEFAULT_BASELINE_PTX = (
    REPOSITORY_ROOT
    / "plugins"
    / "compat"
    / "cuda"
    / "compiler"
    / "ptx"
    / "corpus"
    / "fixtures"
    / "positive-add-copy.ptx"
)
TRACE_LAUNCH = re.compile(r"^MF_LAUNCH ", re.MULTILINE)
TRACE_MODULE = re.compile(r"^MF_PYTORCH_BASELINE_MODULE ", re.MULTILINE)
TRACE_KERNEL_REQUEST = re.compile(
    r"^MF_PYTORCH_BASELINE_REQUEST profile=(?P<profile>[a-z0-9-]+) "
    r"operation=(?P<operation>[a-z0-9-]+) version=(?P<version>\d+) "
    r"kernel-ir=(?P<kernel_ir>\d+) lifetime=(?P<lifetime>[a-z0-9-]+)$",
    re.MULTILINE,
)
TRACE_SEMANTIC = re.compile(r"^MF_SEMANTIC ", re.MULTILINE)
TRACE_ENTRY = re.compile(r"^MF_ENTRY (?P<entry>[A-Za-z0-9_]+)$", re.MULTILINE)
TRACE_STUB_ENTRY = re.compile(r"^MF_STUB_CALL (?P<entry>[A-Za-z0-9_]+)$", re.MULTILINE)
TRACE_EXPORT_TABLE = re.compile(
    r"^MF_EXPORT_TABLE (?P<table>[0-9a-f]{8}(?:-[0-9a-f]{4}){3}-[0-9a-f]{12})$",
    re.MULTILINE,
)
TRACE_UNKNOWN_EXPORT_TABLE = re.compile(r"^MF_TABLE_UUID unknown ", re.MULTILINE)
TRACE_UNCLASSIFIED_TABLE_CALL = re.compile(
    r"^MF_TABLE_UNCLASSIFIED_CALL table=(?P<table>[a-z0-9]+) "
    r"image=(?P<image>[A-Za-z0-9._-]+) "
    r"caller-offset=(?P<caller_offset>0x[0-9a-f]+|unknown)$",
    re.MULTILINE,
)
TRACE_TABLE_SLOT_CALL = re.compile(
    r"^MF_TABLE_SLOT_CALL table=(?P<table>[a-z0-9]+) "
    r"slot=(?P<slot>\d+) behavior=(?P<behavior>[a-z0-9-]+)$",
    re.MULTILINE,
)
DAEMON_EXECUTION_STATISTICS = re.compile(
    r"^metafluxd: cpu-execution mode=(?P<mode>[a-z-]+) "
    r"compiler-requests=(?P<compiler_requests>\d+) "
    r"cache-hits=(?P<cache_hits>\d+) cache-misses=(?P<cache_misses>\d+) "
    r"loaded-modules=(?P<loaded_modules>\d+) ",
    re.MULTILINE,
)
BASELINE_DIRECT_PROVIDER_ENTRYPOINTS = frozenset(
    {
        "mf_cuda_managed_cuInit",
        "mf_cuda_managed_cuDevicePrimaryCtxRetain",
        "mf_cuda_managed_cuDevicePrimaryCtxGetState",
        "mf_cuda_managed_cuDevicePrimaryCtxRelease_v2",
        "mf_cuda_managed_cuCtxSetCurrent",
        "mf_cuda_managed_cuCtxGetCurrent",
    }
)
BASELINE_TYPED_STUB_ENTRYPOINTS = frozenset(
    {
        "mf_cuda_managed_cuCtxGetApiVersion",
        "mf_cuda_managed_cuCtxGetStreamPriorityRange",
        "mf_cuda_managed_cuGetExportTable",
    }
)
BASELINE_INTERNAL_TABLES = frozenset(
    {
        "a094798c-2e74-2e74-93f2-0800200c0a66",
        "42d85a81-23f6-cb47-8298-f6e78a3aecdc",
        "c693336e-1121-df11-a8c3-68f355d89593",
        "263e8860-7cd2-6143-92f6-bbd5006dfa7e",
        "d408" "2055-bde6-704b-8d34-ba123c66e1f2",
        "6bd5fb6c-5bf4-e74a-8987-d93912fd9df9",
    }
)
BASELINE_INTERNAL_TABLE_SLOT_CALLS = frozenset(
    {
        "c693:0:profile-observed-status-success",
        "c693:1:profile-observed-void-noop",
    }
)
BASELINE_KERNEL_REQUESTS = frozenset({"baseline:elementwise-add-i32:1:2:module-load"})


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--daemon", type=Path)
    parser.add_argument("--provider-dir", type=Path)
    parser.add_argument("--profile", default="baseline", choices=("baseline",))
    parser.add_argument("--client-manifest", type=Path, default=DEFAULT_CLIENT_MANIFEST)
    parser.add_argument(
        "--execution-mode",
        choices=("interpreter", "cold-jit", "warm-jit", "aot"),
        default="interpreter",
    )
    parser.add_argument("--ptx", type=Path, default=DEFAULT_BASELINE_PTX)
    parser.add_argument("--application", action="store_true")
    return parser.parse_args()


def load_profile(path: Path, profile_name: str) -> dict[str, str]:
    manifest = json.loads(path.read_text(encoding="utf-8"))
    profile = manifest["profiles"][profile_name]
    torch = profile.get("torch")
    version = profile.get("torch_version")
    if isinstance(torch, dict):
        nested_version = torch.get("version")
        if version is not None and nested_version is not None and version != nested_version:
            raise ValueError("torch_version and torch.version disagree")
        version = nested_version if version is None else version
    if not isinstance(version, str) or not version:
        raise ValueError("missing pinned torch version")
    return {
        "python": manifest["python"]["version"],
        "torch": version,
        "cuda": profile["cuda_version"],
    }


def source_provenance() -> dict[str, str]:
    revision_result = subprocess.run(
        ["git", "rev-parse", "HEAD"],
        cwd=REPOSITORY_ROOT,
        check=False,
        capture_output=True,
        text=True,
    )
    revision = revision_result.stdout.strip()
    if revision_result.returncode != 0 or not re.fullmatch(r"[0-9a-f]{40}", revision):
        raise RuntimeError("stock PyTorch baseline could not identify its source revision")

    status_result = subprocess.run(
        ["git", "status", "--porcelain"],
        cwd=REPOSITORY_ROOT,
        check=False,
        capture_output=True,
        text=True,
    )
    if status_result.returncode != 0:
        raise RuntimeError("stock PyTorch baseline could not inspect its source tree state")
    return {
        "revision": revision,
        "tree_state": "clean" if not status_result.stdout else "dirty",
    }


def result(stage_name: str, **details: Any) -> dict[str, Any]:
    return {"name": stage_name, "status": "passed", "details": details}


def provider_execution_surface(trace: str) -> dict[str, Any]:
    """Return only direct provider calls, never cuGetProcAddress probe names."""
    unclassified_table_calls = sorted(
        {
            f"{match.group('table')}@{match.group('image')}+{match.group('caller_offset')}"
            for match in TRACE_UNCLASSIFIED_TABLE_CALL.finditer(trace)
        }
    )
    table_slot_calls = sorted(
        {
            f"{match.group('table')}:{match.group('slot')}:{match.group('behavior')}"
            for match in TRACE_TABLE_SLOT_CALL.finditer(trace)
        }
    )
    kernel_requests = sorted(
        {
            ":".join(
                (
                    match.group("profile"),
                    match.group("operation"),
                    match.group("version"),
                    match.group("kernel_ir"),
                    match.group("lifetime"),
                )
            )
            for match in TRACE_KERNEL_REQUEST.finditer(trace)
        }
    )
    return {
        "kind": "direct-provider-entrypoints",
        "entrypoints": sorted(set(TRACE_ENTRY.findall(trace))),
        "typed_stub_entrypoints": sorted(set(TRACE_STUB_ENTRY.findall(trace))),
        "internal_tables": sorted(set(TRACE_EXPORT_TABLE.findall(trace))),
        "unknown_internal_table_requests": len(TRACE_UNKNOWN_EXPORT_TABLE.findall(trace)),
        "unclassified_internal_table_calls": unclassified_table_calls,
        "internal_table_slot_calls": table_slot_calls,
        "canonical_artifact_module_loads": len(TRACE_MODULE.findall(trace)),
        "kernel_requests": kernel_requests,
        "launches": len(TRACE_LAUNCH.findall(trace)),
        "local_semantic_execution_events": len(TRACE_SEMANTIC.findall(trace)),
    }


def daemon_execution_statistics(output: str) -> dict[str, int | str]:
    matches = list(DAEMON_EXECUTION_STATISTICS.finditer(output))
    if len(matches) != 1:
        raise RuntimeError("daemon did not emit exactly one CPU execution statistics record")
    fields = matches[0].groupdict()
    return {
        "mode": fields["mode"],
        "compiler_requests": int(fields["compiler_requests"]),
        "cache_hits": int(fields["cache_hits"]),
        "cache_misses": int(fields["cache_misses"]),
        "loaded_modules": int(fields["loaded_modules"]),
    }


def require_execution_statistics(
    statistics: dict[str, int | str],
    mode: str,
    compiler_requests: int,
    cache_hits: int,
    cache_misses: int,
    minimum_loaded_modules: int,
) -> None:
    expected = {
        "mode": mode,
        "compiler_requests": compiler_requests,
        "cache_hits": cache_hits,
        "cache_misses": cache_misses,
    }
    observed = {name: statistics[name] for name in expected}
    if observed != expected:
        raise RuntimeError(
            f"{mode} CPU execution statistics drifted: "
            f"expected {expected!r}, observed {observed!r}"
        )
    if int(statistics["loaded_modules"]) < minimum_loaded_modules:
        raise RuntimeError(
            f"{mode} loaded {statistics['loaded_modules']} modules; "
            f"expected at least {minimum_loaded_modules}"
        )


def cache_identity(cache_root: Path) -> str:
    identities: set[str] = set()
    for metadata_path in cache_root.rglob("metadata.v1"):
        for line in metadata_path.read_text(encoding="utf-8").splitlines():
            if line.startswith("cache_key="):
                identities.add(line.removeprefix("cache_key="))
    if len(identities) != 1:
        raise RuntimeError(
            "compiled baseline must materialize exactly one cache identity: "
            f"observed {sorted(identities)!r}"
        )
    identity = next(iter(identities))
    if re.fullmatch(r"mf-cache-v1-[0-9a-f]{64}", identity) is None:
        raise RuntimeError(f"compiled baseline cache identity is malformed: {identity!r}")
    return identity


def require_stable_gap(payload: dict[str, Any], expected_error: str) -> None:
    error = payload.get("error")
    if payload.get("result") != "gap" or not isinstance(error, str):
        raise RuntimeError(f"stock PyTorch did not return a structured gap: {payload!r}")
    first_line = error.splitlines()[0] if error else ""
    if first_line != expected_error:
        raise RuntimeError(
            f"stock PyTorch gap drifted: expected {expected_error!r}, observed {first_line!r}"
        )


def require_baseline_execution_surface(surface: dict[str, Any]) -> None:
    entrypoints = frozenset(surface["entrypoints"])
    if entrypoints != BASELINE_DIRECT_PROVIDER_ENTRYPOINTS:
        raise RuntimeError(
            "pinned baseline direct provider surface drifted: "
            f"expected {sorted(BASELINE_DIRECT_PROVIDER_ENTRYPOINTS)!r}, "
            f"observed {sorted(entrypoints)!r}"
        )
    typed_stub_entrypoints = frozenset(surface["typed_stub_entrypoints"])
    if typed_stub_entrypoints != BASELINE_TYPED_STUB_ENTRYPOINTS:
        raise RuntimeError(
            "pinned baseline typed-stub surface drifted: "
            f"expected {sorted(BASELINE_TYPED_STUB_ENTRYPOINTS)!r}, "
            f"observed {sorted(typed_stub_entrypoints)!r}"
        )
    tables = frozenset(surface["internal_tables"])
    if tables != BASELINE_INTERNAL_TABLES:
        raise RuntimeError(
            "pinned baseline CUDA internal-table surface drifted: "
            f"expected {sorted(BASELINE_INTERNAL_TABLES)!r}, observed {sorted(tables)!r}"
        )
    slot_calls = frozenset(surface["internal_table_slot_calls"])
    if slot_calls != BASELINE_INTERNAL_TABLE_SLOT_CALLS:
        raise RuntimeError(
            "pinned baseline CUDA internal-table slot calls drifted: "
            f"expected {sorted(BASELINE_INTERNAL_TABLE_SLOT_CALLS)!r}, "
            f"observed {sorted(slot_calls)!r}"
        )
    unclassified_table_calls = surface["unclassified_internal_table_calls"]
    if unclassified_table_calls:
        raise RuntimeError(
            "pinned baseline reached an unclassified CUDA internal-table slot: "
            f"observed {unclassified_table_calls!r}"
        )
    kernel_requests = frozenset(surface["kernel_requests"])
    if kernel_requests != BASELINE_KERNEL_REQUESTS:
        raise RuntimeError(
            "pinned baseline kernel-request contract drifted: "
            f"expected {sorted(BASELINE_KERNEL_REQUESTS)!r}, observed {sorted(kernel_requests)!r}"
        )


def select_baseline_cpu(affinity: set[int]) -> int:
    if not affinity:
        raise RuntimeError("stock PyTorch baseline has no effective CPU affinity")
    return min(affinity)


def pin_baseline_cpu() -> tuple[set[int], dict[str, int]]:
    original_affinity = os.sched_getaffinity(0)
    selected_cpu = select_baseline_cpu(original_affinity)
    os.sched_setaffinity(0, {selected_cpu})
    return original_affinity, {
        "original_cpu_count": len(original_affinity),
        "selected_cpu": selected_cpu,
    }


def stop_daemon(daemon: subprocess.Popen[str]) -> str:
    if daemon.poll() is None:
        daemon.terminate()
    try:
        output, _ = daemon.communicate(timeout=10)
    except subprocess.TimeoutExpired:
        daemon.kill()
        output, _ = daemon.communicate()
    return output


def device_tensor(torch: Any, values: list[int], device: str) -> Any:
    source = torch.tensor(values, dtype=torch.int32, device="cpu")
    destination = torch.empty_like(source, device=device)
    destination.copy_(source)
    return destination


def application(profile: dict[str, str]) -> dict[str, Any]:
    import torch

    observed = {
        "python": ".".join(str(component) for component in sys.version_info[:3]),
        "torch": str(torch.__version__),
        "cuda": str(torch.version.cuda),
    }
    if observed != profile:
        raise RuntimeError(f"pinned client mismatch: expected {profile!r}, observed {observed!r}")

    stages: list[dict[str, Any]] = [result("import", observed=observed)]
    count = int(torch._C._cuda_getDeviceCount())
    if count <= 0:
        raise RuntimeError(f"driver enumeration returned {count}")
    properties = torch.cuda.get_device_properties(0)
    torch.cuda.set_device(0)
    device = "cuda:0"
    stages.append(
        result(
            "driver-enumeration",
            api="torch._C._cuda_getDeviceCount",
            count=count,
            name=str(properties.name),
            compute_capability=f"{int(properties.major)}.{int(properties.minor)}",
        )
    )

    copy_values = [1, -2, 3, 0x01020304]
    copied = device_tensor(torch, copy_values, device)
    copy_roundtrip = torch.empty_like(copied, device="cpu")
    copy_roundtrip.copy_(copied)
    torch.cuda.synchronize(device)
    if [int(value) for value in copy_roundtrip.tolist()] != copy_values:
        raise RuntimeError("runtime copy result mismatch")
    stages.append(result("runtime-copy", bytes_each_direction=len(copy_values) * 4))

    architectures = sorted(str(value) for value in torch.cuda.get_arch_list())
    if "sm_70" not in architectures:
        raise RuntimeError(f"pinned wheel did not advertise sm_70: {architectures!r}")
    artifact_left = device_tensor(torch, [1], device)
    artifact_right = device_tensor(torch, [2], device)
    artifact_result = torch.add(artifact_left, artifact_right)
    torch.cuda.synchronize(device)
    if [int(value) for value in artifact_result.to("cpu").tolist()] != [3]:
        raise RuntimeError("artifact intake add result mismatch")
    stages.append(
        result(
            "artifact-intake",
            operation="stock torch.add activates the canonical PTX artifact",
            wheel_architectures=architectures,
        )
    )

    left = device_tensor(torch, [1, -2, 30, 400], device)
    right = device_tensor(torch, [5, 7, -10, 20], device)
    eager_result = torch.add(left, right)
    torch.cuda.synchronize(device)
    expected = [6, 5, 20, 420]
    observed_result = [int(value) for value in eager_result.to("cpu").tolist()]
    if observed_result != expected:
        raise RuntimeError(f"eager add mismatch: expected {expected!r}, observed {observed_result!r}")
    stages.append(result("eager-add", operation="stock torch.add int32", result=observed_result))
    return {"profile": profile, "stages": stages}


def wait_for_socket(daemon: subprocess.Popen[str], socket_path: Path) -> None:
    for _ in range(200):
        if socket_path.is_socket():
            return
        if daemon.poll() is not None:
            raise RuntimeError(f"metafluxd exited with {daemon.returncode}")
        time.sleep(0.02)
    raise RuntimeError(f"metafluxd did not create {socket_path}")


def runner(arguments: argparse.Namespace) -> dict[str, Any]:
    if arguments.daemon is None or arguments.provider_dir is None:
        raise ValueError("--daemon and --provider-dir are required for the runner")
    profile = load_profile(arguments.client_manifest, arguments.profile)
    daemon_path = arguments.daemon.resolve()
    provider_dir = arguments.provider_dir.resolve()
    if not daemon_path.is_file() or not os.access(daemon_path, os.X_OK):
        raise ValueError(f"daemon is not executable: {daemon_path}")
    if not (provider_dir / "libcuda.so.1").is_file():
        raise ValueError(f"provider directory has no libcuda.so.1: {provider_dir}")
    baseline_ptx = arguments.ptx.resolve()
    if not baseline_ptx.is_file():
        raise ValueError(f"baseline PTX is not a file: {baseline_ptx}")

    original_affinity, affinity = pin_baseline_cpu()
    try:
        return run_pinned_baseline(
            profile,
            daemon_path,
            provider_dir,
            affinity,
            arguments.profile,
            arguments.client_manifest,
            arguments.execution_mode,
            baseline_ptx,
        )
    finally:
        os.sched_setaffinity(0, original_affinity)


def run_pinned_baseline(
    profile: dict[str, str],
    daemon_path: Path,
    provider_dir: Path,
    affinity: dict[str, int],
    profile_name: str,
    client_manifest: Path,
    execution_mode: str,
    baseline_ptx: Path,
) -> dict[str, Any]:
    # The daemon binds its socket inside this directory; AF_UNIX sun_path holds
    # only 108 bytes, so the directory is anchored at /tmp to stay short at any
    # nix-develop TMPDIR nesting depth.
    with tempfile.TemporaryDirectory(prefix="metaflux-pytorch-stock-", dir="/tmp") as temporary:
        root = Path(temporary)
        cache_root = root / "compiler-cache"
        environment = os.environ.copy()
        environment.update(
            {
                "METAFLUX_MODE": "managed",
                "METAFLUX_COMPILER_CACHE": str(cache_root),
                "METAFLUX_TRACE_STUBS": "1",
            }
        )
        existing_library_path = environment.get("LD_LIBRARY_PATH")
        environment["LD_LIBRARY_PATH"] = str(provider_dir)
        if existing_library_path:
            environment["LD_LIBRARY_PATH"] += os.pathsep + existing_library_path

        def run_once(mode: str, label: str, expect_success: bool) -> dict[str, Any]:
            socket_path = root / f"metafluxd-{label}.sock"
            run_environment = environment.copy()
            run_environment.update(
                {"METAFLUX_SOCKET": str(socket_path), "METAFLUX_CPU_EXECUTION_MODE": mode}
            )
            daemon = subprocess.Popen(
                [str(daemon_path), "--socket", str(socket_path)],
                env=run_environment,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
            )
            daemon_output = ""
            try:
                wait_for_socket(daemon, socket_path)
                application_run = subprocess.run(
                    [
                        sys.executable,
                        "-B",
                        str(Path(__file__).resolve()),
                        "--application",
                        "--profile",
                        profile_name,
                        "--client-manifest",
                        str(client_manifest),
                    ],
                    check=False,
                    env=run_environment,
                    capture_output=True,
                    text=True,
                    timeout=120,
                )
            finally:
                daemon_output = stop_daemon(daemon)
            if daemon.returncode != 0:
                raise RuntimeError(
                    f"metafluxd {label} exited with {daemon.returncode}:\n{daemon_output}"
                )

            statistics = daemon_execution_statistics(daemon_output)
            payload = json.loads(application_run.stdout)
            surface = provider_execution_surface(application_run.stderr)
            if not expect_success:
                if application_run.returncode == 0:
                    raise RuntimeError(
                        f"stock PyTorch {label} did not report the expected stable gap:\n"
                        f"stdout:\n{application_run.stdout}\nstderr:\n{application_run.stderr}"
                    )
                require_stable_gap(payload, "CUDA error: operation not supported")
                if surface["local_semantic_execution_events"] != 0:
                    raise RuntimeError("failed provider path recorded forbidden local execution")
                return {
                    "application": payload,
                    "provider_surface": surface,
                    "statistics": statistics,
                }

            if application_run.returncode != 0:
                raise RuntimeError(
                    "stock PyTorch application failed:\n"
                    f"stdout:\n{application_run.stdout}\nstderr:\n{application_run.stderr}"
                )
            if surface["launches"] == 0:
                raise RuntimeError("provider trace did not record a CUDA launch")
            if surface["canonical_artifact_module_loads"] == 0:
                raise RuntimeError("provider trace did not record daemon-owned canonical module intake")
            if surface["unknown_internal_table_requests"] != 0:
                raise RuntimeError("provider trace requested an unclassified CUDA internal table")
            if surface["local_semantic_execution_events"] != 0:
                raise RuntimeError("provider trace recorded forbidden local semantic execution")
            require_baseline_execution_surface(surface)
            return {
                "application": payload,
                "provider_surface": surface,
                "statistics": statistics,
            }

        seed_evidence: dict[str, Any] | None = None
        miss_evidence: dict[str, Any] | None = None
        prewarm_evidence: dict[str, Any] | None = None
        if execution_mode == "interpreter":
            final_run = run_once("interpreter", "interpreter", True)
            require_execution_statistics(final_run["statistics"], "interpreter", 0, 0, 0, 1)
            compiled_cache_identity: str | None = None
        elif execution_mode == "cold-jit":
            final_run = run_once("cold-jit", "cold-jit", True)
            require_execution_statistics(final_run["statistics"], "cold-jit", 1, 0, 1, 1)
            compiled_cache_identity = cache_identity(cache_root)
        elif execution_mode == "warm-jit":
            seed_run = run_once("cold-jit", "warm-seed", True)
            require_execution_statistics(seed_run["statistics"], "cold-jit", 1, 0, 1, 1)
            seed_evidence = {"mode": "cold-jit", "statistics": seed_run["statistics"]}
            seeded_identity = cache_identity(cache_root)
            final_run = run_once("warm-jit", "warm-jit", True)
            require_execution_statistics(final_run["statistics"], "warm-jit", 0, 1, 0, 1)
            compiled_cache_identity = cache_identity(cache_root)
            if compiled_cache_identity != seeded_identity:
                raise RuntimeError("warm JIT did not reuse the cold JIT cache identity")
        else:
            miss_run = run_once("aot", "aot-miss", False)
            require_execution_statistics(miss_run["statistics"], "aot", 0, 0, 1, 0)
            miss_evidence = {
                "classification": "stable-not-supported",
                "statistics": miss_run["statistics"],
            }
            prewarm_environment = environment.copy()
            prewarm_environment["METAFLUX_CPU_EXECUTION_MODE"] = "aot"
            prewarm = subprocess.run(
                [str(daemon_path), "--prewarm-aot", str(baseline_ptx)],
                check=False,
                env=prewarm_environment,
                capture_output=True,
                text=True,
                timeout=120,
            )
            match = re.search(
                r"AOT prewarm compiled cache-key=(mf-cache-v1-[0-9a-f]{64})",
                prewarm.stdout,
            )
            if prewarm.returncode != 0 or match is None:
                raise RuntimeError(
                    f"stock PyTorch AOT prewarm failed ({prewarm.returncode}):\n"
                    f"stdout:\n{prewarm.stdout}\nstderr:\n{prewarm.stderr}"
                )
            prewarm_evidence = {"compiled": True, "cache_identity": match.group(1)}
            final_run = run_once("aot", "aot-hit", True)
            require_execution_statistics(final_run["statistics"], "aot", 0, 1, 0, 1)
            compiled_cache_identity = cache_identity(cache_root)
            if compiled_cache_identity != match.group(1):
                raise RuntimeError("AOT runtime did not reuse the prewarmed cache identity")

        application_report = final_run["application"]
        provider_surface = final_run["provider_surface"]
        daemon_statistics = final_run["statistics"]
        require_execution_statistics(
            daemon_statistics,
            execution_mode,
            int(daemon_statistics["compiler_requests"]),
            int(daemon_statistics["cache_hits"]),
            int(daemon_statistics["cache_misses"]),
            int(provider_surface["canonical_artifact_module_loads"]),
        )
        return {
            "schema_version": 3,
            "gate": "metaflux-pytorch-cuda-stock-baseline",
            "result": "complete",
            "runner": {"cpu_affinity": affinity, "execution_mode": execution_mode},
            "source": source_provenance(),
            "daemon": {
                "cpu_execution_mode": execution_mode,
                "socket": "private",
                "execution_statistics": daemon_statistics,
                "compiler": {
                    "input_identity": (
                        "not-applicable-in-interpreter-mode"
                        if execution_mode == "interpreter"
                        else "canonical-kernel-ir-v2"
                    ),
                    "runtime_requests": daemon_statistics["compiler_requests"],
                    "prewarm": prewarm_evidence,
                },
                "compiled_artifact_cache": {
                    "used": execution_mode != "interpreter",
                    "identity": (
                        "not-applicable-in-interpreter-mode"
                        if compiled_cache_identity is None
                        else compiled_cache_identity
                    ),
                    "seed": seed_evidence,
                    "required_miss": miss_evidence,
                },
            },
            "protocol": {
                "client_abi_version": 1,
                "kernel_request_registration": "MF_CLIENT_CONTROL_KERNEL_REQUEST_REGISTER_V1",
                "profile": "baseline",
                "operation": "elementwise-add-i32",
                "request_schema_version": 1,
                "kernel_ir_schema_version": 2,
                "request_lifetime": "module-load",
                "module_lifetime": "daemon-owned-canonical-kernel-ir",
            },
            "provider": {
                "managed_mode": True,
                "launch_seen": True,
                "canonical_artifact_module_loaded": True,
                "local_semantic_execution_seen": False,
                "execution_surface": provider_surface,
            },
            **application_report,
        }


def main() -> int:
    arguments = parse_arguments()
    try:
        if arguments.application:
            profile = load_profile(arguments.client_manifest, arguments.profile)
            print(json.dumps(application(profile), sort_keys=True, separators=(",", ":")))
        else:
            print(json.dumps(runner(arguments), sort_keys=True, separators=(",", ":")))
    except (OSError, RuntimeError, ValueError, json.JSONDecodeError) as error:
        print(json.dumps({"result": "gap", "error": str(error)}, sort_keys=True, separators=(",", ":")))
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
