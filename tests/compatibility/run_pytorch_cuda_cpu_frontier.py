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
    parser.add_argument("--case")
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


def load_corpus(path: Path, profile_path: Path) -> dict[str, Any]:
    corpus = json.loads(path.read_text(encoding="utf-8"))
    if corpus.get("schema_version") != 1 or corpus.get("status") != "frontier-not-frozen":
        raise ValueError("CPU frontier corpus identity or status drifted")
    profile = pinned_profile(profile_path)
    if corpus.get("client") != profile:
        raise ValueError(
            f"CPU frontier client drifted: expected {profile!r}, observed {corpus.get('client')!r}"
        )
    cases = corpus.get("cases")
    gaps = corpus.get("gaps")
    if not isinstance(cases, list) or not isinstance(gaps, list) or not cases or not gaps:
        raise ValueError("CPU frontier corpus must contain supported cases and explicit gaps")
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
    for entry in gaps:
        if entry.get("status") != "frontier-gap" or not entry.get("expected_error"):
            raise ValueError(f"gap {entry['id']} lacks a stable error classification")
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
        "matmul-f32": lambda: torch.matmul(
            f32([1.0, 2.0, 3.0, 4.0]).reshape(2, 2),
            f32([5.0, 6.0, 7.0, 8.0]).reshape(2, 2),
        ),
        "softmax-f32": lambda: torch.softmax(
            f32([0.0, 1.0, 2.0, 2.0, 1.0, 0.0]).reshape(2, 3), dim=1
        ),
        "softmax-f32-nonlast": lambda: torch.softmax(
            f32(list(range(12))).reshape(2, 3, 2), dim=1
        ),
        "clamp-min-nonzero-f32": lambda: torch.clamp_min(f32([-2.0, 0.0, 3.0]), 1.0),
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


def run_case(
    entry: dict[str, Any], environment: dict[str, str], corpus_path: Path, profile_path: Path
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
    if process.returncode != 0:
        raise RuntimeError(f"application {case_id} failed:\n{process.stdout}\n{process.stderr}")
    payload = json.loads(process.stdout)
    provider = parse_provider_evidence(process.stderr)
    if provider["local_execution"] != 0:
        raise RuntimeError(f"provider performed local execution for {case_id}: {provider!r}")
    if entry.get("status") == "frontier-gap":
        if payload.get("result") != "expected-gap" or provider["requests"]:
            raise RuntimeError(f"gap evidence drifted for {case_id}: {payload!r}, {provider!r}")
        if provider["module_loads"] != 0:
            raise RuntimeError(f"gap {case_id} materialized a daemon module: {provider!r}")
    else:
        expected_requests = entry["expected_requests"]
        if payload.get("result") != "complete" or provider["requests"] != expected_requests:
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
    corpus_path: Path,
    profile_path: Path,
) -> tuple[dict[str, Any], dict[str, int | str]]:
    with tempfile.TemporaryDirectory(prefix="mf-pytorch-frontier-") as temporary:
        socket_path = Path(temporary) / "daemon.sock"
        environment = os.environ.copy()
        environment.update(
            {
                "METAFLUX_MODE": "managed",
                "METAFLUX_SOCKET": str(socket_path),
                "METAFLUX_CPU_EXECUTION_MODE": "interpreter",
                "METAFLUX_TRACE_STUBS": "1",
                "LD_LIBRARY_PATH": str(provider_dir)
                + (
                    os.pathsep + environment["LD_LIBRARY_PATH"]
                    if environment.get("LD_LIBRARY_PATH")
                    else ""
                ),
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
            wait_for_socket(daemon, socket_path)
            cases = {
                entry["id"]: run_case(entry, environment, corpus_path, profile_path)
                for entry in entries
            }
        finally:
            daemon_output = stop_daemon(daemon)
        if daemon.returncode != 0:
            raise RuntimeError(f"metafluxd exited with {daemon.returncode}:\n{daemon_output}")
        statistics = parse_daemon_statistics(daemon_output)
        expected_modules = sum(
            len(entry.get("expected_requests", [])) for entry in entries
        )
        expected_destinations = sum(
            entry.get("repetitions", 1)
            for entry in entries
            if entry.get("status") != "frontier-gap"
        )
        if statistics["mode"] != "interpreter":
            raise RuntimeError(f"daemon mode drifted: {statistics!r}")
        if statistics["loaded_modules"] != expected_modules:
            raise RuntimeError(
                f"daemon module intake drifted: expected {expected_modules}, observed {statistics!r}"
            )
        if statistics["destination_operations"] < expected_destinations:
            raise RuntimeError(
                f"daemon result completion drifted: expected at least {expected_destinations}, "
                f"observed {statistics!r}"
            )
        return cases, statistics


def runner(parsed: argparse.Namespace) -> dict[str, Any]:
    if parsed.daemon is None or parsed.provider_dir is None:
        raise ValueError("--daemon and --provider-dir are required")
    daemon_path = parsed.daemon.resolve()
    provider_dir = parsed.provider_dir.resolve()
    if not daemon_path.is_file() or not os.access(daemon_path, os.X_OK):
        raise ValueError(f"daemon is not executable: {daemon_path}")
    if not (provider_dir / "libcuda.so.1").is_file():
        raise ValueError(f"provider directory has no libcuda.so.1: {provider_dir}")
    corpus_path = parsed.corpus.resolve()
    profile_path = parsed.client_manifest.resolve()
    corpus = load_corpus(corpus_path, profile_path)
    entries = corpus["cases"] + corpus["gaps"]
    if parsed.case:
        entries = [entry for entry in entries if entry["id"] == parsed.case]
        if not entries:
            raise ValueError(f"CPU frontier corpus has no case {parsed.case!r}")
    original_affinity, affinity = pin_client_cpu()
    try:
        cases, statistics = run_corpus(
            entries, daemon_path, provider_dir, corpus_path, profile_path
        )
    finally:
        os.sched_setaffinity(0, original_affinity)
    return {
        "schema_version": 1,
        "gate": "metaflux-pytorch-cuda-cpu-frontier",
        "result": "complete",
        "source": source_provenance(),
        "affinity": affinity,
        "corpus": {"id": corpus["id"], "status": corpus["status"]},
        "cases": cases,
        "daemon": statistics,
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
