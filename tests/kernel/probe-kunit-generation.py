#!/usr/bin/env python3
"""Probe and run the KUnit generation suite.

This wrapper is used by the CTest gate metaflux.kernel.kunit-generation.
It delegates to tools/run-kunit-generation.py which checks that
METAFLUX_LINUX_SRC points at a valid linux-debug source tree and then invokes
kunit.py to build and run the suite under UML. When METAFLUX_LINUX_SRC is not
set, the runner exits 77 and the test is skipped.

This probe is NOT gated on host CONFIG_KUNIT; the host probe belongs only to
metaflux.kernel.debug-qualification (probe-debug-kernel.py).
"""

from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
RUNNER_SCRIPT = SCRIPT_DIR.parent.parent / "tools" / "run-kunit-generation.py"


def main() -> int:
    runner_env = os.environ.copy()
    runner_env["METAFLUX_LINUX_SRC"] = os.environ.get("METAFLUX_LINUX_SRC", "")
    result = subprocess.run(
        [sys.executable, "-B", str(RUNNER_SCRIPT)],
        capture_output=True,
        text=True,
        env=runner_env,
    )
    if result.stdout:
        sys.stdout.write(result.stdout)
    if result.stderr:
        sys.stderr.write(result.stderr)
    return result.returncode


if __name__ == "__main__":
    sys.exit(main())
