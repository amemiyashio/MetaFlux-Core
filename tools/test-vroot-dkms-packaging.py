#!/usr/bin/env python3
"""Self-test metaflux-vroot DKMS staging and launcher packaging contracts."""

from __future__ import annotations

import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
STAGER = ROOT / "tools/stage-vroot-dkms.py"
LAUNCHER = ROOT / "packaging/vroot-launcher/metaflux-vroot-launcher.sh"
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
    if not STAGER.is_file() or not LAUNCHER.is_file():
        raise AssertionError("vroot packaging inputs are missing")

    with tempfile.TemporaryDirectory(prefix="metaflux-vroot-dkms-") as directory:
        staged = Path(directory) / "staged"
        result = run([PYTHON, "-B", str(STAGER), "--output-dir", str(staged)])
        assert result.returncode == 0, result.stderr
        payload = json.loads(result.stdout)
        assert payload["status"] == "ok"
        assert payload["package"] == "metaflux-vroot-dkms"

        metadata_path = staged / "package-metadata.json"
        metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
        assert metadata["id"] == "metaflux-vroot-dkms"
        assert metadata["autoinstall"] is False
        assert metadata["depends_on"] == []
        assert "metaflux-vpci-dkms" in metadata["forbidden_dependencies"]
        assert (staged / "metaflux_vroot_main.c").is_file()
        assert (staged / "generated/include/metaflux/vroot/generated_profile.h").is_file()
        assert (staged / "dkms.conf").is_file()
        assert 'PACKAGE_NAME="metaflux-vroot"' in (staged / "dkms.conf").read_text(encoding="utf-8")
        assert "AUTOINSTALL=\"no\"" in (staged / "dkms.conf").read_text(encoding="utf-8")
        header = (staged / "generated/include/metaflux/vroot/generated_profile.h").read_text(
            encoding="utf-8"
        )
        assert "mf_vroot_profile_config_template" in header
        assert "#include <linux/types.h>" in header

        # Second stage into a non-empty directory must fail.
        failed = run(
            [PYTHON, "-B", str(STAGER), "--output-dir", str(staged)],
            check=False,
        )
        assert failed.returncode != 0
        assert "not empty" in failed.stderr

        env = os.environ.copy()
        describe = run(
            ["bash", str(LAUNCHER), "describe-package", str(metadata_path)],
        )
        described = json.loads(describe.stdout)
        assert described["identity"]["vendor_id"] == 0x4D46
        assert described["launch_path"] == "forbidden"
        assert described["depends_on"] == []

        run(["bash", str(LAUNCHER), "assert-no-vpci-dependency", str(metadata_path)])

        plan = run(
            [
                "bash",
                str(LAUNCHER),
                "plan-namespace",
                str(metadata_path),
                "/run/metaflux/vroot-ns",
            ]
        )
        planned = json.loads(plan.stdout)
        assert planned["compute_entry"] == "forbidden-through-vroot"
        assert all("nvidia" not in bind["target"] for bind in planned["bind_only"])
        assert "/dev/nvidia0" in planned["forbidden_targets"]

        # Tampered dependency on vpci must be rejected by the launcher.
        tampered_dir = Path(directory) / "tampered"
        shutil.copytree(staged, tampered_dir)
        tampered_meta = json.loads((tampered_dir / "package-metadata.json").read_text(encoding="utf-8"))
        tampered_meta["depends_on"] = ["metaflux-vpci-dkms"]
        (tampered_dir / "package-metadata.json").write_text(
            json.dumps(tampered_meta, indent=2) + "\n", encoding="utf-8"
        )
        rejected = run(
            [
                "bash",
                str(LAUNCHER),
                "assert-no-vpci-dependency",
                str(tampered_dir / "package-metadata.json"),
            ],
            check=False,
        )
        assert rejected.returncode != 0

        vendor_mount = run(
            [
                "bash",
                str(LAUNCHER),
                "plan-namespace",
                str(metadata_path),
                "/dev/nvidia-ns",
            ],
            check=False,
        )
        assert vendor_mount.returncode != 0

    print("vroot dkms packaging self-test: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
