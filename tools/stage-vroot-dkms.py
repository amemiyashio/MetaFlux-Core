#!/usr/bin/env python3
"""Stage the packaging-owned metaflux-vroot DKMS source tree."""

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import sys
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
PACKAGE_DIR = ROOT / "packaging/dkms/metaflux-vroot"
KERNEL_MAIN = ROOT / "kernel/vroot/metaflux_vroot_main.c"
VROOT_MANIFEST = (
    ROOT / "contracts/protocol/transport/v1/schema/extensions/vroot/v1/manifest.json"
)
VERSION_FILE = ROOT / "VERSION"
VALIDATOR = ROOT / "tools/validate-vroot-profile.py"

REQUIRED_STAGED = (
    "dkms.conf",
    "Makefile",
    "metaflux_vroot_main.c",
    "generated/include/metaflux/vroot/generated_profile.h",
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


def load_validator():
    import importlib.util

    spec = importlib.util.spec_from_file_location("metaflux_vroot_profile_validator", VALIDATOR)
    if spec is None or spec.loader is None:
        raise StageError("cannot load validate-vroot-profile.py")
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

    validator = load_validator()
    profile, image, writable = validator.validate(ROOT, VROOT_MANIFEST)
    header_text = validator.header_text(profile, image, writable, kernel=True)

    shutil.copy2(PACKAGE_DIR / "dkms.conf", output_dir / "dkms.conf")
    shutil.copy2(PACKAGE_DIR / "Makefile", output_dir / "Makefile")
    shutil.copy2(PACKAGE_DIR / "README.md", output_dir / "README.md")
    shutil.copy2(KERNEL_MAIN, output_dir / "metaflux_vroot_main.c")

    header_path = output_dir / "generated/include/metaflux/vroot/generated_profile.h"
    header_path.parent.mkdir(parents=True, exist_ok=True)
    header_path.write_text(header_text, encoding="utf-8")

    # Rewrite PACKAGE_VERSION in the staged dkms.conf to the selected version.
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
        "id": "metaflux-vroot-dkms",
        "package_name": "metaflux-vroot",
        "package_version": package_version,
        "kind": "experimental-vroot-dkms-source",
        "module": "metaflux_vroot",
        "autoinstall": False,
        "depends_on": [],
        "forbidden_dependencies": ["metaflux-vpci-dkms", "metaflux-vpci"],
        "profile": {
            "manifest": VROOT_MANIFEST.relative_to(ROOT).as_posix(),
            "manifest_sha256": digest(VROOT_MANIFEST),
            "vendor_id": profile["identity"]["vendor_id"],
            "device_id": profile["identity"]["device_id"],
            "class_code": profile["identity"]["class_code"],
            "config_size": profile["config_size"],
            "max_functions": profile["max_functions"],
            "config_image_sha256": hashlib.sha256(image).hexdigest(),
            "writable_mask_sha256": hashlib.sha256(writable).hexdigest(),
        },
        "sources": files,
        "policy": {
            "presentation_only": True,
            "default_off": True,
            "launch_path": "forbidden",
            "canonical_nodes": ["/dev/metafluxctl", "/dev/metafluxN"],
            "vendor_matching": False,
        },
    }
    metadata_path = output_dir / "package-metadata.json"
    metadata_path.write_text(json.dumps(metadata, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    metadata["sources"]["package-metadata.json"] = digest(metadata_path)
    # Re-write with self hash excluded from circularity: keep sources without self.
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
        print(f"stage-vroot-dkms: {error}", file=sys.stderr)
        return 1
    except Exception as error:  # noqa: BLE001 - surface validator failures cleanly
        print(f"stage-vroot-dkms: {error}", file=sys.stderr)
        return 1
    print(json.dumps({"status": "ok", "package": metadata["id"], "version": version}, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
