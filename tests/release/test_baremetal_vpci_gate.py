#!/usr/bin/env python3
"""Self-test the baremetal-vpci packaging qualification gate."""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
GATE = Path(__file__).resolve().with_name("run_baremetal_vpci_gate.py")


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="metaflux-baremetal-vpci-selftest-") as directory:
        report_path = Path(directory) / "report.json"
        result = subprocess.run(
            [sys.executable, "-B", str(GATE), "--output", str(report_path)],
            check=False,
            capture_output=True,
            text=True,
            cwd=str(ROOT),
        )
        if result.returncode != 0:
            raise AssertionError(result.stderr or result.stdout)
        summary = json.loads(result.stdout)
        assert summary["status"] == "passed"
        report = json.loads(report_path.read_text(encoding="utf-8"))
        assert report["gate"] == "baremetal-vpci"
        assert report["package"]["id"] == "metaflux-vroot-dkms"
        assert report["package"]["depends_on"] == []
        assert "metaflux-vpci-dkms" in report["package"]["forbidden_dependencies"]
        assert report["identity"]["vendor_id"] == 0x4D46
        assert report["namespace_plan"]["compute_entry"] == "forbidden-through-vroot"
        assert report["negative_controls"]["vpci_metadata_rejected"] is True
        assert any("lspci" in item for item in report["host_pending"])
    print("baremetal-vpci gate self-test: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
