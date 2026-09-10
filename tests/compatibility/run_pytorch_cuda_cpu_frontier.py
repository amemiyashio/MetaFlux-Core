#!/usr/bin/env python3
"""Qualify the observed stock-PyTorch CUDA CPU operator frontier."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile
import time
from typing import Any, Callable


ROOT = Path(__file__).resolve().parents[2]
CORPUS = Path(__file__).with_name("pytorch_cuda_cpu_frontier_corpus_v1.json")
CLIENT_MANIFEST = ROOT / "toolchains" / "pytorch-cuda-clients-1.json"
EXECUTION_MODES = ("interpreter", "cold-jit", "warm-jit", "aot")
STABLE_AOT_MISS = "CUDA error: operation not supported"
REQUEST_PATTERN = re.compile(
    r"^MF_PYTORCH_BASELINE_REQUEST profile=(?P<profile>[a-z0-9-]+) "
    r"operation=(?P<operation>[a-z0-9-]+) version=(?P<version>\d+) "
    r"kernel-ir=(?P<kernel_ir>\d+) lifetime=(?P<lifetime>[a-z0-9-]+)$",
    re.MULTILINE,
)
STATISTICS_PATTERN = re.compile(
    r"^metafluxd: cpu-execution mode=(?P<mode>[a-z-]+) "
    r"compiler-requests=(?P<compiler_requests>\d+) "
    r"cache-hits=(?P<cache_hits>\d+) cache-misses=(?P<cache_misses>\d+) "
    r"loaded-modules=(?P<loaded_modules>\d+) .*"
    r"direct-host-source-operations=(?P<source_operations>\d+) .*"
    r"direct-host-destination-operations=(?P<destination_operations>\d+)",
    re.MULTILINE,
)
CUBLAS_REQUEST_PATTERN = re.compile(
    r"^MF_CUBLAS_REQUEST operation=(?P<operation>[a-z0-9-]+) ", re.MULTILINE
)


def arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--daemon", type=Path)
    parser.add_argument("--provider-dir", type=Path)
    parser.add_argument("--cublas-provider", type=Path)
    parser.add_argument("--application", action="store_true")
    parser.add_argument("--case")
    parser.add_argument("--compiled-subset", action="store_true")
    parser.add_argument("--execution-mode", choices=EXECUTION_MODES, default="interpreter")
    parser.add_argument("--corpus", type=Path, default=CORPUS)
    parser.add_argument("--client-manifest", type=Path, default=CLIENT_MANIFEST)
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


def compiled_ptx(entry: dict[str, Any]) -> Path:
    compiled = entry.get("compiled")
    if not isinstance(compiled, dict) or set(compiled) != {"ptx"}:
        raise ValueError(f"CPU frontier case {entry.get('id')!r} has no compiled PTX identity")
    relative = compiled["ptx"]
    if not isinstance(relative, str) or not relative or Path(relative).is_absolute():
        raise ValueError(f"CPU frontier case {entry.get('id')!r} has invalid compiled PTX path")
    path = (ROOT / relative).resolve()
    try:
        path.relative_to(ROOT)
    except ValueError as error:
        raise ValueError(
            f"CPU frontier case {entry.get('id')!r} compiled PTX escapes the repository"
        ) from error
    if not path.is_file():
        raise ValueError(f"CPU frontier compiled PTX is not a file: {path}")
    return path


def load_corpus(path: Path, profile_path: Path) -> dict[str, Any]:
    corpus = json.loads(path.read_text(encoding="utf-8"))
    if corpus.get("schema_version") != 1 or corpus.get("status") != "frontier-not-frozen":
        raise ValueError("CPU frontier corpus identity or status drifted")
    if corpus.get("execution_modes") != list(EXECUTION_MODES):
        raise ValueError("CPU frontier execution-mode matrix drifted")
    profile = pinned_profile(profile_path)
    if corpus.get("client") != profile:
        raise ValueError(
            f"CPU frontier client drifted: expected {profile!r}, observed {corpus.get('client')!r}"
        )
    cases = corpus.get("cases")
    gaps = corpus.get("gaps")
    if not isinstance(cases, list) or not isinstance(gaps, list) or not cases:
        raise ValueError("CPU frontier corpus must contain supported cases and a gap list")
    identifiers = [entry.get("id") for entry in cases + gaps]
    if any(not isinstance(identifier, str) or not identifier for identifier in identifiers):
        raise ValueError("CPU frontier case has no stable identifier")
    if len(identifiers) != len(set(identifiers)):
        raise ValueError("CPU frontier case identifiers must be unique")
    for entry in cases:
        requests = entry.get("expected_requests")
        if not isinstance(requests, list) or not requests:
            raise ValueError(f"supported case {entry['id']} has no neutral request")
        if entry.get("repetitions", 1) < 1:
            raise ValueError(f"supported case {entry['id']} has invalid repetitions")
        library_calls = entry.get("expected_library_calls", [])
        if not isinstance(library_calls, list) or any(
            not isinstance(call, str) or not call for call in library_calls
        ):
            raise ValueError(f"supported case {entry['id']} has invalid library-call evidence")
        if "compiled" in entry:
            compiled_ptx(entry)
            if len(requests) != 1:
                raise ValueError(
                    f"compiled case {entry['id']} must map to exactly one neutral request"
                )
    for entry in gaps:
        if entry.get("status") != "frontier-gap" or not entry.get("expected_error"):
            raise ValueError(f"gap {entry['id']} lacks a stable error classification")
    compiled_identifiers = [entry["id"] for entry in cases if "compiled" in entry]
    scope = corpus.get("scope")
    if not isinstance(scope, dict) or scope.get("compiled_subset") != compiled_identifiers:
        raise ValueError("CPU frontier compiled-subset identity drifted")
    return corpus


def source_provenance() -> dict[str, str]:
    revision = subprocess.run(
        ["git", "rev-parse", "HEAD"], cwd=ROOT, check=False, capture_output=True, text=True
    )
    tree = subprocess.run(
        ["git", "status", "--porcelain"], cwd=ROOT, check=False, capture_output=True, text=True
    )
    value = revision.stdout.strip()
    if revision.returncode != 0 or re.fullmatch(r"[0-9a-f]{40}", value) is None:
        raise RuntimeError("CPU frontier gate could not identify its source revision")
    if tree.returncode != 0:
        raise RuntimeError("CPU frontier gate could not inspect its source tree state")
    return {"revision": value, "tree_state": "clean" if not tree.stdout else "dirty"}


def pin_client_cpu() -> tuple[set[int], dict[str, int]]:
    original_affinity = os.sched_getaffinity(0)
    if not original_affinity:
        raise RuntimeError("stock PyTorch frontier has no effective CPU affinity")
    selected_cpu = min(original_affinity)
    os.sched_setaffinity(0, {selected_cpu})
    return original_affinity, {
        "original_cpu_count": len(original_affinity),
        "selected_cpu": selected_cpu,
    }


def cuda_tensor(torch: Any, values: Any, dtype: Any) -> Any:
    source = torch.tensor(values, dtype=dtype, device="cpu")
    destination = torch.empty_like(source, device="cuda:0")
    destination.copy_(source)
    return destination


def operation_cases(torch: Any) -> dict[str, Callable[[], Any]]:
    left_i32 = [12, -7, 100, 0, -999, 5]
    right_i32 = [3, 4, -6, 1, 2, -3]
    i32 = lambda values: cuda_tensor(torch, values, torch.int32)
    f32 = lambda values: cuda_tensor(torch, values, torch.float32)
    f64 = lambda values: cuda_tensor(torch, values, torch.float64)
    negative_zero = struct.unpack("<f", struct.pack("<I", 0x80000000))[0]
    quiet_nan = struct.unpack("<f", struct.pack("<I", 0x7FC00000))[0]
    return {
        "add-i32": lambda: torch.add(i32(left_i32), i32(right_i32)),
        "sub-i32": lambda: torch.sub(i32(left_i32), i32(right_i32)),
        "mul-i32": lambda: torch.mul(i32(left_i32), i32(right_i32)),
        "add-f32": lambda: torch.add(
            f32([1.5, -2.25, 3.125, 0.5]), f32([0.5, 2.0, -1.0, 3.0])
        ),
        "sub-f32": lambda: torch.sub(
            f32([1.5, -2.25, 3.125, 0.5]), f32([0.5, 2.0, -1.0, 5.0])
        ),
        "mul-f32": lambda: torch.mul(
            f32([1.5, -2.25, 3.125, 0.5]), f32([0.5, 2.0, -1.0, -4.0])
        ),
        "div-f32": lambda: torch.div(
            f32([1.5, -2.25, 3.125, 0.5]), f32([0.5, 2.0, -1.0, -4.0])
        ),
        "neg-i32": lambda: torch.neg(i32(left_i32)),
        "fill-i32": lambda: torch.empty(6, dtype=torch.int32, device="cuda:0").fill_(7),
        "scalar-add-i32": lambda: torch.add(i32(left_i32), 100),
        "scalar-mul-i32": lambda: torch.mul(i32(left_i32), 2),
        "alpha-add-i32": lambda: torch.add(i32(left_i32), i32(right_i32), alpha=3),
        "abs-i32": lambda: torch.abs(i32(left_i32)),
        "abs-f32": lambda: torch.abs(f32([1.5, -2.25, 3.125, -0.5])),
        "sqrt-f32": lambda: torch.sqrt(f32([0.0, 1.0, 4.0, 9.0])),
        "eq-i32": lambda: torch.eq(i32(left_i32), i32(right_i32)),
        "gt-i32": lambda: torch.gt(i32(left_i32), i32(right_i32)),
        "lt-f32": lambda: torch.lt(
            f32([1.0, 2.0, 3.0, -1.0]), f32([1.0, 3.0, 2.0, 0.0])
        ),
        "sum-i32": lambda: torch.sum(i32(left_i32), dtype=torch.int32),
        "sum-i64": lambda: torch.sum(i32(left_i32)),
        "sum-f32": lambda: torch.sum(f32([1.5, -2.25, 3.125, -0.5])),
        "mean-f32": lambda: torch.mean(f32([1.5, -2.25, 3.125, -0.5])),
        "max-i32": lambda: torch.max(i32(left_i32)),
        "min-i32": lambda: torch.min(i32(left_i32)),
        "cast-i32-f32": lambda: i32(left_i32).to(torch.float32),
        "strided-contiguous-copy-u32": lambda: i32(list(range(24))).reshape(4, 6).t().contiguous(),
        "concat-u32": lambda: torch.cat((i32(left_i32), i32(right_i32)), dim=0),
        "relu-f32": lambda: torch.relu(f32([negative_zero, 0.0, quiet_nan, -2.0, 3.0])),
        "clamp-min-nonzero-f32": lambda: torch.clamp_min(
            f32([-2.0, 0.0, 1.0, 3.0, negative_zero, quiet_nan]), 1.0
        ),
        "clamp-min-negative-f32": lambda: torch.clamp_min(
            f32([-3.0, -2.5, 0.0, quiet_nan]), -2.5
        ),
        "matmul-f32": lambda: torch.matmul(
            f32([1.0, 2.0, 3.0, 4.0]).reshape(2, 2),
            f32([5.0, 6.0, 7.0, 8.0]).reshape(2, 2),
        ),
        "matmul-rect-f32": lambda: torch.matmul(
            f32([1.0, 2.0, 3.0, 4.0, 5.0, 6.0]).reshape(2, 3),
            f32(list(range(7, 19))).reshape(3, 4),
        ),
        "linear-no-bias-f32": lambda: torch.nn.functional.linear(
            f32([1.0, 2.0, 3.0, 4.0, 5.0, 6.0]).reshape(2, 3),
            f32([1.0, 2.0, 3.0, 4.0, 5.0, 6.0]).reshape(2, 3),
        ),
        "addmm-f32": lambda: torch.addmm(
            f32([1.0, 2.0, 3.0, 4.0]).reshape(2, 2),
            f32([1.0, 2.0, 3.0, 4.0]).reshape(2, 2),
            f32([5.0, 6.0, 7.0, 8.0]).reshape(2, 2),
        ),
        "linear-bias-f32": lambda: torch.nn.functional.linear(
            f32([1.0, 2.0, 3.0, 4.0, 5.0, 6.0]).reshape(2, 3),
            f32([1.0, 2.0, 3.0, 4.0, 5.0, 6.0]).reshape(2, 3),
            f32([0.5, -0.5]),
        ),
        "softmax-f32": lambda: torch.softmax(
            f32([0.0, 1.0, 2.0, 2.0, 1.0, 0.0]).reshape(2, 3), dim=1
        ),
        "softmax-f32-nonlast": lambda: torch.softmax(
            f32(list(range(12))).reshape(2, 3, 2), dim=1
        ),
        "sigmoid-f32": lambda: torch.sigmoid(
            f32([-4.0, -2.0, -1.0, 0.0, 1.0, 2.0, 4.0])
        ),
        "arange-i64": lambda: torch.arange(6, device="cuda"),
        "exp-f32": lambda: torch.exp(f32([0.5, 2.0, -1.0, 4.0])),
        "clamp-min-i32": lambda: torch.relu(i32(left_i32)),
        "sigmoid-f64": lambda: torch.sigmoid(
            f64([-4.0, -2.0, -1.0, 0.0, 1.0, 2.0, 4.0])
        ),
    }


def observed_value(torch: Any, value: Any, oracle_kind: str) -> Any:
    cpu = value.to("cpu").contiguous()
    if oracle_kind == "f32-bits":
        bits = cpu.view(torch.int32).reshape(-1).tolist()
        return [f"0x{int(item) & 0xFFFFFFFF:08x}" for item in bits]
    observed = cpu.tolist()
    if isinstance(observed, list) and observed and isinstance(observed[0], list):
        return cpu.reshape(-1).tolist()
    return observed


def application(case_id: str, corpus_path: Path, profile_path: Path) -> dict[str, Any]:
    import torch

    corpus = load_corpus(corpus_path, profile_path)
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
    functions = operation_cases(torch)
    entries = {entry["id"]: entry for entry in corpus["cases"] + corpus["gaps"]}
    if case_id not in entries or case_id not in functions:
        raise ValueError(f"CPU frontier case is not implemented: {case_id}")
    entry = entries[case_id]

    if entry.get("status") == "frontier-gap":
        try:
            functions[case_id]()
            torch.cuda.synchronize()
        except RuntimeError as error:
            message = str(error)
            expected_error = entry["expected_error"]
            if expected_error not in message:
                raise RuntimeError(
                    f"{case_id} returned the wrong error: expected {expected_error!r}, "
                    f"observed {message!r}"
                ) from error
            return {
                "case": case_id,
                "result": "expected-gap",
                "error": expected_error,
                "profile": observed_profile,
            }
        raise RuntimeError(f"{case_id} unexpectedly succeeded")

    oracle = entry["oracle"]
    observed_runs: list[Any] = []
    for _ in range(entry.get("repetitions", 1)):
        value = functions[case_id]()
        torch.cuda.synchronize()
        observed = observed_value(torch, value, oracle["kind"])
        if observed != oracle["expected"]:
            raise RuntimeError(
                f"{case_id} mismatch: expected {oracle['expected']!r}, observed {observed!r}"
            )
        observed_runs.append(observed)
    return {
        "case": case_id,
        "result": "complete",
        "observed": observed_runs,
        "profile": observed_profile,
    }


def parse_provider_evidence(trace: str) -> dict[str, Any]:
    requests = [
        ":".join(match.group(key) for key in ("profile", "operation", "version", "kernel_ir", "lifetime"))
        for match in REQUEST_PATTERN.finditer(trace)
    ]
    return {
        "requests": requests,
        "library_calls": [
            match.group("operation") for match in CUBLAS_REQUEST_PATTERN.finditer(trace)
        ],
        "launches": len(re.findall(r"^MF_LAUNCH ", trace, re.MULTILINE)),
        "module_loads": len(re.findall(r"^MF_PYTORCH_BASELINE_MODULE ", trace, re.MULTILINE)),
        "local_execution": len(re.findall(r"^MF_SEMANTIC ", trace, re.MULTILINE)),
    }


def parse_daemon_statistics(output: str) -> dict[str, int | str]:
    matches = list(STATISTICS_PATTERN.finditer(output))
    if len(matches) != 1:
        raise RuntimeError("daemon did not emit exactly one complete execution record")
    values = matches[0].groupdict()
    return {key: (value if key == "mode" else int(value)) for key, value in values.items()}


def require_execution_statistics(
    statistics: dict[str, int | str],
    mode: str,
    compiler_requests: int,
    cache_hits: int,
    cache_misses: int,
    loaded_modules: int,
) -> None:
    expected: dict[str, int | str] = {
        "mode": mode,
        "compiler_requests": compiler_requests,
        "cache_hits": cache_hits,
        "cache_misses": cache_misses,
        "loaded_modules": loaded_modules,
    }
    observed = {name: statistics[name] for name in expected}
    if observed != expected:
        raise RuntimeError(
            f"{mode} CPU frontier statistics drifted: expected {expected!r}, observed {observed!r}"
        )


def cache_identities(cache_root: Path, expected_count: int) -> list[str]:
    identities: set[str] = set()
    for metadata_path in cache_root.rglob("metadata.v1"):
        for line in metadata_path.read_text(encoding="utf-8").splitlines():
            if line.startswith("cache_key="):
                identities.add(line.removeprefix("cache_key="))
    if len(identities) != expected_count:
        expected = "exactly one" if expected_count == 1 else f"exactly {expected_count}"
        raise RuntimeError(
            f"compiled CPU frontier must materialize {expected} cache identities: "
            f"observed {sorted(identities)!r}"
        )
    ordered = sorted(identities)
    for identity in ordered:
        if re.fullmatch(r"mf-cache-v1-[0-9a-f]{64}", identity) is None:
            raise RuntimeError(f"compiled CPU frontier cache identity is malformed: {identity!r}")
    return ordered


def cache_identity(cache_root: Path) -> str:
    return cache_identities(cache_root, 1)[0]


def require_stable_gap(payload: dict[str, Any], expected_error: str) -> None:
    error = payload.get("error")
    if payload.get("result") != "gap" or not isinstance(error, str):
        raise RuntimeError(f"CPU frontier did not return a structured gap: {payload!r}")
    first_line = error.splitlines()[0] if error else ""
    if first_line != expected_error:
        raise RuntimeError(
            f"CPU frontier gap drifted: expected {expected_error!r}, observed {first_line!r}"
        )


def prewarm_aot(daemon_path: Path, cache_root: Path, source: Path) -> dict[str, Any]:
    environment = os.environ.copy()
    environment.update(
        {
            "METAFLUX_CPU_EXECUTION_MODE": "aot",
            "METAFLUX_COMPILER_CACHE": str(cache_root),
        }
    )
    process = subprocess.run(
        [str(daemon_path), "--prewarm-aot", str(source)],
        check=False,
        env=environment,
        capture_output=True,
        text=True,
        timeout=120,
    )
    match = re.search(
        r"AOT prewarm compiled cache-key=(mf-cache-v1-[0-9a-f]{64})", process.stdout
    )
    if process.returncode != 0 or match is None:
        raise RuntimeError(
            f"CPU frontier AOT prewarm failed ({process.returncode}):\n"
            f"stdout:\n{process.stdout}\nstderr:\n{process.stderr}"
        )
    return {
        "compiled": True,
        "source": str(source.relative_to(ROOT)),
        "cache_identity": match.group(1),
    }


def run_case(
    entry: dict[str, Any],
    environment: dict[str, str],
    corpus_path: Path,
    profile_path: Path,
    expect_success: bool = True,
) -> dict[str, Any]:
    case_id = entry["id"]
    process = subprocess.run(
        [
            sys.executable,
            "-B",
            str(Path(__file__).resolve()),
            "--application",
            "--case",
            case_id,
            "--corpus",
            str(corpus_path),
            "--client-manifest",
            str(profile_path),
        ],
        env=environment,
        check=False,
        capture_output=True,
        text=True,
        timeout=120,
    )
    payload = json.loads(process.stdout)
    provider = parse_provider_evidence(process.stderr)
    if provider["local_execution"] != 0:
        raise RuntimeError(f"provider performed local execution for {case_id}: {provider!r}")
    if not expect_success:
        if process.returncode == 0:
            raise RuntimeError(f"application {case_id} unexpectedly succeeded")
        require_stable_gap(payload, STABLE_AOT_MISS)
        if (
            provider["requests"] != entry["expected_requests"]
            or provider["library_calls"] != entry.get("expected_library_calls", [])
            or provider["module_loads"] != 0
        ):
            raise RuntimeError(f"compiled miss evidence drifted for {case_id}: {provider!r}")
        return {"application": payload, "provider": provider}
    if process.returncode != 0:
        raise RuntimeError(f"application {case_id} failed:\n{process.stdout}\n{process.stderr}")
    if entry.get("status") == "frontier-gap":
        if payload.get("result") != "expected-gap" or provider["requests"]:
            raise RuntimeError(f"gap evidence drifted for {case_id}: {payload!r}, {provider!r}")
        if provider["module_loads"] != 0:
            raise RuntimeError(f"gap {case_id} materialized a daemon module: {provider!r}")
    else:
        expected_requests = entry["expected_requests"]
        expected_library_calls = entry.get("expected_library_calls", [])
        if (
            payload.get("result") != "complete"
            or provider["requests"] != expected_requests
            or provider["library_calls"] != expected_library_calls
        ):
            raise RuntimeError(
                f"supported evidence drifted for {case_id}: {payload!r}, {provider!r}"
            )
        if provider["launches"] < entry.get("repetitions", 1):
            raise RuntimeError(f"provider did not observe every launch for {case_id}")
        if provider["module_loads"] != len(expected_requests):
            raise RuntimeError(f"provider module intake drifted for {case_id}: {provider!r}")
    return {"application": payload, "provider": provider}


def wait_for_socket(daemon: subprocess.Popen[str], socket_path: Path) -> None:
    for _ in range(200):
        if socket_path.is_socket():
            return
        if daemon.poll() is not None:
            output, _ = daemon.communicate(timeout=10)
            raise RuntimeError(
                f"metafluxd exited before socket creation with {daemon.returncode}:\n{output}"
            )
        time.sleep(0.02)
    raise RuntimeError(f"metafluxd did not create {socket_path}")


def stop_daemon(daemon: subprocess.Popen[str]) -> str:
    if daemon.poll() is None:
        daemon.terminate()
    try:
        output, _ = daemon.communicate(timeout=10)
    except subprocess.TimeoutExpired:
        daemon.kill()
        output, _ = daemon.communicate()
    return output


def run_corpus(
    entries: list[dict[str, Any]],
    daemon_path: Path,
    provider_dir: Path,
    cublas_provider: Path,
    corpus_path: Path,
    profile_path: Path,
    execution_mode: str,
    cache_root: Path | None,
    expect_success: bool,
    label: str,
) -> tuple[dict[str, Any], dict[str, int | str]]:
    with tempfile.TemporaryDirectory(prefix=f"mf-pytorch-frontier-{label}-") as temporary:
        # Unix sockets bind only within 108-byte sun_path; deep temporary
        # hierarchies (nested nix-shell TMPDIRs) overflow it, so the daemon
        # socket uses a short /tmp path cleaned up with the scratch directory.
        socket_path = Path(
            tempfile.mkdtemp(prefix=f"mf-frontier-{label}-", dir="/tmp")
        ) / "daemon.sock"
        environment = os.environ.copy()
        environment.update(
            {
                "METAFLUX_MODE": "managed",
                "METAFLUX_SOCKET": str(socket_path),
                "METAFLUX_CPU_EXECUTION_MODE": execution_mode,
                "METAFLUX_TRACE_STUBS": "1",
                "LD_LIBRARY_PATH": str(provider_dir)
                + (
                    os.pathsep + environment["LD_LIBRARY_PATH"]
                    if environment.get("LD_LIBRARY_PATH")
                    else ""
                ),
            }
        )
        if cache_root is not None:
            environment["METAFLUX_COMPILER_CACHE"] = str(cache_root)
        environment["LD_PRELOAD"] = str(cublas_provider) + (
            os.pathsep + environment["LD_PRELOAD"] if environment.get("LD_PRELOAD") else ""
        )
        daemon = subprocess.Popen(
            [str(daemon_path), "--socket", str(socket_path)],
            env=environment,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )
        daemon_output = ""
        application_failure: Exception | None = None
        try:
            wait_for_socket(daemon, socket_path)
            cases = {
                entry["id"]: run_case(
                    entry, environment, corpus_path, profile_path, expect_success
                )
                for entry in entries
            }
        except Exception as error:
            application_failure = error
        finally:
            daemon_output = stop_daemon(daemon)
        if daemon.returncode != 0:
            raise RuntimeError(f"metafluxd exited with {daemon.returncode}:\n{daemon_output}")
        if application_failure is not None:
            raise RuntimeError(
                f"{application_failure}\nmetafluxd output:\n{daemon_output}"
            ) from application_failure
        statistics = parse_daemon_statistics(daemon_output)
        expected_modules = (
            sum(len(entry.get("expected_requests", [])) for entry in entries)
            if expect_success
            else 0
        )
        expected_destinations = (
            sum(
                entry.get("repetitions", 1)
                for entry in entries
                if entry.get("status") != "frontier-gap"
            )
            if expect_success
            else 0
        )
        if statistics["mode"] != execution_mode:
            raise RuntimeError(f"daemon mode drifted: {statistics!r}")
        if statistics["loaded_modules"] != expected_modules:
            raise RuntimeError(
                f"daemon module intake drifted: expected {expected_modules}, observed {statistics!r}"
            )
        if expect_success and statistics["destination_operations"] < expected_destinations:
            raise RuntimeError(
                f"daemon result completion drifted: expected at least {expected_destinations}, "
                f"observed {statistics!r}"
            )
        return cases, statistics


def runner(parsed: argparse.Namespace) -> dict[str, Any]:
    if parsed.daemon is None or parsed.provider_dir is None or parsed.cublas_provider is None:
        raise ValueError("--daemon, --provider-dir, and --cublas-provider are required")
    daemon_path = parsed.daemon.resolve()
    provider_dir = parsed.provider_dir.resolve()
    if not daemon_path.is_file() or not os.access(daemon_path, os.X_OK):
        raise ValueError(f"daemon is not executable: {daemon_path}")
    if not (provider_dir / "libcuda.so.1").is_file():
        raise ValueError(f"provider directory has no libcuda.so.1: {provider_dir}")
    cublas_provider = parsed.cublas_provider.resolve()
    if not cublas_provider.is_file():
        raise ValueError(f"cuBLAS provider is not a file: {cublas_provider}")
    corpus_path = parsed.corpus.resolve()
    profile_path = parsed.client_manifest.resolve()
    corpus = load_corpus(corpus_path, profile_path)
    if parsed.case and parsed.compiled_subset:
        raise ValueError("--case and --compiled-subset are mutually exclusive")
    entries = corpus["cases"] + corpus["gaps"]
    if parsed.case:
        entries = [entry for entry in entries if entry["id"] == parsed.case]
        if not entries:
            raise ValueError(f"CPU frontier corpus has no case {parsed.case!r}")
    elif parsed.compiled_subset:
        compiled_set = set(corpus["scope"]["compiled_subset"])
        entries = [entry for entry in corpus["cases"] if entry["id"] in compiled_set]
    execution_mode = parsed.execution_mode
    compiled_sources: list[Path] = []
    if execution_mode != "interpreter":
        if not parsed.case and not parsed.compiled_subset:
            raise ValueError("compiled CPU frontier qualification requires --case or --compiled-subset")
        compiled_sources = list(dict.fromkeys(compiled_ptx(entry) for entry in entries))
    expected_cache_identities = len(compiled_sources)
    expected_modules = sum(len(entry.get("expected_requests", [])) for entry in entries)
    original_affinity, affinity = pin_client_cpu()
    try:
        with tempfile.TemporaryDirectory(prefix="mf-pytorch-frontier-cache-") as temporary:
            cache_root = Path(temporary) / "compiler-cache"

            def run_once(
                mode: str, label: str, expect_success: bool = True
            ) -> tuple[dict[str, Any], dict[str, int | str]]:
                return run_corpus(
                    entries,
                    daemon_path,
                    provider_dir,
                    cublas_provider,
                    corpus_path,
                    profile_path,
                    mode,
                    cache_root if mode != "interpreter" else None,
                    expect_success,
                    label,
                )

            seed_evidence: dict[str, Any] | None = None
            miss_evidence: dict[str, Any] | None = None
            prewarm_evidence: list[dict[str, Any]] | None = None
            if execution_mode == "interpreter":
                cases, statistics = run_once("interpreter", "interpreter")
                require_execution_statistics(statistics, "interpreter", 0, 0, 0, expected_modules)
                compiled_cache_identities: list[str] = []
            elif execution_mode == "cold-jit":
                cases, statistics = run_once("cold-jit", "cold-jit")
                require_execution_statistics(
                    statistics, "cold-jit", expected_cache_identities,
                    expected_modules - expected_cache_identities, expected_cache_identities,
                    expected_modules,
                )
                compiled_cache_identities = cache_identities(
                    cache_root, expected_cache_identities
                )
            elif execution_mode == "warm-jit":
                _, seed_statistics = run_once("cold-jit", "warm-seed")
                require_execution_statistics(
                    seed_statistics, "cold-jit", expected_cache_identities,
                    expected_modules - expected_cache_identities, expected_cache_identities,
                    expected_modules,
                )
                seeded_identities = cache_identities(cache_root, expected_cache_identities)
                seed_evidence = {"mode": "cold-jit", "statistics": seed_statistics}
                cases, statistics = run_once("warm-jit", "warm-jit")
                require_execution_statistics(
                    statistics, "warm-jit", 0, expected_modules, 0, expected_modules
                )
                compiled_cache_identities = cache_identities(
                    cache_root, expected_cache_identities
                )
                if compiled_cache_identities != seeded_identities:
                    raise RuntimeError("warm JIT did not reuse the cold JIT cache identities")
            else:
                miss_cases, miss_statistics = run_once("aot", "aot-miss", False)
                require_execution_statistics(
                    miss_statistics, "aot", 0, 0, expected_modules, 0
                )
                miss_evidence = {
                    "classification": "stable-not-supported",
                    "cases": miss_cases,
                    "statistics": miss_statistics,
                }
                if not compiled_sources:
                    raise RuntimeError("AOT qualification has no compiled sources")
                prewarm_evidence = [
                    prewarm_aot(daemon_path, cache_root, source)
                    for source in compiled_sources
                ]
                prewarmed_identities = sorted(
                    evidence["cache_identity"] for evidence in prewarm_evidence
                )
                if len(set(prewarmed_identities)) != expected_cache_identities:
                    raise RuntimeError("AOT prewarm produced duplicate cache identities")
                cases, statistics = run_once("aot", "aot-hit")
                require_execution_statistics(
                    statistics, "aot", 0, expected_modules, 0, expected_modules
                )
                compiled_cache_identities = cache_identities(
                    cache_root, expected_cache_identities
                )
                if compiled_cache_identities != prewarmed_identities:
                    raise RuntimeError("AOT runtime did not reuse the prewarmed cache identities")
    finally:
        os.sched_setaffinity(0, original_affinity)
    return {
        "schema_version": 3,
        "gate": "metaflux-pytorch-cuda-cpu-frontier",
        "result": "complete",
        "source": source_provenance(),
        "runner": {"cpu_affinity": affinity, "execution_mode": execution_mode},
        "corpus": {
            "id": corpus["id"],
            "status": corpus["status"],
            "selected_cases": [entry["id"] for entry in entries],
            "compiled_source_count": expected_cache_identities,
        },
        "cases": cases,
        "daemon": {
            "cpu_execution_mode": execution_mode,
            "execution_statistics": statistics,
            "compiler": {
                "input_identity": (
                    "not-applicable-in-interpreter-mode"
                    if execution_mode == "interpreter"
                    else "canonical-kernel-ir-v2"
                ),
                "runtime_requests": statistics["compiler_requests"],
                "prewarm": prewarm_evidence,
            },
            "compiled_artifact_cache": {
                "used": execution_mode != "interpreter",
                "identity_status": (
                    "not-applicable-in-interpreter-mode"
                    if execution_mode == "interpreter"
                    else "versioned"
                ),
                "identities": compiled_cache_identities,
                "seed": seed_evidence,
                "required_miss": miss_evidence,
            },
        },
    }


def main() -> int:
    parsed = arguments()
    try:
        payload = (
            application(parsed.case, parsed.corpus, parsed.client_manifest)
            if parsed.application
            else runner(parsed)
        )
        print(json.dumps(payload, sort_keys=True, separators=(",", ":")))
        return 0
    except (OSError, RuntimeError, ValueError, json.JSONDecodeError) as error:
        print(json.dumps({"result": "gap", "error": str(error)}, sort_keys=True, separators=(",", ":")))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
