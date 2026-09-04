#!/usr/bin/env python3
"""Self-test metaflux-vpci DKMS staging packaging contracts."""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
STAGER = ROOT / "tools/stage-vpci-dkms.py"
VROOT_STAGER = ROOT / "tools/stage-vroot-dkms.py"
PYTHON = sys.executable


def run(command: list[str], *, check: bool = True) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        command,
        check=check,
        capture_output=True,
        text=True,
        cwd=str(ROOT),
    )


def main() -> int:
    if not STAGER.is_file():
        raise AssertionError("vpci packaging stager is missing")

    with tempfile.TemporaryDirectory(prefix="metaflux-vpci-dkms-") as directory:
        staged = Path(directory) / "staged"
        result = run([PYTHON, "-B", str(STAGER), "--output-dir", str(staged)])
        assert result.returncode == 0, result.stderr
        payload = json.loads(result.stdout)
        assert payload["status"] == "ok"
        assert payload["package"] == "metaflux-vpci-dkms"

        metadata = json.loads((staged / "package-metadata.json").read_text(encoding="utf-8"))
        assert metadata["id"] == "metaflux-vpci-dkms"
        assert metadata["autoinstall"] is False
        assert metadata["depends_on"] == []
        assert "metaflux-vroot-dkms" in metadata["forbidden_dependencies"]
        assert metadata["profile"]["vendor_id"] == 0x4D46
        assert metadata["profile"]["bar0_size"] == 65536
        assert metadata["profile"]["msix_vectors"] == 2
        assert (staged / "metaflux_pci_main.c").is_file()
        header = (
            staged / "generated/include/metaflux/pci/generated_guest_profile.h"
        ).read_text(encoding="utf-8")
        assert "MF_PCI_GUEST_VENDOR_ID" in header
        assert "#include <linux/types.h>" in header
        assert 'PACKAGE_NAME="metaflux-vpci"' in (staged / "dkms.conf").read_text(encoding="utf-8")

        failed = run([PYTHON, "-B", str(STAGER), "--output-dir", str(staged)], check=False)
        assert failed.returncode != 0
        assert "not empty" in failed.stderr

        # Separation: vpci and vroot packages must not share package ids.
        if VROOT_STAGER.is_file():
            vroot_dir = Path(directory) / "vroot"
            vroot = run([PYTHON, "-B", str(VROOT_STAGER), "--output-dir", str(vroot_dir)])
            vroot_meta = json.loads((vroot_dir / "package-metadata.json").read_text(encoding="utf-8"))
            assert vroot_meta["id"] != metadata["id"]
            assert metadata["id"] not in (vroot_meta.get("depends_on") or [])
            assert "metaflux-vpci-dkms" in (vroot_meta.get("forbidden_dependencies") or [])

    print("vpci dkms packaging self-test: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
