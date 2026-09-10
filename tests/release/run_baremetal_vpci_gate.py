#!/usr/bin/env python3
"""Tests-owned baremetal-vpci qualification gate (host-independent slice).

Live lspci/sysfs/module-load rows remain host-gated. This harness freezes the
packaging contract the live rows will consume: staged metaflux-vroot-dkms
metadata, launcher isolation from metaflux-vpci, and proof that presentation
policy forbids compute entry through metaflux_vroot.ko.
"""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
STAGER = ROOT / "packaging/dkms/stage-vroot-dkms.py"
LAUNCHER = ROOT / "packaging/vroot-launcher/metaflux-vroot-launcher.sh"
VPCI_PACKAGE = ROOT / "packaging/dkms/metaflux-vpci"


def run(command: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, check=True, capture_output=True, text=True, cwd=str(ROOT))


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output",
        type=Path,
        required=True,
        help="JSON report path written by the gate",
    )
    parser.add_argument(
        "--package-version",
        default=None,
        help="optional package version override forwarded to the stager",
    )
    return parser.parse_args()


def qualify(output: Path, package_version: str | None) -> dict[str, Any]:
    if not STAGER.is_file():
        raise RuntimeError(f"missing stager: {STAGER}")
    if not LAUNCHER.is_file():
        raise RuntimeError(f"missing launcher: {LAUNCHER}")
    if not (VPCI_PACKAGE / "dkms.conf").is_file():
        raise RuntimeError("metaflux-vpci package skeleton is required as a negative control")

    with tempfile.TemporaryDirectory(prefix="metaflux-baremetal-vpci-") as directory:
        staged = Path(directory) / "metaflux-vroot"
        command = [sys.executable, "-B", str(STAGER), "--output-dir", str(staged)]
        if package_version:
            command.extend(["--package-version", package_version])
        stage_result = run(command)
        stage_payload = json.loads(stage_result.stdout)
        metadata_path = staged / "package-metadata.json"
        metadata = json.loads(metadata_path.read_text(encoding="utf-8"))

        describe = run(["bash", str(LAUNCHER), "describe-package", str(metadata_path)])
        described = json.loads(describe.stdout)
        run(["bash", str(LAUNCHER), "assert-no-vpci-dependency", str(metadata_path)])
        plan = run(
            [
                "bash",
                str(LAUNCHER),
                "plan-namespace",
                str(metadata_path),
                "/run/metaflux/baremetal-vpci",
            ]
        )
        planned = json.loads(plan.stdout)

        # Negative control: launcher must not accept vpci package conf as vroot metadata.
        vpci_reject = subprocess.run(
            ["bash", str(LAUNCHER), "describe-package", str(VPCI_PACKAGE / "dkms.conf")],
            check=False,
            capture_output=True,
            text=True,
            cwd=str(ROOT),
        )
        if vpci_reject.returncode == 0:
            raise RuntimeError("launcher accepted metaflux-vpci dkms.conf as vroot metadata")

        # Policy: compute must not enter through vroot.
        if metadata["policy"]["launch_path"] != "forbidden":
            raise RuntimeError("vroot policy must forbid launch_path")
        if planned["compute_entry"] != "forbidden-through-vroot":
            raise RuntimeError("namespace plan must forbid compute through vroot")

        report = {
            "gate": "baremetal-vpci",
            "status": "passed",
            "scope": "host-independent-packaging-contract",
            "package": {
                "id": metadata["id"],
                "version": metadata["package_version"],
                "module": metadata["module"],
                "autoinstall": metadata["autoinstall"],
                "depends_on": metadata["depends_on"],
                "forbidden_dependencies": metadata["forbidden_dependencies"],
            },
            "stage": stage_payload,
            "identity": described["identity"],
            "namespace_plan": planned,
            "negative_controls": {
                "vpci_metadata_rejected": True,
                "vpci_package_skeleton_present": True,
            },
            "host_pending": [
                "module install/upgrade/uninstall on Linux 6.12 and 6.18",
                "live lspci -Dnn / sysfs / uevent traces",
                "concurrent open/mmap/submit soak with presentation loaded",
                "1 Hz nvidia-smi overhead with vroot enabled vs disabled",
                "module signing and Secure Boot promotion",
            ],
            "evidence": {
                "staged_files": sorted(
                    path.relative_to(staged).as_posix()
                    for path in staged.rglob("*")
                    if path.is_file()
                ),
                "launcher": LAUNCHER.relative_to(ROOT).as_posix(),
                "stager": STAGER.relative_to(ROOT).as_posix(),
            },
        }

    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return report


def main() -> int:
    arguments = parse_arguments()
    try:
        report = qualify(arguments.output.resolve(), arguments.package_version)
    except Exception as error:  # noqa: BLE001
        print(f"baremetal-vpci gate: {error}", file=sys.stderr)
        return 1
    print(json.dumps({"status": report["status"], "gate": report["gate"]}, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
