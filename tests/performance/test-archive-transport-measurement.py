#!/usr/bin/env python3
"""Self-test the offline transport measurement archive builder."""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "tools/archive-transport-measurement.py"


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="metaflux-measurement-archive-") as directory:
        output = Path(directory) / "archive.json"
        result = subprocess.run(
            [
                sys.executable,
                "-B",
                str(SCRIPT),
                "--output",
                str(output),
                "--lifecycle-core",
                "--note",
                "self-test",
            ],
            check=False,
            capture_output=True,
            text=True,
            cwd=str(ROOT),
        )
        if result.returncode != 0:
            raise AssertionError(result.stderr or result.stdout)
        summary = json.loads(result.stdout)
        assert summary["status"] == "ok"
        assert summary["lifecycle_core_enabled"] is True
        archive = json.loads(output.read_text(encoding="utf-8"))
        assert archive["id"] == "transport.measurement-archive.v0"
        assert archive["status"] == "skeleton"
        assert archive["contract"]["id"] == "transport.measurement.v0"
        assert archive["samples"]["status"] == "not-collected"
        assert "local-cdev-add-copy" in archive["contract"]["workloads"]
        assert archive["fingerprints"]["qemu"] is None
        assert "self-test" in archive["notes"]
    print("transport measurement archive self-test: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
