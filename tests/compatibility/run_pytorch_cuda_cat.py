#!/usr/bin/env python3
"""Qualify pinned stock PyTorch contiguous dim-0 concat through MetaFlux."""

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


ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "toolchains" / "pytorch-cuda-clients-1.json"
REQUEST = "baseline:concat-u32:1:2:module-load"
REQUEST_PATTERN = re.compile(
    r"^MF_PYTORCH_BASELINE_REQUEST profile=(?P<profile>[a-z0-9-]+) "
    r"operation=(?P<operation>[a-z0-9-]+) version=(?P<version>\d+) "
    r"kernel-ir=(?P<kernel_ir>\d+) lifetime=(?P<lifetime>[a-z0-9-]+)$",
    re.MULTILINE,
)
STATISTICS_PATTERN = re.compile(
    r"^metafluxd: cpu-execution mode=(?P<mode>[a-z-]+) .*"
    r"loaded-modules=(?P<loaded_modules>\d+) .*"
    r"direct-host-source-operations=(?P<source_operations>\d+) .*"
    r"direct-host-destination-operations=(?P<destination_operations>\d+)",
    re.MULTILINE,
)


def arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--daemon", type=Path)
    parser.add_argument("--provider-dir", type=Path)
    parser.add_argument("--application", action="store_true")
    parser.add_argument(
        "--case",
        choices=(
            "positive",
            "unsupported-dimension",
            "unsupported-layout",
            "unsupported-dtype",
            "source-capacity",
        ),
        default="positive",
    )
    parser.add_argument("--client-manifest", type=Path, default=MANIFEST)
    return parser.parse_args()


def pinned_profile(path: Path) -> dict[str, str]:
    manifest = json.loads(path.read_text(encoding="utf-8"))
    profile = manifest["profiles"]["baseline"]
    torch = profile.get("torch", {})
    return {
        "python": manifest["python"]["version"],
        "torch": profile.get("torch_version", torch.get("version")),
        "cuda": profile["cuda_version"],
    }


def source_provenance() -> dict[str, str]:
    revision = subprocess.run(
        ["git", "rev-parse", "HEAD"],
        cwd=ROOT,
        check=False,
        capture_output=True,
        text=True,
    )
    tree = subprocess.run(
        ["git", "status", "--porcelain"],
        cwd=ROOT,
        check=False,
        capture_output=True,
        text=True,
    )
    value = revision.stdout.strip()
    if revision.returncode != 0 or not re.fullmatch(r"[0-9a-f]{40}", value):
        raise RuntimeError("concat gate could not identify its source revision")
    if tree.returncode != 0:
        raise RuntimeError("concat gate could not inspect its source tree state")
    return {"revision": value, "tree_state": "clean" if not tree.stdout else "dirty"}


def cuda_tensor(torch: Any, values: list[int] | list[float], dtype: Any | None = None) -> Any:
    source = torch.tensor(values, dtype=dtype or torch.int32, device="cpu")
    destination = torch.empty_like(source, device="cuda:0")
    destination.copy_(source)
    return destination


