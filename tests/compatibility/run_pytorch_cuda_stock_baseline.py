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
TRACE_LAUNCH = re.compile(r"^MF_LAUNCH ", re.MULTILINE)
TRACE_MODULE = re.compile(r"^MF_PYTORCH_BASELINE_MODULE ", re.MULTILINE)
TRACE_SEMANTIC = re.compile(r"^MF_SEMANTIC ", re.MULTILINE)


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--daemon", type=Path)
    parser.add_argument("--provider-dir", type=Path)
    parser.add_argument("--profile", default="baseline", choices=("baseline",))
    parser.add_argument("--client-manifest", type=Path, default=DEFAULT_CLIENT_MANIFEST)
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


def result(stage_name: str, **details: Any) -> dict[str, Any]:
    return {"name": stage_name, "status": "passed", "details": details}


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

    with tempfile.TemporaryDirectory(prefix="metaflux-pytorch-stock-") as temporary:
        root = Path(temporary)
        socket_path = root / "metafluxd.sock"
        environment = os.environ.copy()
        environment.update(
            {
                "METAFLUX_SOCKET": str(socket_path),
                "METAFLUX_MODE": "managed",
                "METAFLUX_CPU_EXECUTION_MODE": "interpreter",
                "METAFLUX_COMPILER_CACHE": str(root / "compiler-cache"),
                "METAFLUX_TRACE_STUBS": "1",
            }
        )
        existing_library_path = environment.get("LD_LIBRARY_PATH")
        environment["LD_LIBRARY_PATH"] = str(provider_dir)
        if existing_library_path:
            environment["LD_LIBRARY_PATH"] += os.pathsep + existing_library_path

        daemon = subprocess.Popen(
            [str(daemon_path), "--socket", str(socket_path)],
            env=environment,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )
        try:
            wait_for_socket(daemon, socket_path)
            application_run = subprocess.run(
                [sys.executable, "-B", str(Path(__file__).resolve()), "--application",
                 "--profile", arguments.profile, "--client-manifest", str(arguments.client_manifest)],
                check=False,
                env=environment,
                capture_output=True,
                text=True,
            )
            if application_run.returncode != 0:
                raise RuntimeError(
                    "stock PyTorch application failed:\n"
                    f"stdout:\n{application_run.stdout}\nstderr:\n{application_run.stderr}"
                )
            application_report = json.loads(application_run.stdout)
            trace = application_run.stderr
            if TRACE_LAUNCH.search(trace) is None:
                raise RuntimeError("provider trace did not record a CUDA launch")
            if TRACE_MODULE.search(trace) is None:
                raise RuntimeError("provider trace did not record daemon-owned canonical module intake")
            if TRACE_SEMANTIC.search(trace) is not None:
                raise RuntimeError("provider trace recorded forbidden local semantic execution")
            return {
                "schema_version": 1,
                "gate": "metaflux-pytorch-cuda-stock-baseline",
                "result": "complete",
                "daemon": {"cpu_execution_mode": "interpreter", "socket": "private"},
                "provider": {
                    "managed_mode": True,
                    "launch_seen": True,
                    "canonical_artifact_module_loaded": True,
                    "local_semantic_execution_seen": False,
                },
                **application_report,
            }
        finally:
            if daemon.poll() is None:
                daemon.terminate()
                try:
                    daemon.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    daemon.kill()
                    daemon.wait(timeout=10)


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
