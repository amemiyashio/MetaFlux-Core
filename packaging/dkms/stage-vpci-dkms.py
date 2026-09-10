#!/usr/bin/env python3
"""Stage the packaging-owned metaflux-vpci DKMS source tree."""

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import sys
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
PACKAGE_DIR = ROOT / "packaging/dkms/metaflux-vpci"
KERNEL_MAIN = ROOT / "kernel/pci/metaflux_pci_main.c"
GENERATOR = ROOT / "tools/generate-pci-guest-profile.py"
VERSION_FILE = ROOT / "VERSION"

REQUIRED_STAGED = (
    "dkms.conf",
    "Makefile",
    "metaflux_pci_main.c",
    "generated/include/metaflux/pci/generated_guest_profile.h",
    "package-metadata.json",
    "README.md",
)


class StageError(RuntimeError):
    """Staging failed for a deterministic packaging reason."""


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load_version() -> str:
    text = VERSION_FILE.read_text(encoding="utf-8").strip()
    if not text:
        raise StageError(f"{VERSION_FILE}: empty version")
    return text


def load_generator():
    import importlib.util

    spec = importlib.util.spec_from_file_location("metaflux_pci_guest_profile", GENERATOR)
    if spec is None or spec.loader is None:
        raise StageError("cannot load generate-pci-guest-profile.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def require_package_templates() -> None:
    for name in ("dkms.conf", "Makefile", "README.md", "SOURCE.manifest"):
        path = PACKAGE_DIR / name
        if not path.is_file():
            raise StageError(f"missing packaging template: {path}")
    if not KERNEL_MAIN.is_file():
        raise StageError(f"missing kernel source: {KERNEL_MAIN}")


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output-dir",
        type=Path,
        required=True,
        help="destination directory for the staged DKMS source tree",
    )
    parser.add_argument(
        "--package-version",
        default=None,
        help="override package version (defaults to repository VERSION)",
    )
    return parser.parse_args()


def stage(output_dir: Path, package_version: str) -> dict[str, Any]:
    require_package_templates()
    if output_dir.exists():
        if any(output_dir.iterdir()):
            raise StageError(f"output directory is not empty: {output_dir}")
    else:
        output_dir.mkdir(parents=True)

    generator = load_generator()
    fixture = generator.compose(ROOT)
    header_text = generator.header_text(fixture, kernel=True)

    shutil.copy2(PACKAGE_DIR / "dkms.conf", output_dir / "dkms.conf")
    shutil.copy2(PACKAGE_DIR / "Makefile", output_dir / "Makefile")
    shutil.copy2(PACKAGE_DIR / "README.md", output_dir / "README.md")
    shutil.copy2(KERNEL_MAIN, output_dir / "metaflux_pci_main.c")

    header_path = output_dir / "generated/include/metaflux/pci/generated_guest_profile.h"
    header_path.parent.mkdir(parents=True, exist_ok=True)
    header_path.write_text(header_text, encoding="utf-8")

    dkms_conf = (output_dir / "dkms.conf").read_text(encoding="utf-8")
    rewritten = []
    for line in dkms_conf.splitlines():
        if line.startswith("PACKAGE_VERSION="):
            rewritten.append(f'PACKAGE_VERSION="{package_version}"')
        else:
            rewritten.append(line)
    (output_dir / "dkms.conf").write_text("\n".join(rewritten) + "\n", encoding="utf-8")

    files: dict[str, str] = {}
    for relative in REQUIRED_STAGED:
        if relative == "package-metadata.json":
            continue
        path = output_dir / relative
        if not path.is_file():
            raise StageError(f"staged tree missing {relative}")
        files[relative] = digest(path)

    metadata = {
        "id": "metaflux-vpci-dkms",
        "package_name": "metaflux-vpci",
        "package_version": package_version,
        "kind": "guest-pci-dkms-source",
        "module": "metaflux_pci",
        "autoinstall": False,
        "depends_on": [],
        "sibling_packages": ["metaflux-vfio-userd"],
        "forbidden_dependencies": ["metaflux-vroot-dkms", "metaflux-vroot"],
        "profile": {
            "vendor_id": fixture["vendor_id"],
            "device_id": fixture["device_id"],
            "class_code": fixture["class_code"],
            "bar0_size": fixture["bar0_size"],
            "bar2_size": fixture["bar2_size"],
            "bar4_size": fixture["bar4_size"],
            "msix_vectors": fixture["msix_vectors"],
        },
        "sources": files,
        "policy": {
            "presentation": "static-guest-pci",
            "default_off_autoinstall": True,
            "binds_vendor_drivers": False,
            "canonical_nodes": ["/dev/metafluxctl", "/dev/metafluxN"],
        },
    }
    metadata_path = output_dir / "package-metadata.json"
    frozen = dict(metadata)
    frozen["sources"] = dict(files)
    metadata_path.write_text(json.dumps(frozen, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return frozen


def main() -> int:
    arguments = parse_arguments()
    version = arguments.package_version or load_version()
    try:
        metadata = stage(arguments.output_dir.resolve(), version)
    except StageError as error:
        print(f"stage-vpci-dkms: {error}", file=sys.stderr)
        return 1
    except Exception as error:  # noqa: BLE001
        print(f"stage-vpci-dkms: {error}", file=sys.stderr)
        return 1
    print(json.dumps({"status": "ok", "package": metadata["id"], "version": version}, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
