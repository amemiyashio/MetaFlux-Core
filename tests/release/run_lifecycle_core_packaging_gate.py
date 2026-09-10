#!/usr/bin/env python3
"""Prove lifecycle-core packaging never requires experimental vroot.

Host-independent slice: stage metaflux-vpci-dkms and metaflux-vroot-dkms side by
side, assert mutual forbidden dependencies, and emit a lifecycle-core install
plan that selects only the base guest transport package plus vfio-userd notes.
Live dpkg/rpm install rows remain host qualification.
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
VPCI_STAGER = ROOT / "packaging/dkms/stage-vpci-dkms.py"
VROOT_STAGER = ROOT / "packaging/dkms/stage-vroot-dkms.py"
VFIO_USERD_NOTES = ROOT / "packaging/docs/metaflux-vfio-userd.md"


def run(command: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, check=True, capture_output=True, text=True, cwd=str(ROOT))


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    return parser.parse_args()


def qualify(output: Path) -> dict[str, Any]:
    if not VPCI_STAGER.is_file() or not VROOT_STAGER.is_file():
        raise RuntimeError("vpci and vroot stagers are required")
    if not VFIO_USERD_NOTES.is_file():
        raise RuntimeError("metaflux-vfio-userd packaging notes are required")

    with tempfile.TemporaryDirectory(prefix="metaflux-lifecycle-core-packaging-") as directory:
        base = Path(directory)
        vpci_dir = base / "vpci"
        vroot_dir = base / "vroot"
        run([sys.executable, "-B", str(VPCI_STAGER), "--output-dir", str(vpci_dir)])
        run([sys.executable, "-B", str(VROOT_STAGER), "--output-dir", str(vroot_dir)])

        vpci = json.loads((vpci_dir / "package-metadata.json").read_text(encoding="utf-8"))
        vroot = json.loads((vroot_dir / "package-metadata.json").read_text(encoding="utf-8"))

        if vpci["id"] != "metaflux-vpci-dkms":
            raise RuntimeError("unexpected vpci package id")
        if vroot["id"] != "metaflux-vroot-dkms":
            raise RuntimeError("unexpected vroot package id")
        if "metaflux-vroot-dkms" not in vpci.get("forbidden_dependencies", []):
            raise RuntimeError("vpci must forbid vroot dependency")
        if "metaflux-vpci-dkms" not in vroot.get("forbidden_dependencies", []):
            raise RuntimeError("vroot must forbid vpci dependency")
        if vpci.get("depends_on"):
            raise RuntimeError("vpci lifecycle-core path must declare no package deps")
        if vroot.get("depends_on"):
            raise RuntimeError("vroot must declare no package deps")

        install_plan = {
            "profile": "lifecycle-core",
            "selected_packages": [
                {"id": vpci["id"], "version": vpci["package_version"], "role": "guest-pci-module"},
                {
                    "id": "metaflux-vfio-userd",
                    "role": "vfio-user-service",
                    "notes": VFIO_USERD_NOTES.relative_to(ROOT).as_posix(),
                },
            ],
            "excluded_packages": [
                {
                    "id": vroot["id"],
                    "reason": "experimental-vroot-not-required-for-lifecycle-core",
                }
            ],
            "coexistence": {
                "vroot_may_install_separately": True,
                "lifecycle_core_must_not_select_vroot": True,
                "vendor_nodes_untouched": True,
                "nix_store_runtime_forbidden": True,
            },
        }

        report = {
            "gate": "lifecycle-core-packaging",
            "status": "passed",
            "scope": "host-independent-package-selection",
            "vpci": {
                "id": vpci["id"],
                "version": vpci["package_version"],
                "forbidden_dependencies": vpci["forbidden_dependencies"],
            },
            "vroot": {
                "id": vroot["id"],
                "version": vroot["package_version"],
                "forbidden_dependencies": vroot["forbidden_dependencies"],
            },
            "install_plan": install_plan,
            "host_pending": [
                "real dpkg/rpm install upgrade uninstall on reference hosts",
                "lifecycle-local and lifecycle-qemu latency archives",
                "module signing and Secure Boot enrollment workflow",
            ],
        }

    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return report


def main() -> int:
    arguments = parse_arguments()
    try:
        report = qualify(arguments.output.resolve())
    except Exception as error:  # noqa: BLE001
        print(f"lifecycle-core packaging gate: {error}", file=sys.stderr)
        return 1
    print(json.dumps({"status": report["status"], "gate": report["gate"]}, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
