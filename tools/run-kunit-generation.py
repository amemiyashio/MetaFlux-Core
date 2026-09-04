#!/usr/bin/env python3
"""Run the mf_cdev_generation KUnit suite using the pinned linux-debug source.

This runner is invoked by the CTest gate metaflux.kernel.kunit-generation.
It exits 77 (CTest SKIP_RETURN_CODE) when no valid linux source is available,
and runs the KUnit suite via kunit.py against the pinned linux-debug source.
The suite runs under UML (no host insmod) when the source tree is configured
and kunit.py succeeds in building the kernel.

Exit codes
----------
  0  kunit.py exited 0 (tests passed or were skipped by the kernel)
  77 METAFLUX_LINUX_SRC is unset or missing the required Makefile/kunit.py
  1  unexpected error (kunit.py failed to configure/build/run)
"""

from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path


def main() -> int:
    # METAFLUX_LINUX_SRC must point at a linux 6.12 source tree that contains
    # the kunit.py harness; the linux-debug Nix shell sets this automatically.
    # An empty value must not resolve to the current working directory.
    src_value = os.environ.get("METAFLUX_LINUX_SRC", "").strip()
    if not src_value:
        print(
            "SKIP: METAFLUX_LINUX_SRC is not set or missing Makefile/kunit.py",
            file=sys.stderr,
        )
        return 77
    src_root = Path(src_value).resolve()
    if not (src_root / "Makefile").exists() or not (
        src_root / "tools" / "testing" / "kunit" / "kunit.py"
    ).exists():
        print(
            "SKIP: METAFLUX_LINUX_SRC is not set or missing "
            "Makefile/kunit.py (checked %s)" % src_root,
            file=sys.stderr,
        )
        return 77

    repo_root = Path(__file__).resolve().parent.parent
    kunitconfig = repo_root / "kernel" / "tests" / "kunit" / "kunitconfig"
    if not kunitconfig.exists():
        print(
            "SKIP: repo kunitconfig not found at %s" % kunitconfig,
            file=sys.stderr,
        )
        return 77

    kunit_py = src_root / "tools" / "testing" / "kunit" / "kunit.py"

    # Use a temp work directory for the out-of-tree UML build. The kernel source
    # at METAFLUX_LINUX_SRC is read-only; kunit.py must write to an external
    # build_dir. A real temp dir keeps the repo clean.
    import tempfile

    with tempfile.TemporaryDirectory(prefix="metaflux-kunit-") as workdir:
        result = subprocess.run(
            [
                sys.executable,
                "-B",
                str(kunit_py),
                "run",
                "--build_dir",
                workdir,
                "--kunitconfig",
                str(kunitconfig),
                "--arch",
                "um",
                "--timeout",
                "300",
            ],
            cwd=str(src_root),
            capture_output=True,
            text=True,
            env=os.environ.copy(),
        )
        # Always emit the full kunit.py output so CTest captures TAP lines.
        if result.stdout:
            sys.stdout.write(result.stdout)
        if result.stderr:
            sys.stderr.write(result.stderr)
        # kunit.py exits 0 on success, 1 on config/build/test failure.
        return result.returncode


if __name__ == "__main__":
    sys.exit(main())
