#!/usr/bin/env python3
"""Validate and project the frozen mf_admin_lifecycle_v1 control-plane records.

The lifecycle extension owns this layout. It imports the milestone-0.1.1.0
transport base by content hash and never rewrites base digests. work-item-0.1.2.3
freezes the hashed admin schema after the host-independent Coordinator fault
suite; kernel sanitizer soaks remain batch-0002 host gates and do not reopen the
wire layout.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path
from typing import Any


class AdminError(ValueError):
    """Malformed lifecycle admin schema or extension closure."""


def load_json(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise AdminError(f"cannot load {path}: {error}") from error
    if not isinstance(value, dict):
        raise AdminError(f"{path}: expected a JSON object")
    return value


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def repository_root(path: Path) -> Path:
    for parent in (path, *path.parents):
        if (parent / "contracts").is_dir() and (parent / "tools").is_dir():
            return parent
    raise AdminError(f"cannot locate repository root from {path}")


def require_int(value: Any, label: str, minimum: int = 0) -> int:
    if not isinstance(value, int) or isinstance(value, bool) or value < minimum:
        raise AdminError(f"{label}: expected integer >= {minimum}")
    return value


def validate_record(record: dict[str, Any], path: Path, expected_name: str,
                    expected_size: int) -> None:
    if record.get("name") != expected_name:
        raise AdminError(f"{path}: expected record {expected_name}")
    size = require_int(record.get("size"), f"{path}:{expected_name}.size", 1)
    alignment = require_int(record.get("alignment"), f"{path}:{expected_name}.alignment", 1)
    if size != expected_size or alignment != 8:
        raise AdminError(f"{path}:{expected_name}: size/alignment is not canonical")
    fields = record.get("fields")
    if not isinstance(fields, list) or not fields:
        raise AdminError(f"{path}:{expected_name}: fields required")
    occupied: list[tuple[int, int]] = []
    for field in fields:
        if not isinstance(field, dict) or not isinstance(field.get("name"), str):
            raise AdminError(f"{path}:{expected_name}: malformed field")
        name = field["name"]
        offset = require_int(field.get("offset"), f"{path}:{name}.offset")
        width = require_int(field.get("width"), f"{path}:{name}.width", 1)
        end = offset + width
        if end > size or any(offset < other_end and other_offset < end
                             for other_offset, other_end in occupied):
            raise AdminError(f"{path}:{name}: overlaps or escapes record")
        occupied.append((offset, end))
    if max(end for _, end in occupied) != size:
        raise AdminError(f"{path}:{expected_name}: trailing padding is incomplete")


def validate(root: Path, extension_path: Path, admin_path: Path, model_path: Path) -> dict[str, Any]:
    extension = load_json(extension_path)
    admin = load_json(admin_path)
    model = load_json(model_path)

    if extension.get("id") != "transport.lifecycle-extension.v1" or extension.get("version") != "1.0":
        raise AdminError(f"{extension_path}: expected transport.lifecycle-extension.v1 version 1.0")
    if admin.get("id") != "transport.lifecycle-admin.v1" or admin.get("version") != "1.0":
        raise AdminError(f"{admin_path}: expected transport.lifecycle-admin.v1 version 1.0")
    if model.get("id") != "lifecycle.model.v1" or model.get("version") != "1.0":
        raise AdminError(f"{model_path}: expected lifecycle.model.v1 version 1.0")

    admin_ref = extension.get("admin")
    if not isinstance(admin_ref, dict):
        raise AdminError(f"{extension_path}: admin reference is required for freeze")
    expected_rel = admin_path.resolve().relative_to(root.resolve()).as_posix()
    if (admin_ref.get("id"), admin_ref.get("version"), admin_ref.get("path")) != (
        "transport.lifecycle-admin.v1",
        "1.0",
        expected_rel,
    ):
        raise AdminError(f"{extension_path}: admin reference metadata mismatch")
    actual = digest(admin_path)
    if admin_ref.get("sha256") != actual:
        raise AdminError(f"{extension_path}: admin schema hash mismatch: {actual}")

    records = admin.get("records")
    if not isinstance(records, list) or len(records) != 2:
        raise AdminError(f"{admin_path}: expected exactly two admin records")
    validate_record(records[0], admin_path, "mf_admin_lifecycle_request_v1", 80)
    validate_record(records[1], admin_path, "mf_admin_lifecycle_result_v1", 64)

    constants = admin.get("constants")
    if not isinstance(constants, dict) or constants.get("MF_ADMIN_LIFECYCLE_ABI_VERSION_1") != 1:
        raise AdminError(f"{admin_path}: ABI version constant missing")
    required = {
        "MF_ADMIN_LIFECYCLE_SOURCE_ADMIN": 1,
        "MF_ADMIN_LIFECYCLE_SOURCE_MEMFD": 2,
        "MF_ADMIN_LIFECYCLE_SOURCE_CDEV": 3,
        "MF_ADMIN_LIFECYCLE_SOURCE_VFIO_USER": 4,
        "MF_ADMIN_LIFECYCLE_SOURCE_QMP": 5,
        "MF_ADMIN_LIFECYCLE_SOURCE_DISCONNECT": 6,
        "MF_ADMIN_LIFECYCLE_SOURCE_RESTART": 7,
        "MF_ADMIN_LIFECYCLE_OP_ADD": 1,
        "MF_ADMIN_LIFECYCLE_OP_REMOVE": 2,
        "MF_ADMIN_LIFECYCLE_OP_RESET": 3,
        "MF_ADMIN_LIFECYCLE_OP_TRANSPORT_LOSS": 4,
        "MF_ADMIN_LIFECYCLE_OP_RECOVER": 5,
    }
    for key, value in required.items():
        if constants.get(key) != value:
            raise AdminError(f"{admin_path}: constant {key} is not canonical")

    # Model events must remain the operation set projected by the admin wire.
    model_events = {item.get("event") for item in model.get("transitions", [])
                    if isinstance(item, dict)}
    expected_events = {"add", "remove", "reset", "transport_loss", "recover"}
    if not expected_events.issubset(model_events):
        raise AdminError(f"{model_path}: model events do not cover admin operations")
    return admin


def header_text(admin: dict[str, Any]) -> str:
    constants = admin["constants"]
    lines = [
        "/* Generated by tools/validate-lifecycle-admin.py; do not edit. */",
        "#ifndef METAFLUX_LIFECYCLE_ADMIN_GENERATED_H",
        "#define METAFLUX_LIFECYCLE_ADMIN_GENERATED_H",
        "",
        "#include <stdint.h>",
        "",
    ]
    for name, value in sorted(constants.items()):
        lines.append(f"#define {name} UINT32_C({value})")
    lines.extend(
        [
            "",
            "typedef struct mf_admin_lifecycle_request_v1 {",
            "  uint32_t struct_size;",
            "  uint32_t abi_version;",
            "  uint64_t request_id;",
            "  uint64_t logical_device_id;",
            "  uint64_t daemon_incarnation;",
            "  uint64_t expected_identity_record_id;",
            "  uint64_t expected_generation;",
            "  uint64_t expected_epoch;",
            "  uint64_t deadline_tick;",
            "  uint32_t source;",
            "  uint32_t operation;",
            "  uint8_t reserved[8];",
            "} mf_admin_lifecycle_request_v1;",
            "",
            "typedef struct mf_admin_lifecycle_result_v1 {",
            "  uint32_t struct_size;",
            "  uint32_t abi_version;",
            "  uint64_t request_id;",
            "  uint32_t result;",
            "  uint32_t state;",
            "  uint64_t candidate_generation;",
            "  uint64_t committed_generation;",
            "  uint64_t committed_epoch;",
            "  uint64_t identity_record_id;",
            "  uint8_t reserved[8];",
            "} mf_admin_lifecycle_result_v1;",
            "",
            "#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L",
            "_Static_assert(sizeof(mf_admin_lifecycle_request_v1) == 80,",
            '               "admin lifecycle request size");',
            "_Static_assert(sizeof(mf_admin_lifecycle_result_v1) == 64,",
            '               "admin lifecycle result size");',
            "#endif",
            "",
            "#endif",
            "",
        ]
    )
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=None)
    parser.add_argument(
        "--extension",
        type=Path,
        default=Path("contracts/protocol/transport/v1/schema/extensions/lifecycle/v1/manifest.json"),
    )
    parser.add_argument(
        "--admin",
        type=Path,
        default=Path("contracts/protocol/transport/v1/schema/extensions/lifecycle/v1/admin.json"),
    )
    parser.add_argument(
        "--model",
        type=Path,
        default=Path("contracts/protocol/transport/v1/schema/extensions/lifecycle/v1/model.json"),
    )
    parser.add_argument("--generate-header", type=Path)
    arguments = parser.parse_args()
    root = arguments.root.resolve() if arguments.root is not None else repository_root(Path(__file__))
    extension = arguments.extension if arguments.extension.is_absolute() else root / arguments.extension
    admin_path = arguments.admin if arguments.admin.is_absolute() else root / arguments.admin
    model = arguments.model if arguments.model.is_absolute() else root / arguments.model
    try:
        admin = validate(root, extension.resolve(), admin_path.resolve(), model.resolve())
        if arguments.generate_header is not None:
            output = arguments.generate_header.resolve()
            output.parent.mkdir(parents=True, exist_ok=True)
            output.write_text(header_text(admin), encoding="utf-8")
    except AdminError as error:
        print(f"lifecycle-admin: invalid: {error}", file=sys.stderr)
        return 1
    print(f"lifecycle-admin: ok ({admin['id']} {admin['version']})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
