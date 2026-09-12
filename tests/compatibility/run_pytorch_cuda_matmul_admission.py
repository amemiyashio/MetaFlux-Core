#!/usr/bin/env python3
"""Exercise stock PyTorch matmul profile rejection before and after module reuse."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
from typing import Any

import run_pytorch_cuda_cpu_frontier as frontier
import run_pytorch_cuda_stock_baseline as baseline


PROFILES = ("matmul-f32", "matmul-rect-f32", "linear-no-bias-f32", "addmm-f32", "linear-bias-f32")
SEQUENCES = ("matmul-fresh", "matmul-reuse", "linear-bias-fresh", "linear-bias-reuse", "all-profiles")
REQUEST = "baseline:matmul-f32:1:2:module-load"
PHASE_PATTERN = re.compile(
    r"^MF_ADMISSION_BEGIN (?P<phase>[a-z0-9-]+)\n(?P<trace>.*?)"
    r"^MF_ADMISSION_END (?P=phase)$", re.MULTILINE | re.DOTALL
)
LT_FALLBACK_PATTERN = re.compile(
    r"gemm_and_bias error: CUBLAS_STATUS_NOT_SUPPORTED when calling cublasLtMatmul\b"
    r"[^\n]*\. Will attempt to recover by calling unfused cublas path\."
)


def arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--daemon", type=Path)
    parser.add_argument("--provider-dir", type=Path)
    parser.add_argument("--cublas-provider", type=Path)
    parser.add_argument("--client-manifest", type=Path, default=baseline.DEFAULT_CLIENT_MANIFEST)
    parser.add_argument("--execution-mode", choices=frontier.EXECUTION_MODES, default="interpreter")
    parser.add_argument("--sequence", choices=SEQUENCES)
    parser.add_argument("--application", action="store_true")
    return parser.parse_args()


def application(sequence: str, manifest: Path) -> dict[str, Any]:
    import torch

    profile = baseline.load_profile(manifest, "baseline")
    observed = {
        "python": ".".join(str(value) for value in sys.version_info[:3]),
        "torch": str(torch.__version__),
        "cuda": str(torch.version.cuda),
    }
    if observed != profile:
        raise RuntimeError(f"pinned client mismatch: expected {profile!r}, observed {observed!r}")
    if int(torch._C._cuda_getDeviceCount()) <= 0:
        raise RuntimeError("driver enumeration returned no device")
    torch.cuda.set_device(0)
    if sequence == "all-profiles":
        corpus = frontier.load_corpus(frontier.CORPUS, manifest)
        entries = {entry["id"]: entry for entry in corpus["cases"]}
        operations = frontier.operation_cases(torch)
        phases = {}
        for name in PROFILES:
            for repetition in ("before", "after"):
                phase = f"{name}-{repetition}"
                print(f"MF_ADMISSION_BEGIN {phase}", file=sys.stderr, flush=True)
                output = operations[name]()
                torch.cuda.synchronize()
                oracle = entries[name]["oracle"]
                value = frontier.observed_value(torch, output, oracle["kind"])
                if value != oracle["expected"]:
                    raise RuntimeError(f"{phase}: expected {oracle['expected']!r}, observed {value!r}")
                phases[phase] = {"result": "complete", "observed": value}
                print(f"MF_ADMISSION_END {phase}", file=sys.stderr, flush=True)
        return {"result": "complete", "pid": os.getpid(), "profile": observed, "phases": phases}
    f32 = lambda values: frontier.cuda_tensor(torch, values, torch.float32)
    is_linear = sequence.startswith("linear-bias")
    reuse = sequence.endswith("reuse")
    if is_linear:
        left = f32([[1.0, 2.0, 3.0], [4.0, 5.0, 6.0]])
        right = f32([[1.0, 2.0, 3.0], [4.0, 5.0, 6.0]])
        bias = f32([0.5, -0.5])
        expected = ["0x41680000", "0x41fc0000", "0x42020000", "0x42990000"]
        supported = lambda: torch.nn.functional.linear(left, right, bias)
    else:
        left = f32([[1.0, 2.0], [3.0, 4.0]])
        right = f32([[5.0, 6.0], [7.0, 8.0]])
        expected = ["0x41980000", "0x41b00000", "0x422c0000", "0x42480000"]
        supported = lambda: torch.matmul(left, right)
    other_left = f32([[1.0, 2.0, 3.0], [4.0, 5.0, 6.0], [7.0, 8.0, 9.0]])
    other_right = f32([[9.0, 8.0, 7.0], [6.0, 5.0, 4.0], [3.0, 2.0, 1.0]])
    if is_linear:
        other_bias = f32([0.5, -0.5, 1.0])
        unsupported = lambda: torch.nn.functional.linear(other_left, other_right, other_bias)
    else:
        unsupported = lambda: torch.matmul(other_left, other_right)
    torch.cuda.synchronize()

    phases: dict[str, Any] = {}
    for phase in (("before", "reject", "after") if reuse else ("reject",)):
        print(f"MF_ADMISSION_BEGIN {phase}", file=sys.stderr, flush=True)
        if phase == "reject":
            try:
                unsupported()
                torch.cuda.synchronize()
            except RuntimeError as error:
                message = str(error)
                if is_linear:
                    correct_error = (
                        message.partition("\n")[0] == "CUDA error: operation not supported"
                        and "cudaErrorNotSupported" in message
                    )
                    expected_error = "cudaErrorNotSupported"
                else:
                    correct_error = "CUBLAS_STATUS_NOT_SUPPORTED" in message
                    expected_error = "CUBLAS_STATUS_NOT_SUPPORTED"
                if not correct_error:
                    raise RuntimeError(f"{sequence} returned the wrong rejection: {error}") from error
                # Rejection is synchronous and leaves the existing context usable.
                torch.cuda.synchronize()
                phases[phase] = {"result": "rejected", "error": expected_error}
            else:
                raise RuntimeError(f"{sequence}: unsupported 3x3 operation returned success")
        else:
            output = supported()
            torch.cuda.synchronize()
            bits = frontier.observed_value(torch, output, "f32-bits")
            if bits != expected:
                raise RuntimeError(f"{sequence}/{phase}: expected {expected!r}, observed {bits!r}")
            phases[phase] = {"result": "complete", "bits": bits}
        print(f"MF_ADMISSION_END {phase}", file=sys.stderr, flush=True)
    return {"result": "complete", "pid": os.getpid(), "profile": observed, "phases": phases}


def qualify(
    sequence: str, mode: str, payload: dict[str, Any], trace: str, daemon_output: str
) -> dict[str, Any]:
    reuse = sequence.endswith("reuse")
    all_profiles = sequence == "all-profiles"
    is_linear = sequence.startswith("linear-bias")
    rejected_attempts = 0 if all_profiles else (2 if is_linear else 1)
    expected_phases = (
        [f"{name}-{part}" for name in PROFILES for part in ("before", "after")]
        if all_profiles else (["before", "reject", "after"] if reuse else ["reject"])
    )
    matches = list(PHASE_PATTERN.finditer(trace))
    if [match["phase"] for match in matches] != expected_phases:
        raise RuntimeError(f"{sequence}: missing or reordered phase markers")
    phase_evidence = {
        match["phase"]: frontier.parse_provider_evidence(match["trace"]) for match in matches
    }
    if not all_profiles:
        rejected = phase_evidence["reject"]
        rejection_trace = next(match["trace"] for match in matches if match["phase"] == "reject")
        fallback_warnings = LT_FALLBACK_PATTERN.findall(rejection_trace)
        if len(fallback_warnings) != int(is_linear):
            raise RuntimeError(f"{sequence}: stock cuBLASLt fallback warning drifted: {fallback_warnings!r}")
        expected_error = "cudaErrorNotSupported" if is_linear else "CUBLAS_STATUS_NOT_SUPPORTED"
        if payload.get("phases", {}).get("reject") != {"result": "rejected", "error": expected_error}:
            raise RuntimeError(f"{sequence}: rejected error evidence drifted: {payload!r}")
        rejected["stock_fallback_warnings"] = fallback_warnings
        if any(rejected[key] for key in ("module_loads", "warm_cache_hits", "library_calls", "local_execution")):
            raise RuntimeError(f"rejected {sequence} reached materialization or execution: {rejected!r}")
        # Initial cuBLAS artifact registration is allowed; no executable module
        # may be loaded. MF_LAUNCH is an API attempt, before admission.
        allowed_requests = [REQUEST] if not reuse else []
        if rejected["requests"] != allowed_requests or rejected["launches"] != rejected_attempts:
            raise RuntimeError(f"rejection request/attempt evidence drifted: {rejected!r}")
    provider = frontier.parse_provider_evidence(trace)
    successful_calls = 10 if all_profiles else (2 if reuse else 0)
    loaded_modules = 5 if all_profiles else int(reuse)
    expected_library = "lt-matmul-bias-f32" if sequence.startswith("linear-bias") else "sgemm-f32"
    library_calls = (["sgemm-f32"] * 8 + ["lt-matmul-bias-f32"] * 2
                     if all_profiles else [expected_library] * successful_calls)
    if (
        payload.get("result") != "complete"
        or provider["requests"] != [REQUEST] * (2 if all_profiles else 1)
        or provider["library_calls"] != library_calls
        or provider["local_execution"] != 0
        or provider["module_loads"] != loaded_modules
        or provider["warm_cache_hits"] != loaded_modules
        or provider["launches"] != successful_calls + rejected_attempts
    ):
        raise RuntimeError(f"{sequence}: total provider evidence drifted: {provider!r}")
    if successful_calls:
        pairs = ([(f"{name}-before", f"{name}-after") for name in PROFILES]
                 if all_profiles else [("before", "after")])
        for before, after in pairs:
            if phase_evidence[before]["module_loads"] != 1 or phase_evidence[after]["warm_cache_hits"] != 1:
                raise RuntimeError(f"{sequence}: materialization/warm reuse drifted at {before}/{after}")
        # This helper expects successful launch count. Library success records
        # count those calls; MF_LAUNCH also includes the rejected API attempt.
        execution_case = {"application": payload, "provider": {"launches": len(provider["library_calls"])}}
        frontier.qualify_executors(
            daemon_output,
            [{"id": sequence, "compiled": {}, "expected_requests": [REQUEST], "repetitions": successful_calls}],
            {sequence: execution_case}, mode,
        )
        executions = execution_case["daemon_executions"]
    else:
        executions = []
    if len(re.findall(r"^MF_CPU_EXECUTION ", daemon_output, re.MULTILINE)) != successful_calls:
        raise RuntimeError(f"{sequence}: rejected call produced an extra CPU completion")
    statistics = frontier.parse_daemon_statistics(daemon_output)
    compiler_requests = loaded_modules if mode == "cold-jit" else 0
    cache_hits = loaded_modules if mode in ("warm-jit", "aot") else 0
    frontier.require_execution_statistics(
        statistics, mode, compiler_requests, cache_hits, compiler_requests, loaded_modules
    )
    return {
        "application": payload,
        "phases": phase_evidence,
        "launch_attempts": provider["launches"],
        "successful_library_calls": successful_calls,
        "executor_completions": executions,
        "daemon": statistics,
    }


def run_sequence(sequence: str, parsed: argparse.Namespace) -> dict[str, Any]:
    # /tmp keeps Unix socket names below sun_path even with nested Nix shells.
    with tempfile.TemporaryDirectory(prefix="mf-matmul-admission-", dir="/tmp") as temporary:
        socket_path = Path(temporary) / "daemon.sock"
        cache_root = Path(temporary) / "cache"
        prewarm = []
        if parsed.execution_mode in ("warm-jit", "aot"):
            source_ids = (PROFILES if sequence == "all-profiles" else
                          (("linear-bias-f32" if sequence.startswith("linear-bias") else "matmul-f32",)
                           if sequence.endswith("reuse") else ()))
            corpus = frontier.load_corpus(frontier.CORPUS, parsed.client_manifest)
            entries = {entry["id"]: entry for entry in corpus["cases"]}
            prewarm = [frontier.prewarm_aot(parsed.daemon.resolve(), cache_root, frontier.compiled_ptx(entries[name]))
                       for name in source_ids]
        environment = os.environ.copy()
        environment.update({
            "METAFLUX_MODE": "managed",
            "METAFLUX_SOCKET": str(socket_path),
            "METAFLUX_CPU_EXECUTION_MODE": parsed.execution_mode,
            "METAFLUX_COMPILER_CACHE": str(cache_root),
            "METAFLUX_TRACE_STUBS": "1",
            "METAFLUX_TRACE_EXECUTION": "1",
            "LD_LIBRARY_PATH": str(parsed.provider_dir.resolve()) + (
                os.pathsep + environment["LD_LIBRARY_PATH"] if environment.get("LD_LIBRARY_PATH") else ""
            ),
            "LD_PRELOAD": str(parsed.cublas_provider.resolve()),
        })
        daemon = subprocess.Popen(
            [str(parsed.daemon.resolve()), "--socket", str(socket_path)], env=environment,
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
        )
        failure: Exception | None = None
        process = None
        try:
            frontier.wait_for_socket(daemon, socket_path)
            process = subprocess.run(
                [sys.executable, "-B", str(Path(__file__).resolve()), "--application",
                 "--sequence", sequence, "--client-manifest", str(parsed.client_manifest.resolve())],
                env=environment, capture_output=True, text=True, check=False, timeout=90,
            )
        except Exception as error:
            failure = error
        finally:
            daemon_output = baseline.stop_daemon(daemon)
        if failure is not None:
            raise RuntimeError(f"{failure}\nmetafluxd output:\n{daemon_output}") from failure
        assert process is not None
        try:
            if daemon.returncode != 0 or process.returncode != 0:
                raise RuntimeError(f"{sequence}: application={process.returncode}, daemon={daemon.returncode}")
            result = qualify(sequence, parsed.execution_mode, json.loads(process.stdout), process.stderr, daemon_output)
            result["prewarm"] = prewarm
            return result
        except (RuntimeError, ValueError) as error:
            raise RuntimeError(
                f"{error}\napplication stdout:\n{process.stdout}\napplication stderr:\n{process.stderr}"
                f"\nmetafluxd output:\n{daemon_output}"
            ) from error


def runner(parsed: argparse.Namespace) -> dict[str, Any]:
    if parsed.daemon is None or parsed.provider_dir is None or parsed.cublas_provider is None:
        raise ValueError("--daemon, --provider-dir and --cublas-provider are required")
    if not parsed.daemon.is_file() or not (parsed.provider_dir / "libcuda.so.1").is_file() or not parsed.cublas_provider.is_file():
        raise ValueError("daemon or provider artifact is missing")
    affinity, placement = baseline.pin_baseline_cpu()
    try:
        return {
            "schema_version": 1,
            "gate": "metaflux-pytorch-cuda-matmul-admission",
            "result": "complete",
            "source": baseline.source_provenance(),
            "execution_mode": parsed.execution_mode,
            "cpu_affinity": placement,
            "sequences": {
                sequence: run_sequence(sequence, parsed)
                for sequence in ((parsed.sequence,) if parsed.sequence else SEQUENCES)
            },
        }
    finally:
        os.sched_setaffinity(0, affinity)


def main() -> int:
    parsed = arguments()
    try:
        if parsed.application and parsed.sequence is None:
            raise ValueError("--application requires --sequence")
        payload = application(parsed.sequence, parsed.client_manifest) if parsed.application else runner(parsed)
        print(json.dumps(payload, sort_keys=True, separators=(",", ":")))
        return 0
    except (OSError, RuntimeError, ValueError) as error:
        print(json.dumps({"result": "gap", "error": str(error)}, sort_keys=True, separators=(",", ":")))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
