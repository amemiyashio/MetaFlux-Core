#!/usr/bin/env python3
"""Probe CONFIG_KUNIT and exit 77 (CTest skip) when absent.

This wrapper is used by the CTest gate metaflux.kernel.kunit-generation.
It delegates to probe-debug-kernel.py --require-config CONFIG_KUNIT so that
the KUnit suite is skipped on hosts without a debug kernel.
"""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
PROBE_SCRIPT = SCRIPT_DIR.parent.parent / "tools" / "probe-debug-kernel.py"


def main() -> int:
    result = subprocess.run(
        [sys.executable, "-B", str(PROBE_SCRIPT), "--require-config", "CONFIG_KUNIT"],
        capture_output=True,
        text=True,
    )
    if result.stdout:
        sys.stdout.write(result.stdout)
    if result.stderr:
        sys.stderr.write(result.stderr)
    return result.returncode


if __name__ == "__main__":
    sys.exit(main())
