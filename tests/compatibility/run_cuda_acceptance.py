#!/usr/bin/env python3

import argparse
from dataclasses import dataclass
import os
from pathlib import Path
import re
import signal
import socket
import subprocess
import tempfile
import time


def wait_for_socket(path: Path, daemon: subprocess.Popen[str], timeout: float) -> None:
    deadline = time.monotonic() + timeout
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
    raise TimeoutError(f"metafluxd did not publish {path} within {timeout:.1f}s")


def stop_daemon(daemon: subprocess.Popen[str]) -> tuple[str, str]:
    if daemon.poll() is None:
        daemon.send_signal(signal.SIGTERM)
    try:
        return daemon.communicate(timeout=5)
    except subprocess.TimeoutExpired:
        daemon.kill()
        return daemon.communicate(timeout=5)


@dataclass(frozen=True)
class RunResult:
    application_stdout: str
    application_stderr: str
    application_returncode: int
    daemon_stdout: str
    daemon_stderr: str


def run_application(
    daemon_path: Path,
    application_path: Path,
    root: Path,
    environment: dict[str, str],
    mode: str,
    label: str,
    expect_success: bool,
) -> RunResult:
    socket_path = root / f"metafluxd-{label}.sock"
    run_environment = environment.copy()
    run_environment["METAFLUX_SOCKET"] = str(socket_path)
    run_environment["METAFLUX_CPU_EXECUTION_MODE"] = mode
    daemon = subprocess.Popen(
        [str(daemon_path), "--socket", str(socket_path)],
        env=run_environment,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    daemon_stdout = ""
    daemon_stderr = ""
    try:
        wait_for_socket(socket_path, daemon, 10)
        application = subprocess.run(
            [str(application_path)],
            env=run_environment,
            capture_output=True,
            text=True,
            timeout=120,
            check=False,
        )
    finally:
        daemon_stdout, daemon_stderr = stop_daemon(daemon)
    if daemon.returncode != 0:
        raise RuntimeError(
            f"metafluxd {label} exited with {daemon.returncode}\n"
            f"stdout:\n{daemon_stdout}\nstderr:\n{daemon_stderr}"
        )
    passed = application.returncode == 0 and "cuda-add-copy: PASS" in application.stdout
    if passed != expect_success:
        expectation = "success" if expect_success else "an explicit failure"
        raise RuntimeError(
            f"CUDA acceptance {label} expected {expectation} ({application.returncode})\n"
            f"stdout:\n{application.stdout}\nstderr:\n{application.stderr}\n"
            f"daemon stdout:\n{daemon_stdout}\ndaemon stderr:\n{daemon_stderr}"
        )
    return RunResult(
        application_stdout=application.stdout,
        application_stderr=application.stderr,
        application_returncode=application.returncode,
        daemon_stdout=daemon_stdout,
        daemon_stderr=daemon_stderr,
    )


def assert_metrics(
    result: RunResult,
    mode: str,
    compiler_requests: int,
    cache_hits: int,
    cache_misses: int,
) -> None:
    pattern = re.compile(
        rf"cpu-execution mode={re.escape(mode)} "
        rf"compiler-requests={compiler_requests} "
        rf"cache-hits={cache_hits} cache-misses={cache_misses} "
        rf"loaded-modules=\d+"
    )
    if pattern.search(result.daemon_stderr) is None:
        raise RuntimeError(
            f"metafluxd {mode} metrics did not match {pattern.pattern}\n"
            f"stderr:\n{result.daemon_stderr}"
        )


def assert_direct_host_copy(result: RunResult) -> None:
    fields = {
        name: int(value)
        for name, value in re.findall(
            r"(host-address-space-registrations|"
            r"direct-host-source-operations|direct-host-source-bytes|"
            r"direct-host-destination-operations|direct-host-destination-bytes|"
            r"staged-host-source-operations|staged-host-source-bytes|"
            r"staged-host-destination-operations|staged-host-destination-bytes)=(\d+)",
            result.daemon_stderr,
        )
    }
    required = {
        "host-address-space-registrations",
        "direct-host-source-operations",
        "direct-host-source-bytes",
        "direct-host-destination-operations",
        "direct-host-destination-bytes",
        "staged-host-source-operations",
        "staged-host-source-bytes",
        "staged-host-destination-operations",
        "staged-host-destination-bytes",
    }
    if fields.keys() != required:
        raise RuntimeError(f"metafluxd direct-copy counters are incomplete: {fields}")
    if (
        fields["host-address-space-registrations"] != 1
        or fields["direct-host-source-operations"] < 1
        or fields["direct-host-source-bytes"] < 1
        or fields["direct-host-destination-operations"] < 1
        or fields["direct-host-destination-bytes"] < 1
        or fields["staged-host-source-operations"] != 0
        or fields["staged-host-source-bytes"] != 0
        or fields["staged-host-destination-operations"] != 0
        or fields["staged-host-destination-bytes"] != 0
    ):
        raise RuntimeError(f"CUDA acceptance did not use direct host copy exclusively: {fields}")


def assert_peer_uid_cache(cache_root: Path) -> None:
    users = cache_root / "mutable" / "users"
    expected = str(os.getuid())
    materialized = sorted(path.name for path in users.iterdir() if path.is_dir())
    if materialized != [expected] or not (users / expected / "epoch-1").is_dir():
        raise RuntimeError(
            f"mutable cache is not isolated to SO_PEERCRED uid {expected}: {materialized}"
        )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--daemon", required=True, type=Path)
    parser.add_argument("--application", required=True, type=Path)
    parser.add_argument("--provider-dir", required=True, type=Path)
    parser.add_argument(
        "--execution-mode",
        choices=("interpreter", "cold-jit", "warm-jit", "aot"),
        default="interpreter",
    )
    parser.add_argument("--ptx", type=Path)
    args = parser.parse_args()

    with tempfile.TemporaryDirectory(prefix="metaflux-cuda-acceptance-") as temporary:
        root = Path(temporary)
        environment = os.environ.copy()
        cache_root = root / "compiler-cache"
        environment["METAFLUX_COMPILER_CACHE"] = str(cache_root)
        previous_library_path = environment.get("LD_LIBRARY_PATH")
        environment["LD_LIBRARY_PATH"] = str(args.provider_dir)
        if previous_library_path:
            environment["LD_LIBRARY_PATH"] += os.pathsep + previous_library_path

        if args.execution_mode == "interpreter":
            result = run_application(
                args.daemon,
                args.application,
                root,
                environment,
                "interpreter",
                "interpreter",
                True,
            )
            assert_metrics(result, "interpreter", 0, 0, 0)
            assert_direct_host_copy(result)
        elif args.execution_mode == "cold-jit":
            result = run_application(
                args.daemon,
                args.application,
                root,
                environment,
                "cold-jit",
                "cold-jit",
                True,
            )
            assert_metrics(result, "cold-jit", 1, 0, 1)
            assert_direct_host_copy(result)
            assert_peer_uid_cache(cache_root)
        elif args.execution_mode == "warm-jit":
            seed = run_application(
                args.daemon,
                args.application,
                root,
                environment,
                "cold-jit",
                "warm-seed",
                True,
            )
            assert_metrics(seed, "cold-jit", 1, 0, 1)
            assert_direct_host_copy(seed)
            assert_peer_uid_cache(cache_root)
            result = run_application(
                args.daemon,
                args.application,
                root,
                environment,
                "warm-jit",
                "warm-jit",
                True,
            )
            assert_metrics(result, "warm-jit", 0, 1, 0)
            assert_direct_host_copy(result)
        else:
            if args.ptx is None:
                raise RuntimeError("--ptx is required for AOT acceptance")
            miss = run_application(
                args.daemon,
                args.application,
                root,
                environment,
                "aot",
                "aot-miss",
                False,
            )
            assert_metrics(miss, "aot", 0, 0, 1)
            prewarm_environment = environment.copy()
            prewarm_environment["METAFLUX_CPU_EXECUTION_MODE"] = "aot"
            prewarm = subprocess.run(
                [str(args.daemon), "--prewarm-aot", str(args.ptx)],
                env=prewarm_environment,
                capture_output=True,
                text=True,
                timeout=120,
                check=False,
            )
            if prewarm.returncode != 0 or "AOT prewarm compiled" not in prewarm.stdout:
                raise RuntimeError(
                    f"AOT prewarm failed ({prewarm.returncode})\n"
                    f"stdout:\n{prewarm.stdout}\nstderr:\n{prewarm.stderr}"
                )
            result = run_application(
                args.daemon,
                args.application,
                root,
                environment,
                "aot",
                "aot-hit",
                True,
            )
            assert_metrics(result, "aot", 0, 1, 0)
            assert_direct_host_copy(result)
        print(result.application_stdout, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
