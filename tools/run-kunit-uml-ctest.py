#!/usr/bin/env python3
"""CTest entry for the UML KUnit gate via the named linux-debug shell.

Re-enters ``nix develop .#linux-debug`` so CTest exercises the real
in-kernel KUnit qualification on the pinned linux_6_12 source without
requiring the ambient environment to already carry METAFLUX_LINUX_SRC.
Exit codes: 0 pass, 77 skip (nix unavailable), else the child's failure.
"""

from __future__ import annotations

import shutil
import subprocess
import sys
from pathlib import Path

REPOSITORY = Path(__file__).resolve().parent.parent
RUNNER = REPOSITORY / "tools" / "run-kunit-generation.py"


def main() -> int:
    nix = shutil.which("nix")
    if nix is None:
        print(
            "SKIP: nix is unavailable; cannot re-enter the linux-debug shell",
            file=sys.stderr,
        )
        return 77
    command = [
        nix,
        "develop",
        ".#linux-debug",
        "--command",
        "python3",
        "-B",
        str(RUNNER),
    ]
    # stdout/stderr are inherited so CTest streams the kunit.py output live.
    result = subprocess.run(command, cwd=str(REPOSITORY))
    return result.returncode


if __name__ == "__main__":
    sys.exit(main())