def application(case: str, profile_path: Path) -> dict[str, Any]:
    import torch

    expected_profile = pinned_profile(profile_path)
    observed_profile = {
        "python": ".".join(str(value) for value in sys.version_info[:3]),
        "torch": str(torch.__version__),
        "cuda": str(torch.version.cuda),
    }
    if observed_profile != expected_profile:
        raise RuntimeError(
            f"pinned client mismatch: expected {expected_profile!r}, observed {observed_profile!r}"
        )
    if int(torch._C._cuda_getDeviceCount()) <= 0:
        raise RuntimeError("driver enumeration returned no device")
    torch.cuda.set_device(0)

    if case == "positive":
        cases = (
            ([[1, 2, 3, 4], [5, 6, 7, 8]], [1, 2, 3, 4, 5, 6, 7, 8]),
            ([[1, 2], [3, 4, 5]], [1, 2, 3, 4, 5]),
            ([[1], [2, 3], [4, 5, 6]], [1, 2, 3, 4, 5, 6]),
        )
        observed: list[list[int]] = []
        for inputs, expected in cases:
            output = torch.cat([cuda_tensor(torch, values) for values in inputs], dim=0)
            torch.cuda.synchronize()
            result = [int(value) for value in output.to("cpu").tolist()]
            if result != expected:
                raise RuntimeError(f"concat mismatch: expected {expected!r}, observed {result!r}")
            observed.append(result)
        float_inputs = ([1.25, -2.5], [3.75])
        float_output = torch.cat(
            [cuda_tensor(torch, values, torch.float32) for values in float_inputs], dim=0
        )
        torch.cuda.synchronize()
        float_result = [float(value) for value in float_output.to("cpu").tolist()]
        float_expected = [1.25, -2.5, 3.75]
        if float_result != float_expected:
            raise RuntimeError(
                f"float32 concat mismatch: expected {float_expected!r}, observed {float_result!r}"
            )
        return {
            "result": "complete",
            "profile": observed_profile,
            "outputs": observed,
            "float32_output": float_result,
        }

    try:
        if case == "unsupported-dimension":
            left = cuda_tensor(torch, [1, 2, 3, 4]).reshape(2, 2)
            right = cuda_tensor(torch, [5, 6, 7, 8]).reshape(2, 2)
            torch.cat((left, right), dim=1)
        elif case == "unsupported-layout":
            left = cuda_tensor(torch, [1, 2, 3, 4]).reshape(2, 2).t()
            right = cuda_tensor(torch, [5, 6, 7, 8]).reshape(2, 2).t()
            torch.cat((left, right), dim=0)
        elif case == "unsupported-dtype":
            left = torch.tensor([1, 2], dtype=torch.int64, device="cuda:0")
            right = torch.tensor([3, 4], dtype=torch.int64, device="cuda:0")
            torch.cat((left, right), dim=0)
        else:
            torch.cat([cuda_tensor(torch, [index]) for index in range(32)], dim=0)
        torch.cuda.synchronize()
    except RuntimeError as error:
        message = str(error)
        if "CUDA error: operation not supported" not in message:
            raise RuntimeError(f"{case} returned the wrong CUDA error: {message}") from error
        return {"result": "expected-gap", "case": case, "error": "CUDA_ERROR_NOT_SUPPORTED"}
    raise RuntimeError(f"{case} unexpectedly succeeded")


def parse_provider_evidence(trace: str) -> dict[str, Any]:
    requests = [
        ":".join(match.group(key) for key in ("profile", "operation", "version", "kernel_ir", "lifetime"))
        for match in REQUEST_PATTERN.finditer(trace)
    ]
    return {
        "requests": requests,
        "launches": len(re.findall(r"^MF_LAUNCH ", trace, re.MULTILINE)),
        "local_execution": len(re.findall(r"^MF_SEMANTIC ", trace, re.MULTILINE)),
    }


def parse_daemon_statistics(output: str) -> dict[str, int | str]:
    matches = list(STATISTICS_PATTERN.finditer(output))
    if len(matches) != 1:
        raise RuntimeError("daemon did not emit exactly one complete execution record")
    values = matches[0].groupdict()
    return {key: (value if key == "mode" else int(value)) for key, value in values.items()}


