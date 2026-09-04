#!/usr/bin/env python3
"""Self-test lifecycle-core packaging coexistence gate."""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
GATE = Path(__file__).resolve().with_name("run_lifecycle_core_packaging_gate.py")


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="metaflux-lifecycle-core-packaging-selftest-") as directory:
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
        assert report["gate"] == "lifecycle-core-packaging"
        assert report["install_plan"]["coexistence"]["lifecycle_core_must_not_select_vroot"] is True
        selected = {entry["id"] for entry in report["install_plan"]["selected_packages"]}
        assert "metaflux-vpci-dkms" in selected
        assert "metaflux-vfio-userd" in selected
        excluded = {entry["id"] for entry in report["install_plan"]["excluded_packages"]}
        assert "metaflux-vroot-dkms" in excluded
    print("lifecycle-core packaging gate self-test: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
