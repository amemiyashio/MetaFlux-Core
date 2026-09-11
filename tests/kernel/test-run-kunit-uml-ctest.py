#!/usr/bin/env python3
"""Self-tests for tools/run-kunit-uml-ctest.py.

No real UML build is performed:
  - with a PATH that has no nix, the wrapper exits 77 (skip)
  - with a stub nix, the wrapper invokes it with the linux-debug shell and
    the repository runner path, inheriting METAFLUX_KUNIT_CACHE_DIR
"""

from __future__ import annotations

import os
import stat
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "tools/run-kunit-uml-ctest.py"
REPOSITORY = SCRIPT.parent.parent
RUNNER = REPOSITORY / "tools" / "run-kunit-generation.py"


def test_missing_nix_exits_77() -> None:
    env = {
        "PATH": "/nonexistent",
    }
    result = subprocess.run(
        [sys.executable, "-B", str(SCRIPT)],
        capture_output=True,
        text=True,
        env=env,
    )
    assert result.returncode == 77, f"expected 77, got {result.returncode}: {result.stderr!r}"
    print("PASS: missing_nix_exits_77")


def test_stub_nix_receives_runner_invocation() -> None:
    with tempfile.TemporaryDirectory(prefix="metaflux-kunit-uml-ctest-") as tmp:
        stub_dir = Path(tmp) / "bin"
        stub_dir.mkdir()
        stub_out = Path(tmp) / "argv.txt"
        stub = stub_dir / "nix"
        stub.write_text(
            "#!/bin/sh\n"
            'printf "%s\\n" "$@" > "$STUB_OUT"\n'
            "exit 0\n"
        )
        stub.chmod(stub.stat().st_mode | stat.S_IXUSR)
        env = {
            "PATH": f"{stub_dir}:/usr/bin:/bin",
            "STUB_OUT": str(stub_out),
            "METAFLUX_KUNIT_CACHE_DIR": str(Path(tmp) / "cache"),
        }
        result = subprocess.run(
            [sys.executable, "-B", str(SCRIPT)],
            capture_output=True,
            text=True,
            env=env,
        )
        assert result.returncode == 0, f"expected 0, got {result.returncode}: {result.stderr!r}"
        argv = stub_out.read_text().splitlines()
        assert ".#linux-debug" in argv, f"missing flake shell in {argv}"
        assert "develop" in argv, f"missing develop in {argv}"
        assert "--ignore-environment" in argv, f"missing clean Nix isolation in {argv}"
        assert argv.count("--keep") == 2, f"expected HOME/USER retention in {argv}"
        assert "HOME" in argv and "USER" in argv, f"missing retained identity vars in {argv}"
        assert str(RUNNER) in argv, f"missing runner path in {argv}"
    print("PASS: stub_nix_receives_runner_invocation")


def main() -> int:
    tests = [
        test_missing_nix_exits_77,
        test_stub_nix_receives_runner_invocation,
    ]
    failed = 0
    for test in tests:
        try:
            test()
        except Exception as exc:
            print(f"FAIL: {test.__name__}: {exc}", file=sys.stderr)
            failed += 1
    if failed:
        print(f"{failed} self-test(s) failed", file=sys.stderr)
        return 1
    print("All self-tests passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