def run_case(case: str, daemon_path: Path, provider_dir: Path, profile_path: Path) -> dict[str, Any]:
    # The daemon binds its socket inside this directory; AF_UNIX sun_path holds
    # only 108 bytes, so the directory is anchored at /tmp to stay short at any
    # nix-develop TMPDIR nesting depth.
    with tempfile.TemporaryDirectory(prefix=f"metaflux-pytorch-cat-{case}-", dir="/tmp") as temporary:
        socket_path = Path(temporary) / "metafluxd.sock"
        environment = os.environ.copy()
        environment.update(
            {
                "METAFLUX_MODE": "managed",
                "METAFLUX_SOCKET": str(socket_path),
                "METAFLUX_CPU_EXECUTION_MODE": "interpreter",
                "METAFLUX_TRACE_STUBS": "1",
                "LD_LIBRARY_PATH": str(provider_dir)
                + (os.pathsep + environment["LD_LIBRARY_PATH"] if environment.get("LD_LIBRARY_PATH") else ""),
            }
        )
        daemon = subprocess.Popen(
            [str(daemon_path), "--socket", str(socket_path)],
            env=environment,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )
        daemon_output = ""
        try:
            for _ in range(200):
                if socket_path.is_socket():
                    break
                if daemon.poll() is not None:
                    raise RuntimeError(f"metafluxd exited with {daemon.returncode}")
                time.sleep(0.02)
            else:
                raise RuntimeError("metafluxd did not create its socket")
            process = subprocess.run(
                [
                    sys.executable,
                    "-B",
                    str(Path(__file__).resolve()),
                    "--application",
                    "--case",
                    case,
                    "--client-manifest",
                    str(profile_path),
                ],
                env=environment,
                check=False,
                capture_output=True,
                text=True,
                timeout=120,
            )
        finally:
            if daemon.poll() is None:
                daemon.terminate()
            daemon_output, _ = daemon.communicate(timeout=10)
        if daemon.returncode != 0:
            raise RuntimeError(f"metafluxd {case} exited with {daemon.returncode}:\n{daemon_output}")
        if process.returncode != 0:
            raise RuntimeError(f"application {case} failed:\n{process.stdout}\n{process.stderr}")
        payload = json.loads(process.stdout)
        provider = parse_provider_evidence(process.stderr)
        statistics = parse_daemon_statistics(daemon_output)
        if provider["local_execution"] != 0 or provider["launches"] == 0:
            raise RuntimeError(f"provider evidence drifted for {case}: {provider!r}")
        if case == "positive":
            if payload.get("result") != "complete" or provider["requests"] != [REQUEST]:
                raise RuntimeError(f"positive concat evidence drifted: {payload!r}, {provider!r}")
            if statistics["loaded_modules"] != 1 or statistics["destination_operations"] < 4:
                raise RuntimeError(f"daemon did not complete all concat launches: {statistics!r}")
        else:
            if payload.get("result") != "expected-gap" or provider["requests"]:
                raise RuntimeError(f"negative concat evidence drifted: {payload!r}, {provider!r}")
            if statistics["loaded_modules"] != 0 or statistics["destination_operations"] != 0:
                raise RuntimeError(f"rejected concat reached daemon execution: {statistics!r}")
        return {"application": payload, "provider": provider, "daemon": statistics}


def runner(parsed: argparse.Namespace) -> dict[str, Any]:
    if parsed.daemon is None or parsed.provider_dir is None:
        raise ValueError("--daemon and --provider-dir are required")
    daemon_path = parsed.daemon.resolve()
    provider_dir = parsed.provider_dir.resolve()
    if not daemon_path.is_file() or not (provider_dir / "libcuda.so.1").is_file():
        raise ValueError("daemon or provider artifact is missing")
    # Match the stock baseline/frontier's controlled placement before importing
    # the client in any child. Preserve import failures rather than retrying them.
    original_affinity = os.sched_getaffinity(0)
    if not original_affinity:
        raise RuntimeError("stock PyTorch concat has no effective CPU affinity")
    selected_cpu = min(original_affinity)
    os.sched_setaffinity(0, {selected_cpu})
    try:
        return {
            "schema_version": 1,
            "gate": "metaflux-pytorch-cuda-concat",
            "result": "complete",
            "source": source_provenance(),
            "affinity": {"original_cpu_count": len(original_affinity), "selected_cpu": selected_cpu},
            "cases": {
                case: run_case(case, daemon_path, provider_dir, parsed.client_manifest.resolve())
                for case in (
                    "positive",
                    "unsupported-dimension",
                    "unsupported-layout",
                    "unsupported-dtype",
                    "source-capacity",
                )
            },
        }
    finally:
        os.sched_setaffinity(0, original_affinity)


def main() -> int:
    parsed = arguments()
    try:
        payload = application(parsed.case, parsed.client_manifest) if parsed.application else runner(parsed)
        print(json.dumps(payload, sort_keys=True, separators=(",", ":")))
        return 0
    except (OSError, RuntimeError, ValueError, json.JSONDecodeError) as error:
        print(json.dumps({"result": "gap", "error": str(error)}, sort_keys=True, separators=(",", ":")))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
