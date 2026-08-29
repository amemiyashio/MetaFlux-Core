#!/usr/bin/env python3

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import platform
import random
import re
import shlex
import shutil
import statistics
import subprocess
import sys
import tempfile
import time
from typing import Any, Iterable, Sequence


PGO_TRAINING_TESTS = (
    "metaflux.unit.provider.cuda-semantics",
    "metaflux.unit.provider.nvml-semantics",
    "metaflux.integration.provider.nvml-policy-setters",
    "metaflux.integration.daemon-execution-modes",
    "metaflux.integration.daemon-compiler-worker",
    "metaflux.differential.cpu-compiled-corpus",
    "metaflux.integration.cuda-add-copy-managed",
    "metaflux.integration.cuda-add-copy-managed.cold-jit",
    "metaflux.integration.cuda-add-copy-managed.warm-jit",
    "metaflux.integration.cuda-add-copy-managed.aot",
    "metaflux.performance.m0001-managed-smoke",
    "metaflux.contract.shared-device-layout.c",
    "metaflux.contract.shared-device-layout.cpp",
    "metaflux.contract.backend-plugin-api.v1",
    "metaflux.contract.client-protocol.v1",
    "metaflux.abi.c",
    "metaflux.abi.cpp",
    "metaflux.abi.backend-cpu",
    "metaflux.abi.provider.cuda.bootstrap",
    "metaflux.abi.provider.nvml.bootstrap",
    "metaflux.differential.cpu-add-copy",
    "metaflux.differential.cpu-ptx-corpus",
    "metaflux.unit.cpu-interpreter",
    "metaflux.unit.cpu-placement",
    "metaflux.unit.artifact-cache",
    "metaflux.unit.compiler-core",
    "metaflux.unit.runtime-recovery-model",
    "metaflux.unit.ptx-parser",
    "metaflux.unit.ptx-frontend",
    "metaflux.unit.cuda-passthrough",
    "metaflux.integration.cpu-compiler-pipeline",
    "metaflux.integration.cpu-placement-amd",
    "metaflux.integration.daemon-cross-process",
    "metaflux.integration.daemon-pre-negotiation-admission",
    "metaflux.integration.daemon-process-snapshot",
    "metaflux.integration.daemon-version",
    "metaflux.integration.provider.co-load",
    "metaflux.integration.provider.cuda-nvml-mode",
    "metaflux.integration.runtime-host-fixture",
    "metaflux.integration.runtime-fastpath-registry",
    "metaflux.integration.runtime-registry-recovery",
    "metaflux.stress.runtime-multiprocess-recovery",
    "metaflux.performance.fastpath-smoke",
    "metaflux.performance.m0001-ring-smoke",
    "metaflux.stress.daemon-million-noop",
    "metaflux.unit.client-fastpath-ring",
    "metaflux.abi.provider.cuda-ptds-exports",
    "metaflux.abi.provider.cuda-ptds-header",
    "metaflux.unit.provider.cuda-install-layout",
    "metaflux.unit.provider.nvml-install-layout",
    "metaflux.unit.ptx-manifest",
    "metaflux.unit.runtime",
    "metaflux.unit.runtime-registry",
    "metaflux.qualification.cpu-pic-elf-readelf",
    "metaflux.qualification.cpu-simd-objdump",
    "metaflux.release.matrix-assertions",
)

PROVIDER_VARIANT_TESTS = (
    "metaflux.unit.provider.cuda-semantics",
    "metaflux.unit.provider.nvml-semantics",
    "metaflux.integration.provider.cuda-nvml-mode",
    "metaflux.integration.provider.co-load",
    "metaflux.abi.provider.cuda.dynamic",
    "metaflux.abi.provider.cuda.exports",
    "metaflux.abi.provider.nvml.dynamic",
    "metaflux.abi.provider.nvml.exports",
)

SOAK_TESTS = (
    "metaflux.integration.runtime-registry-recovery",
    "metaflux.unit.provider.cuda-semantics",
    "metaflux.unit.provider.nvml-semantics",
    "metaflux.integration.provider.nvml-policy-setters",
    "metaflux.integration.daemon-cross-process",
    "metaflux.integration.daemon-execution-modes",
    "metaflux.integration.daemon-compiler-worker",
    "metaflux.integration.cuda-add-copy-managed",
    "metaflux.integration.cuda-add-copy-managed.cold-jit",
    "metaflux.integration.cuda-add-copy-managed.warm-jit",
    "metaflux.integration.cuda-add-copy-managed.aot",
)

FUZZ_OPERATIONS = (
    "bit_flip",
    "byte_insert",
    "byte_delete",
    "truncate",
    "duplicate_slice",
    "splice",
    "nul_insert",
    "token_replace",
    "whitespace_burst",
    "numeric_burst",
)

PROFILE_ROLE_MARKERS = {
    "cuda_provider": ("mf_cuda_managed_",),
    "nvml_provider": ("mf_nvml_managed_",),
    "compiler_service": (
        "CompilerWorker",
        "compiler_worker",
        "compile_kernel",
        "_ZN8metaflux8compiler",
    ),
}

PGO_USE_DIAGNOSTIC_FLAGS = ("-Wno-profile-instr-unprofiled",)


class QualificationError(RuntimeError):
    pass


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat()


