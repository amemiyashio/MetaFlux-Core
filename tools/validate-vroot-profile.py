#!/usr/bin/env python3
"""Validate and project the canonical experimental vroot PCI profile."""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path
from typing import Any


class ProfileError(ValueError):
    """A malformed vroot profile or import closure."""


def load_json(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ProfileError(f"cannot load {path}: {error}") from error
    if not isinstance(value, dict):
        raise ProfileError(f"{path}: expected a JSON object")
    return value


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def repository_root(path: Path) -> Path:
    for parent in (path, *path.parents):
        if (parent / "contracts").is_dir() and (parent / "tools").is_dir():
            return parent
    raise ProfileError(f"cannot locate repository root from {path}")


def resolve_reference(root: Path, owner: Path, reference: dict[str, Any], expected_id: str,
                      expected_version: str) -> tuple[Path, dict[str, Any]]:
    rel = reference.get("path")
    if not isinstance(rel, str) or not rel or Path(rel).is_absolute():
        raise ProfileError(f"{owner}: invalid import path")
    path = (root / rel).resolve()
    if root.resolve() not in path.parents or not path.is_file():
        raise ProfileError(f"{owner}: import is missing or escapes repository: {rel}")
    if reference.get("id") != expected_id or reference.get("version") != expected_version:
        raise ProfileError(f"{owner}: import metadata mismatch for {rel}")
    actual = digest(path)
    if reference.get("sha256") != actual:
        raise ProfileError(f"{owner}: hash mismatch for {rel}: {actual}")
    document = load_json(path)
    if document.get("id") != expected_id or document.get("version") != expected_version:
        raise ProfileError(f"{path}: referenced document metadata mismatch")
    return path, document


def checked_integer(value: Any, path: str, minimum: int, maximum: int) -> int:
    if not isinstance(value, int) or isinstance(value, bool) or value < minimum or value > maximum:
        raise ProfileError(f"{path}: expected integer in [{minimum}, {maximum}]")
    return value


def profile_bytes(profile: dict[str, Any], profile_path: Path) -> tuple[bytes, bytes]:
    config_size = checked_integer(profile.get("config_size"), f"{profile_path}:config_size", 1, 4096)
    if config_size != 256:
        raise ProfileError(f"{profile_path}: vroot Type-0 config size must be 256")
    max_functions = checked_integer(profile.get("max_functions"), f"{profile_path}:max_functions", 1, 8)
    bdf = profile.get("bdf")
    if not isinstance(bdf, dict) or bdf.get("domain_bits") != 16 or bdf.get("bus_bits") != 8 or \
            bdf.get("devfn_stride") != 8 or bdf.get("stable_scope") != "enumeration-domain":
        raise ProfileError(f"{profile_path}: BDF allocation policy is not canonical")
    identity = profile.get("identity")
    if not isinstance(identity, dict) or identity.get("vendor_id") != 0x4D46 or \
            identity.get("device_id") != 0x0001 or identity.get("class_code") != 0x120000 or \
            identity.get("header_type") != 0:
        raise ProfileError(f"{profile_path}: CI Type-0 identity is not canonical")
    if profile.get("capabilities") != [] or profile.get("regions") != [] or profile.get("notifications") != []:
        raise ProfileError(f"{profile_path}: vroot must not advertise BAR, IRQ, or PCI capabilities")
    binding = profile.get("binding")
    if not isinstance(binding, dict) or binding.get("driver") != "metaflux_pci" or \
            binding.get("vendor_matching") is not False or \
            binding.get("stages") != ["present", "prepare_driver", "enable_matching", "probe", "online"] or \
            binding.get("probe_failure") != "quarantine_and_remove" or \
            binding.get("rescan_requires_logical_presence") is not True:
        raise ProfileError(f"{profile_path}: pre-bind policy is not canonical")

    image = bytearray(config_size)
    writable = bytearray(config_size)
    occupied: set[int] = set()
    fields = profile.get("config_fields")
    if not isinstance(fields, list) or not fields:
        raise ProfileError(f"{profile_path}: config_fields must be a non-empty array")
    for field in fields:
        if not isinstance(field, dict) or not isinstance(field.get("name"), str):
            raise ProfileError(f"{profile_path}: malformed config field")
        name = field["name"]
        offset = checked_integer(field.get("offset"), f"{profile_path}:{name}.offset", 0, config_size - 1)
        width = checked_integer(field.get("width"), f"{profile_path}:{name}.width", 1, 8)
        if offset + width > config_size or width not in (1, 2, 4, 8):
            raise ProfileError(f"{profile_path}:{name}: invalid field range")
        if any(index in occupied for index in range(offset, offset + width)):
            raise ProfileError(f"{profile_path}:{name}: overlapping field")
        occupied.update(range(offset, offset + width))
        value = checked_integer(field.get("value"), f"{profile_path}:{name}.value", 0, (1 << (width * 8)) - 1)
        mask = checked_integer(field.get("writable_mask"), f"{profile_path}:{name}.writable_mask", 0,
                               (1 << (width * 8)) - 1)
        for index in range(width):
            image[offset + index] = (value >> (index * 8)) & 0xFF
            writable[offset + index] = (mask >> (index * 8)) & 0xFF
    if image[0:2] != bytes((0x46, 0x4D)) or image[2:4] != bytes((0x01, 0x00)) or \
            image[8:12] != bytes((0, 0, 0, 0x12)) or image[14] != 0:
        raise ProfileError(f"{profile_path}: config field projection disagrees with identity")
    _ = max_functions
    return bytes(image), bytes(writable)


def validate(root: Path, manifest_path: Path) -> tuple[dict[str, Any], bytes, bytes]:
    manifest = load_json(manifest_path)
    if manifest.get("id") != "transport.vroot-extension.v1" or manifest.get("version") != "1.0":
        raise ProfileError(f"{manifest_path}: expected transport.vroot-extension.v1 version 1.0")
    imports = manifest.get("imports")
    if not isinstance(imports, list) or len(imports) != 2 or not all(isinstance(item, dict) for item in imports):
        raise ProfileError(f"{manifest_path}: exactly base and lifecycle imports are required")
    import_ids = [item.get("id") for item in imports]
    if len(set(import_ids)) != len(import_ids) or set(import_ids) != {
            "transport.base.v0", "transport.lifecycle-extension.v1"}:
        raise ProfileError(f"{manifest_path}: import closure must contain unique base and lifecycle entries")
    base_path, base = resolve_reference(root, manifest_path, next(item for item in imports if item.get("id") == "transport.base.v0"),
                                        "transport.base.v0", "0.1")
    lifecycle_path, lifecycle = resolve_reference(
        root, manifest_path, next(item for item in imports if item.get("id") == "transport.lifecycle-extension.v1"),
        "transport.lifecycle-extension.v1", "1.0")
    lifecycle_imports = lifecycle.get("imports")
    if not isinstance(lifecycle_imports, list) or len(lifecycle_imports) != 1 or \
            lifecycle_imports[0].get("id") != "transport.base.v0" or \
            lifecycle_imports[0].get("path") != base_path.relative_to(root).as_posix() or \
            lifecycle_imports[0].get("sha256") != digest(base_path):
        raise ProfileError(f"{lifecycle_path}: transitive base import does not match direct base")
    profile_ref = manifest.get("profile")
    if not isinstance(profile_ref, dict):
        raise ProfileError(f"{manifest_path}: profile reference is required")
    profile_path, profile = resolve_reference(root, manifest_path, profile_ref,
                                              "transport.vroot-profile.v1", "1.0")
    image, writable = profile_bytes(profile, profile_path)
    return profile, image, writable


def header_text(profile: dict[str, Any], image: bytes, writable: bytes, kernel: bool = False) -> str:
    def rows(values: bytes) -> str:
        return "\n".join("  " + ", ".join(f"0x{value:02x}" for value in values[index:index + 16]) + ","
                         for index in range(0, len(values), 16))

    include = "#include <linux/types.h>" if kernel else "#include <stdint.h>"
    element_type = "u8" if kernel else "uint8_t"
    return """/* Generated by tools/validate-vroot-profile.py; do not edit. */
#ifndef METAFLUX_VROOT_GENERATED_PROFILE_H
#define METAFLUX_VROOT_GENERATED_PROFILE_H

%s

#define MF_VROOT_PROFILE_CONFIG_SIZE 256U
#define MF_VROOT_PROFILE_MAX_FUNCTIONS 8U
#define MF_VROOT_PROFILE_VENDOR_ID 0x4d46U
#define MF_VROOT_PROFILE_DEVICE_ID 0x0001U
#define MF_VROOT_PROFILE_CLASS_CODE 0x120000U
#define MF_VROOT_PROFILE_DEVFN_STRIDE 8U

static const %s mf_vroot_profile_config_template[MF_VROOT_PROFILE_CONFIG_SIZE] = {
%s
};

static const %s mf_vroot_profile_writable_mask[MF_VROOT_PROFILE_CONFIG_SIZE] = {
%s
};

#endif
""" % (include, element_type, rows(image), element_type, rows(writable))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path("."))
    parser.add_argument("--manifest", type=Path,
                        default=Path("contracts/protocol/transport/v1/schema/extensions/vroot/v1/manifest.json"))
    parser.add_argument("--generate-header", type=Path)
    parser.add_argument("--kernel-header", action="store_true",
                        help="Generate a Linux-kernel-compatible header")
    arguments = parser.parse_args()
    root = arguments.root.resolve()
    manifest = arguments.manifest if arguments.manifest.is_absolute() else root / arguments.manifest
    try:
        profile, image, writable = validate(root, manifest.resolve())
        if arguments.generate_header is not None:
            output = arguments.generate_header.resolve()
            output.parent.mkdir(parents=True, exist_ok=True)
            output.write_text(header_text(profile, image, writable, arguments.kernel_header), encoding="utf-8")
    except ProfileError as error:
        print(f"vroot profile: invalid: {error}", file=sys.stderr)
        return 1
    print(f"vroot profile: ok ({profile['id']} {profile['version']})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