def sha256_bytes(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest()


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def canonical_json(value: Any) -> bytes:
    return json.dumps(value, sort_keys=True, separators=(",", ":")).encode("utf-8")


def atomic_write_text(path: Path, value: str) -> None:
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_text(value, encoding="utf-8")
    os.replace(temporary, path)


def atomic_write_json(path: Path, value: Any) -> None:
    atomic_write_text(path, json.dumps(value, indent=2, sort_keys=True) + "\n")


def relative_or_absolute(path: Path, root: Path) -> str:
    try:
        return path.resolve().relative_to(root.resolve()).as_posix()
    except ValueError:
        return str(path.resolve())


def git_output(repository: Path, *arguments: str) -> str:
    completed = subprocess.run(
        ["git", "-C", str(repository), *arguments],
        check=False,
        capture_output=True,
        text=True,
        errors="replace",
    )
    if completed.returncode != 0:
        raise QualificationError(
            f"git {' '.join(arguments)} failed: {completed.stderr.strip()}"
        )
    return completed.stdout.strip()


def git_source_identity(repository: Path) -> dict[str, Any]:
    repository = repository.resolve()
    root = Path(git_output(repository, "rev-parse", "--show-toplevel")).resolve()
    if root != repository:
        raise QualificationError(
            f"--repository must be the Git worktree root: expected {root}, got {repository}"
        )
    revision = git_output(repository, "rev-parse", "--verify", "HEAD")
    tree = git_output(repository, "rev-parse", "--verify", "HEAD^{tree}")
    status = git_output(repository, "status", "--porcelain=v1", "--untracked-files=all")
    if status:
        entries = status.splitlines()
        raise QualificationError(
            "M0001 optimization qualification requires a clean Git worktree; "
            f"observed {len(entries)} changed path(s), first entry: {entries[0]}"
        )
    return {
        "kind": "clean-git-head-tree",
        "git_revision": revision,
        "git_tree": tree,
        "clean": True,
    }


def ensure_fresh_output(path: Path) -> None:
    if path.exists():
        if not path.is_dir():
            raise QualificationError(f"output path exists and is not a directory: {path}")
        entries = sorted(path.iterdir())
        if entries:
            raise QualificationError(
                f"output directory must be empty; refusing pre-existing evidence: {entries[0]}"
            )
    else:
        path.mkdir(parents=True)


WORK_DIRECTORY_MARKER = ".metaflux-m0001-optimization-work"


def prepare_work_directory(requested: Path | None) -> Path:
    if requested is None:
        path = Path(tempfile.mkdtemp(prefix="metaflux-m0001-optimization-"))
    else:
        path = requested.resolve()
        ensure_fresh_output(path)
    (path / WORK_DIRECTORY_MARKER).write_text("owned by run_m0001_optimization.py\n", encoding="utf-8")
    return path


def finalize_work_directory(path: Path, keep: bool) -> bool:
    if keep:
        return True
    marker = path / WORK_DIRECTORY_MARKER
    if not marker.is_file():
        raise QualificationError(f"refusing to remove unmarked work directory: {path}")
    shutil.rmtree(path)
    return False


def sanitize_label(label: str) -> str:
    return re.sub(r"[^A-Za-z0-9_.-]+", "-", label).strip("-") or "command"


class CommandRecorder:
    def __init__(self, output_dir: Path) -> None:
        self.logs_dir = output_dir / "logs"
        self.logs_dir.mkdir(parents=True)
        self.commands: list[dict[str, Any]] = []

    def run(
        self,
        label: str,
        argv: Sequence[str | Path],
        *,
        cwd: Path,
        environment: dict[str, str] | None = None,
        timeout: float | None = None,
        check: bool = True,
    ) -> dict[str, Any]:
        command = [str(value) for value in argv]
        sequence = len(self.commands) + 1
        log_path = self.logs_dir / f"{sequence:04d}-{sanitize_label(label)}.log"
        started_at = utc_now()
        start = time.monotonic_ns()
        timed_out = False
        try:
            completed = subprocess.run(
                command,
                cwd=cwd,
                env=environment,
                check=False,
                capture_output=True,
                text=True,
                errors="replace",
                timeout=timeout,
            )
            returncode = completed.returncode
            stdout = completed.stdout
            stderr = completed.stderr
        except subprocess.TimeoutExpired as error:
            timed_out = True
            returncode = None
            stdout = error.stdout if isinstance(error.stdout, str) else ""
            stderr = error.stderr if isinstance(error.stderr, str) else ""
        duration_ns = time.monotonic_ns() - start
        log = (
            json.dumps({"argv": command, "cwd": str(cwd), "started_at": started_at})
            + "\n--- stdout ---\n"
            + stdout
            + "\n--- stderr ---\n"
            + stderr
        )
        atomic_write_text(log_path, log)
        result = {
            "label": label,
            "argv": command,
            "cwd": str(cwd),
            "started_at": started_at,
            "duration_ns": duration_ns,
            "returncode": returncode,
            "timed_out": timed_out,
            "status": "timeout" if timed_out else "pass" if returncode == 0 else "fail",
            "stdout_sha256": sha256_bytes(stdout.encode("utf-8")),
            "stderr_sha256": sha256_bytes(stderr.encode("utf-8")),
            "stdout_tail": stdout.splitlines()[-20:],
            "stderr_tail": stderr.splitlines()[-20:],
            "log": str(log_path),
            "log_sha256": sha256_file(log_path),
            "stdout": stdout,
            "stderr": stderr,
        }
        self.commands.append(result)
        if check and result["status"] != "pass":
            raise QualificationError(
                f"command {label} {result['status']}; see {log_path}"
            )
        return result

    def serializable_commands(self) -> list[dict[str, Any]]:
        return [
            {
                key: value
                for key, value in item.items()
                if key not in {"stdout", "stderr"}
            }
            for item in self.commands
        ]


def executable(prefix: Path, name: str) -> Path:
    candidate = prefix / "bin" / name
    if not candidate.is_file() or not os.access(candidate, os.X_OK):
        raise QualificationError(f"required tool is missing or not executable: {candidate}")
    return candidate.resolve()


def runtime_environment(toolchain: Path, *, sanitizer: bool = False) -> dict[str, str]:
    environment = os.environ.copy()
    library_path = str((toolchain / "lib").resolve())
    if environment.get("LD_LIBRARY_PATH"):
        library_path += os.pathsep + environment["LD_LIBRARY_PATH"]
    environment["LD_LIBRARY_PATH"] = library_path
    if sanitizer:
        environment["ASAN_OPTIONS"] = "abort_on_error=1:detect_leaks=1:strict_string_checks=1"
        environment["UBSAN_OPTIONS"] = "halt_on_error=1:print_stacktrace=1"
    return environment


def host_fingerprint() -> dict[str, Any]:
    cpu_fields: dict[str, str] = {}
    try:
        first_cpu = Path("/proc/cpuinfo").read_text(encoding="utf-8").split("\n\n", 1)[0]
        for line in first_cpu.splitlines():
            if ":" in line:
                key, value = line.split(":", 1)
                cpu_fields[key.strip()] = value.strip()
    except (OSError, UnicodeError):
        pass
    status_fields: dict[str, str] = {}
    try:
        for line in Path("/proc/self/status").read_text(encoding="utf-8").splitlines():
            if ":" in line:
                key, value = line.split(":", 1)
                status_fields[key.strip()] = value.strip()
    except (OSError, UnicodeError):
        pass
    return {
        "platform": platform.uname()._asdict(),
        "timezone": {
            "environment": os.environ.get("TZ"),
            "names": list(time.tzname),
        },
        "effective_affinity": sorted(os.sched_getaffinity(0)),
        "proc_status": {
            "cpus_allowed_list": status_fields.get("Cpus_allowed_list"),
            "mems_allowed_list": status_fields.get("Mems_allowed_list"),
        },
        "cpu": {
            "vendor_id": cpu_fields.get("vendor_id"),
            "model_name": cpu_fields.get("model name"),
            "cpu_family": cpu_fields.get("cpu family"),
            "model": cpu_fields.get("model"),
            "stepping": cpu_fields.get("stepping"),
            "microcode": cpu_fields.get("microcode"),
        },
    }


def common_cmake_arguments(
    repository: Path, build_dir: Path, tools: dict[str, Path], toolchain: Path
) -> list[str | Path]:
    return [
        tools["cmake"],
        "-S",
        repository,
        "-B",
        build_dir,
        "-G",
        "Ninja",
        f"-DCMAKE_MAKE_PROGRAM={tools['ninja']}",
        f"-DCMAKE_C_COMPILER={tools['clang']}",
        f"-DCMAKE_CXX_COMPILER={tools['clangxx']}",
        f"-DCMAKE_PREFIX_PATH={toolchain.resolve()}",
        "-DBUILD_TESTING=ON",
        "-DMETAFLUX_BUILD_TESTS=ON",
        "-DMETAFLUX_ENABLE_WERROR=ON",
    ]


def configure_full_build(
    recorder: CommandRecorder,
    label: str,
    repository: Path,
    build_dir: Path,
    tools: dict[str, Path],
    toolchain: Path,
    nvidia_headers: Path,
    *,
    build_type: str,
    lto: bool,
    sanitizers: bool,
    pgo_mode: str,
    raw_pattern: str | None = None,
    profile: Path | None = None,
) -> None:
    arguments = common_cmake_arguments(repository, build_dir, tools, toolchain)
    arguments.extend(
        [
            f"-DCMAKE_BUILD_TYPE={build_type}",
            f"-DMETAFLUX_NVIDIA_HEADER_DIR={nvidia_headers.resolve()}",
            f"-DMETAFLUX_ENABLE_LTO={'ON' if lto else 'OFF'}",
            f"-DMETAFLUX_ENABLE_SANITIZERS={'ON' if sanitizers else 'OFF'}",
            "-DMETAFLUX_ENABLE_COVERAGE=OFF",
            f"-DMETAFLUX_PGO_MODE={pgo_mode}",
        ]
    )
    if raw_pattern is not None:
        arguments.append(f"-DMETAFLUX_PGO_RAW_PATTERN={raw_pattern}")
    if profile is not None:
        arguments.append(f"-DMETAFLUX_PGO_PROFILE={profile.resolve()}")
    if pgo_mode == "USE":
        diagnostic_flags = " ".join(PGO_USE_DIAGNOSTIC_FLAGS)
        arguments.extend(
            [
                f"-DCMAKE_C_FLAGS={diagnostic_flags}",
                f"-DCMAKE_CXX_FLAGS={diagnostic_flags}",
            ]
        )
    recorder.run(label, arguments, cwd=repository)


def build_tree(
    recorder: CommandRecorder,
    label: str,
    build_dir: Path,
    tools: dict[str, Path],
    repository: Path,
    jobs: int,
) -> None:
    recorder.run(
        label,
        [tools["cmake"], "--build", build_dir, "--parallel", str(jobs)],
        cwd=repository,
    )


def load_json(path: Path) -> Any:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise QualificationError(f"invalid JSON artifact {path}: {error}") from error


def checked_build_manifest(
    path: Path, *, expected_pgo: str, expected_lto: bool, profile_sha256: str
) -> dict[str, Any]:
    manifest = load_json(path)
    expected_lto_text = "ON" if expected_lto else "OFF"
    expected = {
        "pgo_mode": expected_pgo,
        "lto": expected_lto_text,
        "pgo_profile_sha256": profile_sha256,
    }
    observed = {key: manifest.get(key) for key in expected}
    if observed != expected:
        raise QualificationError(
            f"build manifest {path} does not match requested optimization mode: "
            f"expected {expected}, observed {observed}"
        )
    return {
        "name": path.name,
        "sha256": sha256_file(path),
        "descriptor": manifest,
    }


def ctest_inventory(
    recorder: CommandRecorder,
    label: str,
    build_dir: Path,
    tools: dict[str, Path],
    repository: Path,
    environment: dict[str, str],
) -> set[str]:
    result = recorder.run(
        label,
        [tools["ctest"], "--test-dir", build_dir, "--show-only=json-v1"],
        cwd=repository,
        environment=environment,
    )
    try:
        payload = json.loads(result["stdout"])
        return {test["name"] for test in payload["tests"]}
    except (KeyError, TypeError, json.JSONDecodeError) as error:
        raise QualificationError(f"failed to parse CTest inventory for {build_dir}: {error}") from error


def require_tests(inventory: set[str], tests: Sequence[str], label: str) -> None:
    missing = sorted(set(tests) - inventory)
    if missing:
        raise QualificationError(f"{label} is missing required tests: {', '.join(missing)}")


def ctest_regex(tests: Sequence[str]) -> str:
    return "^(" + "|".join(re.escape(test) for test in tests) + ")$"


def run_ctest(
    recorder: CommandRecorder,
    label: str,
    build_dir: Path,
    tools: dict[str, Path],
    repository: Path,
    environment: dict[str, str],
    tests: Sequence[str],
    timeout: float,
    *,
    check: bool = True,
) -> dict[str, Any]:
    return recorder.run(
        label,
        [
            tools["ctest"],
            "--test-dir",
            build_dir,
            "--output-on-failure",
            "--no-tests=error",
            "--timeout",
            str(timeout),
            "-R",
            ctest_regex(tests),
        ],
        cwd=repository,
        environment=environment,
        timeout=timeout * max(2, len(tests) + 1),
        check=check,
    )


def profile_snapshot(raw_dir: Path) -> dict[str, str]:
    return {
        path.name: sha256_file(path)
        for path in sorted(raw_dir.glob("*.profraw"))
        if path.is_file()
    }


def parse_profile_summary(text: str) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for line in text.splitlines():
        match = re.fullmatch(r"([^:]+):\s*([0-9]+)", line.strip())
        if match:
            result[match.group(1).lower().replace(" ", "_")] = int(match.group(2))
        elif line.startswith("Instrumentation level:"):
            result["instrumentation_level"] = line.split(":", 1)[1].strip()
    required = ("total_functions", "maximum_function_count", "total_count")
    if any(result.get(field, 0) <= 0 for field in required):
        raise QualificationError(f"llvm-profdata summary lacks executed counters: {result}")
    return result


def profile_roles(covered_names: Sequence[str]) -> dict[str, Any]:
    roles: dict[str, Any] = {}
    for role, markers in PROFILE_ROLE_MARKERS.items():
        matches = sorted(
            name for name in covered_names if any(marker in name for marker in markers)
        )
        roles[role] = {
            "markers": list(markers),
            "matched_count": len(matches),
            "sample": matches[:20],
        }
        if not matches:
            raise QualificationError(f"merged PGO profile has no covered function for role {role}")
    return roles


def reject_relative_pgo_profile(
    args: argparse.Namespace, recorder: CommandRecorder, tools: dict[str, Path]
) -> dict[str, Any]:
    build_dir = args.work_dir / "pgo" / "relative-profile-negative-build"
    arguments = common_cmake_arguments(
        args.repository, build_dir, tools, args.toolchain_prefix
    )
    arguments.extend(
        [
            "-DCMAKE_BUILD_TYPE=Release",
            "-DBUILD_TESTING=OFF",
            "-DMETAFLUX_BUILD_TESTS=OFF",
            "-DMETAFLUX_BUILD_RUNTIME_CORE=OFF",
            "-DMETAFLUX_BUILD_CLIENT_FASTPATH=OFF",
            "-DMETAFLUX_BUILD_CUDA_DRIVER_PROVIDER=OFF",
            "-DMETAFLUX_BUILD_NVML_PROVIDER=OFF",
            "-DMETAFLUX_BUILD_CUDA_PTX_FRONTEND=OFF",
            "-DMETAFLUX_BUILD_DAEMON=OFF",
            "-DMETAFLUX_BUILD_COMPILER=OFF",
            "-DMETAFLUX_BUILD_CPU_BACKEND_COMPILER=OFF",
            "-DMETAFLUX_BUILD_CPU_BACKEND_RUNTIME=OFF",
            "-DMETAFLUX_PGO_MODE=USE",
            "-DMETAFLUX_PGO_PROFILE=CMakeLists.txt",
        ]
    )
    command = recorder.run(
        "pgo-relative-profile-negative",
        arguments,
        cwd=args.repository,
        check=False,
    )
    diagnostic = command["stdout"] + command["stderr"]
    expected = "METAFLUX_PGO_PROFILE must be absolute"
    if command["returncode"] == 0 or expected not in diagnostic:
        raise QualificationError(
            "relative PGO profile negative configure did not fail with the absolute-path diagnostic"
        )
    return {
        "status": "pass",
        "input": "CMakeLists.txt",
        "observed_returncode": command["returncode"],
        "expected_diagnostic": expected,
        "log": command["log"],
        "log_sha256": command["log_sha256"],
    }


def reject_out_of_date_pgo_profile(
    args: argparse.Namespace, recorder: CommandRecorder, tools: dict[str, Path]
) -> dict[str, Any]:
    root = args.work_dir / "pgo" / "out-of-date-negative"
    raw_dir = root / "raw"
    raw_dir.mkdir(parents=True)
    source = root / "profile-negative.c"
    generate_binary = root / "profile-negative-generate"
    use_binary = root / "profile-negative-use"
    profile = root / "profile-negative.profdata"
    raw_pattern = str((raw_dir / "%m-%p.profraw").resolve())
    atomic_write_text(
        source,
        "__attribute__((noinline)) int hot(int value) {\n"
        "  return value > 3 ? value + 1 : value - 1;\n"
        "}\n"
        "int main(void) { return hot(7) == 8 ? 0 : 1; }\n",
    )
    recorder.run(
        "pgo-out-of-date-negative-generate-compile",
        [
            tools["clang"],
            "-O2",
            f"-fprofile-instr-generate={raw_pattern}",
            source,
            "-o",
            generate_binary,
        ],
        cwd=args.repository,
    )
    recorder.run(
        "pgo-out-of-date-negative-train",
        [generate_binary],
        cwd=args.repository,
    )
    raw_profiles = sorted(raw_dir.glob("*.profraw"))
    if not raw_profiles or any(path.stat().st_size <= 0 for path in raw_profiles):
        raise QualificationError("out-of-date PGO negative fixture emitted no profile")
    recorder.run(
        "pgo-out-of-date-negative-merge",
        [tools["llvm_profdata"], "merge", "--sparse", "-o", profile, *raw_profiles],
        cwd=args.repository,
    )
    atomic_write_text(
        source,
        "__attribute__((noinline)) int hot(int value) {\n"
        "  if (value > 3) return value + 1;\n"
        "  if (value == 3) return value + 2;\n"
        "  return value - 1;\n"
        "}\n"
        "int main(void) { return hot(7) == 8 ? 0 : 1; }\n",
    )
    command = recorder.run(
        "pgo-out-of-date-negative-use",
        [
            tools["clang"],
            "-O2",
            *PGO_USE_DIAGNOSTIC_FLAGS,
            "-Werror=profile-instr-out-of-date",
            f"-fprofile-instr-use={profile.resolve()}",
            source,
            "-o",
            use_binary,
        ],
        cwd=args.repository,
        check=False,
    )
    diagnostic = command["stdout"] + command["stderr"]
    expected = "profile-instr-out-of-date"
    if command["returncode"] == 0 or expected not in diagnostic:
        raise QualificationError(
            "out-of-date PGO negative compile did not fail with the profile diagnostic"
        )
    return {
        "status": "pass",
        "observed_returncode": command["returncode"],
        "expected_diagnostic": expected,
        "profile_sha256": sha256_file(profile),
        "log": command["log"],
        "log_sha256": command["log_sha256"],
    }


def run_pgo_stage(
    args: argparse.Namespace,
    recorder: CommandRecorder,
    tools: dict[str, Path],
    source_identity: dict[str, Any],
    tool_identity: dict[str, Any],
) -> dict[str, Any]:
    work_root = args.work_dir / "pgo"
    evidence_root = args.output_dir / "pgo"
    generate_build = work_root / "generate-build"
    use_build = work_root / "use-build"
    raw_dir = work_root / "raw"
    progress_path = evidence_root / "progress.json"
    raw_dir.mkdir(parents=True)
    evidence_root.mkdir(parents=True)
    relative_profile_rejection = reject_relative_pgo_profile(args, recorder, tools)
    out_of_date_profile_rejection = reject_out_of_date_pgo_profile(args, recorder, tools)
    if list(raw_dir.glob("*.profraw")):
        raise QualificationError("PGO raw directory contains stale profiles before configuration")
    progress: dict[str, Any] = {
        "schema_version": 1,
        "status": "in_progress",
        "relative_profile_rejection": relative_profile_rejection,
        "out_of_date_profile_rejection": out_of_date_profile_rejection,
        "generate": {"status": "pending"},
        "training": [],
        "merge": {"status": "pending"},
        "use_validation": {"status": "pending", "completed_tests": []},
    }
    atomic_write_json(progress_path, progress)
    raw_pattern = str((raw_dir / "%m-%p.profraw").resolve())
    configure_full_build(
        recorder,
        "pgo-configure-generate",
        args.repository,
        generate_build,
        tools,
        args.toolchain_prefix,
        args.nvidia_header_dir,
        build_type="Release",
        lto=True,
        sanitizers=False,
        pgo_mode="GENERATE",
        raw_pattern=raw_pattern,
    )
    build_tree(
        recorder,
        "pgo-build-generate",
        generate_build,
        tools,
        args.repository,
        args.jobs,
    )
    if list(raw_dir.glob("*.profraw")):
        raise QualificationError(
            "instrumented build emitted profiles before training; refusing contaminated profraw"
        )

    generate_manifests = {
        "provider": checked_build_manifest(
            generate_build / "metaflux-build-manifest.json",
            expected_pgo="GENERATE",
            expected_lto=True,
            profile_sha256="none",
        ),
        "compiler_service": checked_build_manifest(
            generate_build / "metaflux-compiler-build-manifest.json",
            expected_pgo="GENERATE",
            expected_lto=True,
            profile_sha256="none",
        ),
    }
    environment = runtime_environment(args.toolchain_prefix)
    environment["LLVM_PROFILE_FILE"] = raw_pattern
    inventory = ctest_inventory(
        recorder,
        "pgo-generate-inventory",
        generate_build,
        tools,
        args.repository,
        environment,
    )
    require_tests(inventory, args.training_test, "PGO training build")
    progress["generate"] = {
        "status": "pass",
        "manifests": generate_manifests,
        "test_inventory_count": len(inventory),
    }
    atomic_write_json(progress_path, progress)

    training_results: list[dict[str, Any]] = []
    before: dict[str, str] = {}
    for index, test in enumerate(args.training_test, start=1):
        command = run_ctest(
            recorder,
            f"pgo-train-{index:02d}-{test}",
            generate_build,
            tools,
            args.repository,
            environment,
            [test],
            args.test_timeout,
            check=False,
        )
        training_result = {
            "test": test,
            "duration_ns": command["duration_ns"],
            "status": command["status"],
            "log": command["log"],
            "log_sha256": command["log_sha256"],
        }
        training_results.append(training_result)
        progress["training"] = training_results
        atomic_write_json(progress_path, progress)
        if command["status"] != "pass":
            raise QualificationError(
                f"PGO training test {test} {command['status']}; see {command['log']}"
            )
        after = profile_snapshot(raw_dir)
        changed = sorted(name for name, digest in after.items() if before.get(name) != digest)
        training_result["profile_files_changed"] = changed
        atomic_write_json(progress_path, progress)
        before = after

    raw_profiles = [
        {
            "path": path.name,
            "size_bytes": path.stat().st_size,
            "sha256": sha256_file(path),
        }
        for path in sorted(raw_dir.glob("*.profraw"))
    ]
    if not raw_profiles or any(profile["size_bytes"] <= 0 for profile in raw_profiles):
        raise QualificationError("PGO training did not produce non-empty raw profiles")
    profile_set_sha = sha256_bytes(canonical_json(raw_profiles))
    training_contract = {
        "git_revision": source_identity["git_revision"],
        "git_tree": source_identity["git_tree"],
        "generate_manifest_sha256": {
            role: manifest["sha256"] for role, manifest in generate_manifests.items()
        },
        "tests": list(args.training_test),
        "raw_profile_pattern": "%m-%p.profraw",
        "raw_profile_set_sha256": profile_set_sha,
        "tool_fingerprints_sha256": sha256_bytes(canonical_json(tool_identity)),
    }
    training_contract["sha256"] = sha256_bytes(canonical_json(training_contract))

    merged_profile = evidence_root / "m0001-measured.profdata"
    recorder.run(
        "pgo-merge",
        [
            tools["llvm_profdata"],
            "merge",
            "--sparse",
            "-o",
            merged_profile,
            *sorted(raw_dir.glob("*.profraw")),
        ],
        cwd=args.repository,
    )
    profile_sha = sha256_file(merged_profile)
    summary_command = recorder.run(
        "pgo-validate-summary",
        [tools["llvm_profdata"], "show", "--all-functions", "--counts", merged_profile],
        cwd=args.repository,
    )
    profile_summary = parse_profile_summary(summary_command["stdout"])
    covered_command = recorder.run(
        "pgo-validate-covered",
        [tools["llvm_profdata"], "show", "--covered", "--all-functions", merged_profile],
        cwd=args.repository,
    )
    covered_names = sorted(line.strip() for line in covered_command["stdout"].splitlines() if line.strip())
    if not covered_names:
        raise QualificationError("llvm-profdata reported no covered functions")
    role_coverage = profile_roles(covered_names)
    version_command = recorder.run(
        "pgo-validate-version",
        [tools["llvm_profdata"], "show", "--profile-version", "--binary-ids", merged_profile],
        cwd=args.repository,
    )

    profile_identity = {
        "schema_version": 1,
        "status": "measured",
        "git_source": source_identity,
        "tool_fingerprints": tool_identity,
        "negative_checks": {
            "relative_profile": relative_profile_rejection,
            "out_of_date_profile": out_of_date_profile_rejection,
        },
        "training_contract": training_contract,
        "raw_profiles": raw_profiles,
        "profile": {
            "path": merged_profile.name,
            "size_bytes": merged_profile.stat().st_size,
            "sha256": profile_sha,
            "summary": profile_summary,
            "covered_function_count": len(covered_names),
            "covered_function_list_sha256": sha256_bytes("\n".join(covered_names).encode("utf-8")),
            "role_coverage": role_coverage,
            "version_output": version_command["stdout"].splitlines(),
        },
        "use_validation": {"status": "pending"},
    }
    identity_path = evidence_root / "profile-evidence.json"
    atomic_write_json(identity_path, profile_identity)
    progress.update(
        {
            "training_contract": training_contract,
            "merge": {
                "status": "pass",
                "profile": profile_identity["profile"],
                "profile_identity": {
                    "path": str(identity_path),
                    "sha256": sha256_file(identity_path),
                },
            },
        }
    )
    atomic_write_json(progress_path, progress)

    configure_full_build(
        recorder,
        "pgo-configure-use",
        args.repository,
        use_build,
        tools,
        args.toolchain_prefix,
        args.nvidia_header_dir,
        build_type="Release",
        lto=True,
        sanitizers=False,
        pgo_mode="USE",
        profile=merged_profile,
    )
    build_tree(
        recorder, "pgo-build-use", use_build, tools, args.repository, args.jobs
    )
    use_manifests = {
        "provider": checked_build_manifest(
            use_build / "metaflux-build-manifest.json",
            expected_pgo="USE",
            expected_lto=True,
            profile_sha256=profile_sha,
        ),
        "compiler_service": checked_build_manifest(
            use_build / "metaflux-compiler-build-manifest.json",
            expected_pgo="USE",
            expected_lto=True,
            profile_sha256=profile_sha,
        ),
    }
    use_environment = runtime_environment(args.toolchain_prefix)
    use_inventory = ctest_inventory(
        recorder,
        "pgo-use-inventory",
        use_build,
        tools,
        args.repository,
        use_environment,
    )
    require_tests(use_inventory, args.training_test, "PGO USE build")
    use_results = []
    progress["use_validation"]["manifests"] = use_manifests
    atomic_write_json(progress_path, progress)
    for index, test in enumerate(args.training_test, start=1):
        command = run_ctest(
            recorder,
            f"pgo-use-{index:02d}-{test}",
            use_build,
            tools,
            args.repository,
            use_environment,
            [test],
            args.test_timeout,
            check=False,
        )
        use_results.append(
            {
                "test": test,
                "duration_ns": command["duration_ns"],
                "status": command["status"],
                "log": command["log"],
                "log_sha256": command["log_sha256"],
            }
        )
        progress["use_validation"]["completed_tests"] = use_results
        atomic_write_json(progress_path, progress)
        if command["status"] != "pass":
            raise QualificationError(
                f"PGO USE verification test {test} {command['status']}; see {command['log']}"
            )

    profile_identity["use_validation"] = {
        "status": "pass",
        "manifests": use_manifests,
        "tests": use_results,
    }
    atomic_write_json(identity_path, profile_identity)
    progress.update(
        {
            "status": "pass",
            "merge": {
                **progress["merge"],
                "profile_identity": {
                    "path": str(identity_path),
                    "sha256": sha256_file(identity_path),
                },
            },
            "use_validation": profile_identity["use_validation"],
        }
    )
    atomic_write_json(progress_path, progress)

    return {
        "status": "pass",
        "relative_profile_rejection": relative_profile_rejection,
        "out_of_date_profile_rejection": out_of_date_profile_rejection,
        "generate_manifests": generate_manifests,
        "training": training_results,
        "training_contract": training_contract,
        "profile_identity": {
            "path": str(identity_path),
            "sha256": sha256_file(identity_path),
        },
        "profile": profile_identity["profile"],
        "use_manifests": use_manifests,
        "use_verification": use_results,
    }


def configure_provider_variant(
    args: argparse.Namespace,
    recorder: CommandRecorder,
    tools: dict[str, Path],
    build_dir: Path,
    optimization: str,
) -> None:
    arguments = common_cmake_arguments(
        args.repository, build_dir, tools, args.toolchain_prefix
    )
    arguments.extend(
        [
            "-DCMAKE_BUILD_TYPE=Release",
            f"-DCMAKE_C_FLAGS_RELEASE=-{optimization} -DNDEBUG",
            f"-DCMAKE_CXX_FLAGS_RELEASE=-{optimization} -DNDEBUG",
            "-DMETAFLUX_ENABLE_LTO=ON",
            "-DMETAFLUX_ENABLE_SANITIZERS=OFF",
            "-DMETAFLUX_ENABLE_COVERAGE=OFF",
            "-DMETAFLUX_PGO_MODE=OFF",
            "-DMETAFLUX_BUILD_RUNTIME_CORE=OFF",
            "-DMETAFLUX_BUILD_DAEMON=OFF",
            "-DMETAFLUX_BUILD_COMPILER=OFF",
            "-DMETAFLUX_BUILD_CPU_BACKEND_COMPILER=OFF",
            "-DMETAFLUX_BUILD_CPU_BACKEND_RUNTIME=OFF",
            "-DMETAFLUX_BUILD_CUDA_PTX_FRONTEND=OFF",
        ]
    )
    recorder.run(f"variant-{optimization}-configure", arguments, cwd=args.repository)
    build_tree(
        recorder,
        f"variant-{optimization}-build",
        build_dir,
        tools,
        args.repository,
        args.jobs,
    )


def check_variant_compile_commands(path: Path, optimization: str) -> dict[str, Any]:
    entries = load_json(path)
    relevant = []
    for entry in entries:
        source = str(entry.get("file", ""))
        if not (
            source.endswith("/plugins/compat/cuda/abi/driver/src/provider.c")
            or source.endswith("/plugins/compat/cuda/management/nvml/src/provider.c")
            or source.endswith("/plugins/compat/cuda/abi/driver/src/dispatch.c")
            or source.endswith("/plugins/compat/cuda/management/nvml/src/dispatch.c")
        ):
            continue
        command = entry.get("command")
        tokens = shlex.split(command) if isinstance(command, str) else list(entry.get("arguments", []))
        optimization_flags = sorted(
            token for token in tokens if re.fullmatch(r"-O(?:[0-3]|fast|g|s|z)", token)
        )
        if f"-{optimization}" not in optimization_flags or len(optimization_flags) != 1:
            raise QualificationError(
                f"provider compile command has ambiguous optimization flags: {optimization_flags}"
            )
        if "-flto=thin" not in tokens:
            raise QualificationError("provider compile command does not contain -flto=thin")
        relevant.append(
            {
                "file": source,
                "optimization_flags": optimization_flags,
                "thin_lto": True,
                "command_sha256": sha256_bytes("\0".join(tokens).encode("utf-8")),
            }
        )
    if not relevant:
        raise QualificationError(f"no provider commands found in {path}")
    return {"count": len(relevant), "commands": relevant}


def executable_sections(readobj: Path, binary: Path, recorder: CommandRecorder, label: str, repository: Path) -> dict[str, Any]:
    result = recorder.run(
        label,
        [readobj, "--elf-output-style=JSON", "--sections", binary],
        cwd=repository,
    )
    try:
        payload = json.loads(result["stdout"])[0]
        sections = payload["Sections"]
    except (json.JSONDecodeError, KeyError, IndexError, TypeError) as error:
        raise QualificationError(f"failed to parse llvm-readobj JSON for {binary}: {error}") from error
    executable = []
    for wrapper in sections:
        section = wrapper["Section"]
        flags = {flag["Name"] for flag in section["Flags"]["Flags"]}
        if "SHF_ALLOC" in flags and "SHF_EXECINSTR" in flags:
            executable.append(
                {"name": section["Name"]["Name"], "size_bytes": int(section["Size"])}
            )
    if not executable:
        raise QualificationError(f"no allocated executable sections found in {binary}")
    return {
        "sections": executable,
        "total_bytes": sum(section["size_bytes"] for section in executable),
    }


def dynamic_symbols(
    nm: Path, binary: Path, recorder: CommandRecorder, label: str, repository: Path
) -> dict[str, Any]:
    result = recorder.run(
        label,
        [nm, "--dynamic", "--defined-only", "--format=posix", binary],
        cwd=repository,
    )
    names = sorted(
        line.split()[0] for line in result["stdout"].splitlines() if line.strip()
    )
    if not names:
        raise QualificationError(f"no defined dynamic symbols found in {binary}")
    return {
        "count": len(names),
        "sha256": sha256_bytes("\n".join(names).encode("utf-8")),
        "names": names,
    }


def provider_binary_paths(build_dir: Path) -> dict[str, Path]:
    return {
        "cuda": build_dir / "plugins/compat/cuda/abi/driver/libcuda.so.1.0.0",
        "nvml": build_dir / "plugins/compat/cuda/management/nvml/libnvidia-ml.so.1.0.0",
    }


def run_variant_stage(
    args: argparse.Namespace, recorder: CommandRecorder, tools: dict[str, Path]
) -> dict[str, Any]:
    root = args.work_dir / "variants"
    environment = runtime_environment(args.toolchain_prefix)
    variants: dict[str, Any] = {}
    for optimization in ("O2", "O3"):
        build_dir = root / optimization.lower()
        configure_provider_variant(args, recorder, tools, build_dir, optimization)
        manifest = checked_build_manifest(
            build_dir / "metaflux-build-manifest.json",
            expected_pgo="OFF",
            expected_lto=True,
            profile_sha256="none",
        )
        compile_commands = check_variant_compile_commands(
            build_dir / "compile_commands.json", optimization
        )
        inventory = ctest_inventory(
            recorder,
            f"variant-{optimization}-inventory",
            build_dir,
            tools,
            args.repository,
            environment,
        )
        require_tests(inventory, PROVIDER_VARIANT_TESTS, f"provider {optimization} build")
        binaries: dict[str, Any] = {}
        for provider, binary in provider_binary_paths(build_dir).items():
            if not binary.is_file():
                raise QualificationError(f"provider binary is missing: {binary}")
            binaries[provider] = {
                "name": binary.name,
                "size_bytes": binary.stat().st_size,
                "sha256": sha256_file(binary),
                "executable_sections": executable_sections(
                    tools["llvm_readobj"],
                    binary,
                    recorder,
                    f"variant-{optimization}-{provider}-sections",
                    args.repository,
                ),
                "dynamic_symbols": dynamic_symbols(
                    tools["llvm_nm"],
                    binary,
                    recorder,
                    f"variant-{optimization}-{provider}-symbols",
                    args.repository,
                ),
            }
        variants[optimization] = {
            "manifest": manifest,
            "compile_commands": compile_commands,
            "runtime": {
                "test_names": list(PROVIDER_VARIANT_TESTS),
                "runs": args.variant_runs,
                "samples": [],
            },
            "binaries": binaries,
        }

    original_affinity = sorted(os.sched_getaffinity(0))
    selected_cpu = args.variant_cpu if args.variant_cpu is not None else original_affinity[0]
    if selected_cpu not in original_affinity:
        raise QualificationError(
            f"variant CPU {selected_cpu} is outside effective affinity {original_affinity}"
        )
    os.sched_setaffinity(0, {selected_cpu})
    try:
        for iteration in range(args.variant_runs):
            order = ("O2", "O3") if iteration % 2 == 0 else ("O3", "O2")
            for order_index, optimization in enumerate(order):
                command = run_ctest(
                    recorder,
                    f"variant-{optimization}-runtime-{iteration + 1:02d}",
                    root / optimization.lower(),
                    tools,
                    args.repository,
                    environment,
                    PROVIDER_VARIANT_TESTS,
                    args.test_timeout,
                )
                variants[optimization]["runtime"]["samples"].append(
                    {
                        "iteration": iteration + 1,
                        "order_index": order_index,
                        "duration_ns": command["duration_ns"],
                    }
                )
    finally:
        os.sched_setaffinity(0, set(original_affinity))
    for optimization in ("O2", "O3"):
        durations = [
            sample["duration_ns"]
            for sample in variants[optimization]["runtime"]["samples"]
        ]
        variants[optimization]["runtime"]["median_ns"] = int(statistics.median(durations))

    comparisons: dict[str, Any] = {}
    for provider in ("cuda", "nvml"):
        o2 = variants["O2"]["binaries"][provider]
        o3 = variants["O3"]["binaries"][provider]
        o2_symbols = o2["dynamic_symbols"]["names"]
        o3_symbols = o3["dynamic_symbols"]["names"]
        added = sorted(set(o3_symbols) - set(o2_symbols))
        removed = sorted(set(o2_symbols) - set(o3_symbols))
        if added or removed:
            raise QualificationError(
                f"{provider} dynamic export set changed between O2 and O3"
            )
        o2_text = o2["executable_sections"]["total_bytes"]
        o3_text = o3["executable_sections"]["total_bytes"]
        comparisons[provider] = {
            "dynamic_symbol_parity": "pass",
            "executable_bytes": {"O2": o2_text, "O3": o3_text},
            "o3_minus_o2_bytes": o3_text - o2_text,
            "o3_over_o2": float(o3_text) / float(o2_text),
            "file_size_bytes": {"O2": o2["size_bytes"], "O3": o3["size_bytes"]},
        }
    o2_runtime = variants["O2"]["runtime"]["median_ns"]
    o3_runtime = variants["O3"]["runtime"]["median_ns"]
    return {
        "status": "pass",
        "interpretation": "measured regression-suite wall time and ELF code size; no performance threshold is inferred",
        "runtime_affinity": {
            "original": original_affinity,
            "selected_cpu": selected_cpu,
            "policy": "single pinned CPU with alternating O2/O3 order",
        },
        "variants": variants,
        "comparison": {
            "providers": comparisons,
            "runtime_median_ns": {"O2": o2_runtime, "O3": o3_runtime},
            "o3_over_o2_runtime": float(o3_runtime) / float(o2_runtime),
        },
    }


def mutate_bytes(
    operation: str,
    primary: bytes,
    secondary: bytes,
    randomizer: random.Random,
    maximum_size: int,
) -> bytes:
    data = bytearray(primary[:maximum_size])
    if operation == "bit_flip":
        if not data:
            data.append(0)
        index = randomizer.randrange(len(data))
        data[index] ^= 1 << randomizer.randrange(8)
    elif operation == "byte_insert":
        index = randomizer.randrange(len(data) + 1)
        data[index:index] = bytes([randomizer.randrange(256)])
    elif operation == "byte_delete":
        if data:
            del data[randomizer.randrange(len(data))]
        else:
            data.extend(b";")
    elif operation == "truncate":
        del data[randomizer.randrange(len(data) + 1) :]
    elif operation == "duplicate_slice":
        if data:
            start = randomizer.randrange(len(data))
            end = min(len(data), start + 1 + randomizer.randrange(min(64, len(data) - start)))
            destination = randomizer.randrange(len(data) + 1)
            data[destination:destination] = data[start:end]
    elif operation == "splice":
        position = randomizer.randrange(len(data) + 1)
        other_start = randomizer.randrange(len(secondary) + 1)
        data[position:position] = secondary[other_start : other_start + 64]
    elif operation == "nul_insert":
        data.insert(randomizer.randrange(len(data) + 1), 0)
    elif operation == "token_replace":
        replacements = ((b"add", b"xor"), (b".u32", b".b17"), (b"ret", b"bra"))
        before, after = replacements[randomizer.randrange(len(replacements))]
        position = data.find(before)
        if position >= 0:
            data[position : position + len(before)] = after
        else:
            data.extend(after)
    elif operation == "whitespace_burst":
        position = randomizer.randrange(len(data) + 1)
        data[position:position] = b" \t\r\n" * (1 + randomizer.randrange(16))
    elif operation == "numeric_burst":
        position = randomizer.randrange(len(data) + 1)
        data[position:position] = str(randomizer.getrandbits(128)).encode("ascii")
    else:
        raise QualificationError(f"unknown fuzz mutation: {operation}")
    return bytes(data[:maximum_size])


def generate_fuzz_cases(
    corpus: Sequence[tuple[str, bytes]], total: int, seed: int, maximum_size: int
) -> list[dict[str, Any]]:
    if total < len(corpus) + len(FUZZ_OPERATIONS):
        raise QualificationError(
            "fuzz case count must cover every seed and every mutation operation"
        )
    randomizer = random.Random(seed)
    cases: list[dict[str, Any]] = []
    for name, data in corpus:
        cases.append({"base": name, "operation": "seed", "data": data[:maximum_size]})
    while len(cases) < total:
        operation = FUZZ_OPERATIONS[(len(cases) - len(corpus)) % len(FUZZ_OPERATIONS)]
        primary_name, primary = corpus[randomizer.randrange(len(corpus))]
        secondary_name, secondary = corpus[randomizer.randrange(len(corpus))]
        cases.append(
            {
                "base": primary_name,
                "secondary": secondary_name,
                "operation": operation,
                "data": mutate_bytes(operation, primary, secondary, randomizer, maximum_size),
            }
        )
    return cases


def parse_fuzz_outcome(stdout: str) -> str:
    match = re.search(r"(?:^|\s)outcome=(accepted|rejected)(?:\s|$)", stdout)
    if match is None:
        raise QualificationError(f"fuzz driver omitted its outcome: {stdout!r}")
    return match.group(1)


def run_fuzz_gate(
    args: argparse.Namespace,
    driver: Path,
    environment: dict[str, str],
    output_dir: Path,
) -> dict[str, Any]:
    corpus_dir = args.repository / "plugins/compat/cuda/compiler/ptx/corpus/fixtures"
    corpus = [(path.name, path.read_bytes()) for path in sorted(corpus_dir.glob("*.ptx"))]
    if not corpus:
        raise QualificationError(f"PTX fuzz corpus is empty: {corpus_dir}")
    cases = generate_fuzz_cases(corpus, args.fuzz_cases, args.fuzz_seed, args.fuzz_max_bytes)
    current_input = output_dir / "current-input.ptx"
    evidence_path = output_dir / "fuzz-cases.jsonl"
    rows: list[dict[str, Any]] = []
    outcomes = {"accepted": 0, "rejected": 0}
    operations: set[str] = set()
    atomic_write_text(evidence_path, "")
    for index, case in enumerate(cases):
        current_input.write_bytes(case["data"])
        start = time.monotonic_ns()
        try:
            completed = subprocess.run(
                [str(driver), str(current_input)],
                cwd=args.repository,
                env=environment,
                check=False,
                capture_output=True,
                text=True,
                errors="replace",
                timeout=args.fuzz_timeout,
            )
        except subprocess.TimeoutExpired as error:
            failure = output_dir / f"failure-{index:06d}-timeout.ptx"
            failure.write_bytes(case["data"])
            raise QualificationError(f"PTX fuzz case {index} timed out; input {failure}") from error
        duration_ns = time.monotonic_ns() - start
        if completed.returncode != 0:
            failure = output_dir / f"failure-{index:06d}-exit-{completed.returncode}.ptx"
            failure.write_bytes(case["data"])
            raise QualificationError(
                f"PTX fuzz case {index} failed with {completed.returncode}; input {failure}; "
                f"stderr={completed.stderr[-1000:]}"
            )
        outcome = parse_fuzz_outcome(completed.stdout)
        outcomes[outcome] += 1
        operations.add(case["operation"])
        rows.append(
            {
                "index": index,
                "base": case["base"],
                "secondary": case.get("secondary"),
                "operation": case["operation"],
                "input_bytes": len(case["data"]),
                "input_sha256": sha256_bytes(case["data"]),
                "outcome": outcome,
                "duration_ns": duration_ns,
                "stderr_sha256": sha256_bytes(completed.stderr.encode("utf-8")),
            }
        )
        atomic_write_text(
            evidence_path, "".join(json.dumps(row, sort_keys=True) + "\n" for row in rows)
        )
    if set(FUZZ_OPERATIONS) - operations:
        raise QualificationError("PTX fuzz gate did not execute every mutation operation")
    if not all(outcomes.values()):
        raise QualificationError(f"PTX fuzz gate did not exercise both parser outcomes: {outcomes}")
    current_input.unlink()
    return {
        "status": "pass",
        "seed": args.fuzz_seed,
        "case_count": len(rows),
        "maximum_input_bytes": args.fuzz_max_bytes,
        "operations": sorted(operations),
        "outcomes": outcomes,
        "corpus": [
            {"path": name, "size_bytes": len(data), "sha256": sha256_bytes(data)}
            for name, data in corpus
        ],
        "raw_evidence": {
            "path": str(evidence_path),
            "sha256": sha256_file(evidence_path),
        },
    }


def run_hardening_stage(
    args: argparse.Namespace, recorder: CommandRecorder, tools: dict[str, Path]
) -> dict[str, Any]:
    work_root = args.work_dir / "hardening"
    evidence_root = args.output_dir / "hardening"
    build_dir = work_root / "asan-ubsan-build"
    fuzz_dir = evidence_root / "fuzz"
    progress_path = evidence_root / "progress.json"
    soak_path = evidence_root / "soak-runs.jsonl"
    fuzz_dir.mkdir(parents=True)
    configure_full_build(
        recorder,
        "hardening-configure",
        args.repository,
        build_dir,
        tools,
        args.toolchain_prefix,
        args.nvidia_header_dir,
        build_type="RelWithDebInfo",
        lto=False,
        sanitizers=True,
        pgo_mode="OFF",
    )
    build_tree(
        recorder, "hardening-build", build_dir, tools, args.repository, args.jobs
    )
    manifest = checked_build_manifest(
        build_dir / "metaflux-build-manifest.json",
        expected_pgo="OFF",
        expected_lto=False,
        profile_sha256="none",
    )
    environment = runtime_environment(args.toolchain_prefix, sanitizer=True)
    inventory = ctest_inventory(
        recorder,
        "hardening-inventory",
        build_dir,
        tools,
        args.repository,
        environment,
    )
    require_tests(inventory, args.soak_test, "ASan/UBSan soak build")
    fuzz_driver = (
        build_dir
        / "plugins/compat/cuda/compiler/ptx/tests/metaflux_cuda_ptx_fuzz_driver"
    )
    if not fuzz_driver.is_file():
        raise QualificationError(f"PTX fuzz driver is missing: {fuzz_driver}")
    progress = {
        "schema_version": 1,
        "status": "in_progress",
        "manifest": manifest,
        "sanitizers": {
            "address": True,
            "undefined_behavior": True,
            "environment": {
                "ASAN_OPTIONS": environment["ASAN_OPTIONS"],
                "UBSAN_OPTIONS": environment["UBSAN_OPTIONS"],
            },
        },
        "fuzz": {"status": "pending"},
        "soak": {
            "status": "pending",
            "iterations": args.soak_iterations,
            "test_names": list(args.soak_test),
            "expected_runs": args.soak_iterations * len(args.soak_test),
            "completed_runs": 0,
        },
    }
    atomic_write_json(progress_path, progress)
    fuzz = run_fuzz_gate(args, fuzz_driver, environment, fuzz_dir)
    progress["fuzz"] = fuzz
    progress["soak"]["status"] = "in_progress"
    atomic_write_json(progress_path, progress)

    soak_rows: list[dict[str, Any]] = []
    soak_start = time.monotonic_ns()
    for iteration in range(args.soak_iterations):
        for test in args.soak_test:
            command = run_ctest(
                recorder,
                f"soak-{iteration + 1:03d}-{test}",
                build_dir,
                tools,
                args.repository,
                environment,
                [test],
                args.test_timeout,
                check=False,
            )
            soak_rows.append(
                {
                    "iteration": iteration + 1,
                    "test": test,
                    "duration_ns": command["duration_ns"],
                    "status": command["status"],
                    "log": command["log"],
                    "log_sha256": command["log_sha256"],
                }
            )
            atomic_write_text(
                soak_path,
                "".join(json.dumps(row, sort_keys=True) + "\n" for row in soak_rows),
            )
            progress["soak"].update(
                {
                    "status": "in_progress" if command["status"] == "pass" else "fail",
                    "completed_runs": len(soak_rows),
                    "duration_ns": time.monotonic_ns() - soak_start,
                    "raw_evidence": {
                        "path": str(soak_path),
                        "sha256": sha256_file(soak_path),
                    },
                }
            )
            if command["status"] != "pass":
                progress["status"] = "fail"
            atomic_write_json(progress_path, progress)
            if command["status"] != "pass":
                raise QualificationError(
                    f"soak iteration {iteration + 1} test {test} {command['status']}; "
                    f"see {command['log']}"
                )
    soak_duration = time.monotonic_ns() - soak_start
    expected_runs = args.soak_iterations * len(args.soak_test)
    if len(soak_rows) != expected_runs or any(row["status"] != "pass" for row in soak_rows):
        raise QualificationError("soak gate did not complete every configured run")
    progress.update(
        {
            "status": "pass",
        }
    )
    progress["soak"].update(
        {
            "status": "pass",
            "completed_runs": len(soak_rows),
            "duration_ns": soak_duration,
            "raw_evidence": {"path": str(soak_path), "sha256": sha256_file(soak_path)},
        }
    )
    atomic_write_json(progress_path, progress)
    return progress


def partial_stage_evidence(output_dir: Path, stage: str) -> dict[str, Any]:
    progress_path = output_dir / stage / "progress.json"
    if not progress_path.is_file():
        return {}
    reference: dict[str, Any] = {
        "path": relative_or_absolute(progress_path, output_dir),
        "sha256": sha256_file(progress_path),
    }
    try:
        partial = json.loads(progress_path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        reference["parse_error"] = str(error)
        return {"progress_evidence": reference}
    return {"progress_evidence": reference, "partial": partial}


def tool_fingerprints(
    recorder: CommandRecorder, tools: dict[str, Path], repository: Path
) -> dict[str, Any]:
    versions = {}
    for name in ("cmake", "ctest", "ninja", "clang", "clangxx", "llvm_profdata", "llvm_readobj", "llvm_nm"):
        tool = tools[name]
        result = recorder.run(f"tool-version-{name}", [tool, "--version"], cwd=repository)
        versions[name] = {
            "path": str(tool),
            "sha256": sha256_file(tool),
            "version_output": result["stdout"].splitlines()[:10],
        }
    return versions


def parse_int(value: str) -> int:
    return int(value, 0)


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Build and measure M0001 PGO, O2/O3, fuzz, and soak evidence"
    )
    parser.add_argument("--repository", type=Path, default=Path("."))
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument(
        "--work-dir",
        type=Path,
        help="fresh disposable build directory; defaults to a system temporary directory",
    )
    parser.add_argument(
        "--keep-work",
        action="store_true",
        help="retain the disposable build directory after the run",
    )
    parser.add_argument("--toolchain-prefix", required=True, type=Path)
    parser.add_argument("--nvidia-header-dir", type=Path)
    parser.add_argument(
        "--stage", action="append", choices=("pgo", "variants", "hardening")
    )
    parser.add_argument("--jobs", type=int, default=max(1, os.cpu_count() or 1))
    parser.add_argument("--test-timeout", type=float, default=300.0)
    parser.add_argument("--variant-runs", type=int, default=7)
    parser.add_argument("--variant-cpu", type=int)
    parser.add_argument("--fuzz-cases", type=int, default=1024)
    parser.add_argument("--fuzz-seed", type=parse_int, default=0x4D46303030315736)
    parser.add_argument("--fuzz-max-bytes", type=int, default=64 * 1024)
    parser.add_argument("--fuzz-timeout", type=float, default=5.0)
    parser.add_argument("--soak-iterations", type=int, default=10)
    parser.add_argument("--training-test", action="append")
    parser.add_argument("--soak-test", action="append")
    args = parser.parse_args()
    args.repository = args.repository.resolve()
    args.output_dir = args.output_dir.resolve()
    if args.work_dir is not None:
        args.work_dir = args.work_dir.resolve()
        if args.work_dir == args.output_dir or args.work_dir.is_relative_to(args.output_dir):
            parser.error("--work-dir must be separate from --output-dir")
        if args.output_dir.is_relative_to(args.work_dir):
            parser.error("--output-dir must be separate from --work-dir")
    args.toolchain_prefix = args.toolchain_prefix.resolve()
    if args.nvidia_header_dir is not None:
        args.nvidia_header_dir = args.nvidia_header_dir.resolve()
    args.stage = tuple(dict.fromkeys(args.stage or ("pgo", "variants", "hardening")))
    args.training_test = tuple(args.training_test or PGO_TRAINING_TESTS)
    args.soak_test = tuple(args.soak_test or SOAK_TESTS)
    if not args.repository.joinpath("CMakeLists.txt").is_file():
        parser.error("--repository is not the MetaFlux source root")
    if args.jobs <= 0 or args.test_timeout <= 0 or args.variant_runs <= 0:
        parser.error("jobs, test-timeout, and variant-runs must be positive")
    if args.fuzz_cases <= 0 or args.fuzz_max_bytes <= 0 or args.fuzz_timeout <= 0:
        parser.error("fuzz-cases, fuzz-max-bytes, and fuzz-timeout must be positive")
    if args.soak_iterations <= 0:
        parser.error("soak-iterations must be positive")
    if ("pgo" in args.stage or "hardening" in args.stage) and args.nvidia_header_dir is None:
        parser.error("PGO and hardening stages require --nvidia-header-dir")
    if args.nvidia_header_dir is not None and not all(
        args.nvidia_header_dir.joinpath(header).is_file() for header in ("cuda.h", "nvml.h")
    ):
        parser.error("--nvidia-header-dir must contain cuda.h and nvml.h")
    return args


def main() -> int:
    args = parse_arguments()
    source_start = git_source_identity(args.repository)
    ensure_fresh_output(args.output_dir)
    args.work_dir = prepare_work_directory(args.work_dir)
    try:
        recorder = CommandRecorder(args.output_dir)
        tools = {
            "cmake": executable(args.toolchain_prefix, "cmake"),
            "ctest": executable(args.toolchain_prefix, "ctest"),
            "ninja": executable(args.toolchain_prefix, "ninja"),
            "clang": executable(args.toolchain_prefix, "clang"),
            "clangxx": executable(args.toolchain_prefix, "clang++"),
            "llvm_profdata": executable(args.toolchain_prefix, "llvm-profdata"),
            "llvm_readobj": executable(args.toolchain_prefix, "llvm-readobj"),
            "llvm_nm": executable(args.toolchain_prefix, "llvm-nm"),
        }
        started_at = utc_now()
        tool_identity = tool_fingerprints(recorder, tools, args.repository)
        stage_results: dict[str, Any] = {}
        errors: list[dict[str, str]] = []
        stage_functions = {
            "pgo": lambda: run_pgo_stage(
                args, recorder, tools, source_start, tool_identity
            ),
            "variants": lambda: run_variant_stage(args, recorder, tools),
            "hardening": lambda: run_hardening_stage(args, recorder, tools),
        }
        for stage in args.stage:
            try:
                stage_results[stage] = stage_functions[stage]()
            except Exception as error:
                stage_results[stage] = {
                    "status": "fail",
                    "error": str(error),
                    **partial_stage_evidence(args.output_dir, stage),
                }
                errors.append({"stage": stage, "error": str(error)})

        try:
            source_end = git_source_identity(args.repository)
            source_stable = all(
                source_end[key] == source_start[key]
                for key in ("git_revision", "git_tree")
            )
            if not source_stable:
                errors.append(
                    {
                        "stage": "source-identity",
                        "error": "Git HEAD or tree changed while qualification was running",
                    }
                )
        except (QualificationError, OSError) as error:
            source_end = {"error": str(error)}
            source_stable = False
            errors.append({"stage": "source-identity", "error": str(error)})

        report = {
            "schema_version": 1,
            "status": "pass" if not errors else "fail",
            "started_at": started_at,
            "ended_at": utc_now(),
            "selected_stages": list(args.stage),
            "host": host_fingerprint(),
            "parameters": {
                "jobs": args.jobs,
                "test_timeout_seconds": args.test_timeout,
                "variant_runs": args.variant_runs,
                "variant_cpu": args.variant_cpu,
                "fuzz_cases": args.fuzz_cases,
                "fuzz_seed": args.fuzz_seed,
                "fuzz_max_bytes": args.fuzz_max_bytes,
                "fuzz_timeout_seconds": args.fuzz_timeout,
                "soak_iterations": args.soak_iterations,
                "training_tests": list(args.training_test),
                "soak_tests": list(args.soak_test),
            },
            "source": {
                "start": source_start,
                "end": source_end,
                "stable": source_stable,
            },
            "work_directory": {
                "retained": args.keep_work,
                "path": str(args.work_dir) if args.keep_work else None,
                "policy": "temporary; retained only by explicit --keep-work",
            },
            "tools": tool_identity,
            "stages": stage_results,
            "errors": errors,
            "commands": recorder.serializable_commands(),
        }
        report_path = args.output_dir / "optimization-evidence.json"
        atomic_write_json(report_path, report)
        print(f"M0001 optimization evidence: {report_path} status={report['status']}")
        for error in errors:
            print(f"{error['stage']}: {error['error']}", file=sys.stderr)
        return 0 if report["status"] == "pass" else 1
    finally:
        finalize_work_directory(args.work_dir, args.keep_work)


if __name__ == "__main__":
    raise SystemExit(main())
