#!/usr/bin/env python3
"""Validate the milestone-0.1.1.0 transport schema and emit deterministic C projections.

The transport manifest is intentionally small and explicit.  It names every
definition in the v0 base closure together with its content digest; consumers
build from this projection rather than maintaining private wire layouts.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
import sys
from pathlib import Path
from typing import Any


TYPE_SIZES = {"u16": 2, "u32": 4, "u64": 8, "s32": 4}
IDENTIFIER = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")


class SchemaError(ValueError):
    """A malformed or inconsistent schema document."""


def load_json(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise SchemaError(f"cannot load {path}: {error}") from error
    if not isinstance(value, dict):
        raise SchemaError(f"{path} must contain a JSON object")
    return value


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def records(document: dict[str, Any], path: Path) -> list[dict[str, Any]]:
    raw = document.get("records")
    if raw is None and isinstance(document.get("record"), dict):
        raw = [document["record"]]
    if not isinstance(raw, list) or not raw:
        raise SchemaError(f"{path}: records must be a non-empty array")
    result: list[dict[str, Any]] = []
    for record in raw:
        if not isinstance(record, dict):
            raise SchemaError(f"{path}: each record must be an object")
        name = record.get("name")
        if not isinstance(name, str) or not IDENTIFIER.fullmatch(name):
            raise SchemaError(f"{path}: record name is not a C identifier: {name!r}")
        size = record.get("size")
        alignment = record.get("alignment")
        fields = record.get("fields")
        if not isinstance(size, int) or size <= 0:
            raise SchemaError(f"{path}:{name}: size must be positive")
        if not isinstance(alignment, int) or alignment <= 0 or alignment & (alignment - 1):
            raise SchemaError(f"{path}:{name}: alignment must be a power of two")
        if not isinstance(fields, list) or not fields:
            raise SchemaError(f"{path}:{name}: fields must be a non-empty array")
        occupied: list[tuple[int, int, str]] = []
        seen: set[str] = set()
        for field in fields:
            if not isinstance(field, dict):
                raise SchemaError(f"{path}:{name}: field must be an object")
            field_name = field.get("name")
            offset = field.get("offset")
            width = field.get("width")
            field_type = field.get("type")
            if not isinstance(field_name, str) or not IDENTIFIER.fullmatch(field_name):
                raise SchemaError(f"{path}:{name}: invalid field name {field_name!r}")
            if field_name in seen:
                raise SchemaError(f"{path}:{name}: duplicate field {field_name}")
            seen.add(field_name)
            if not isinstance(offset, int) or offset < 0:
                raise SchemaError(f"{path}:{name}.{field_name}: invalid offset")
            if not isinstance(width, int) or width <= 0:
                raise SchemaError(f"{path}:{name}.{field_name}: invalid width")
            if not isinstance(field_type, str):
                raise SchemaError(f"{path}:{name}.{field_name}: missing type")
            end = offset + width
            if end > size:
                raise SchemaError(f"{path}:{name}.{field_name}: extends past record size")
            if field.get("reserved") and not field_name.startswith("reserved"):
                raise SchemaError(f"{path}:{name}.{field_name}: reserved field must be named reserved*")
            for old_offset, old_end, old_name in occupied:
                if offset < old_end and old_offset < end:
                    raise SchemaError(f"{path}:{name}: fields {old_name} and {field_name} overlap")
            occupied.append((offset, end, field_name))
        record_copy = dict(record)
        record_copy["_path"] = str(path)
        result.append(record_copy)
    return result


def load_manifest(root: Path, manifest_path: Path) -> tuple[dict[str, Any], list[tuple[Path, dict[str, Any], list[dict[str, Any]]]]]:
    manifest = load_json(manifest_path)
    if manifest.get("id") != "transport.base.v0" or manifest.get("version") != "0.1":
        raise SchemaError(f"{manifest_path}: expected transport.base.v0 version 0.1")
    definitions = manifest.get("definitions")
    if not isinstance(definitions, list) or not definitions:
        raise SchemaError(f"{manifest_path}: definitions must be a non-empty array")
    seen_paths: set[str] = set()
    seen_ids: set[str] = set()
    loaded: list[tuple[Path, dict[str, Any], list[dict[str, Any]]]] = []
    for entry in definitions:
        if not isinstance(entry, dict):
            raise SchemaError(f"{manifest_path}: definition entry must be an object")
        rel = entry.get("path")
        expected_hash = entry.get("sha256")
        if not isinstance(rel, str) or Path(rel).is_absolute() or rel in seen_paths:
            raise SchemaError(f"{manifest_path}: duplicate or invalid definition path {rel!r}")
        if not isinstance(expected_hash, str) or not re.fullmatch(r"[0-9a-f]{64}", expected_hash):
            raise SchemaError(f"{manifest_path}: invalid sha256 for {rel}")
        seen_paths.add(rel)
        path = (root / rel).resolve()
        if root.resolve() not in path.parents:
            raise SchemaError(f"{manifest_path}: definition escapes repository: {rel}")
        if not path.is_file():
            raise SchemaError(f"{manifest_path}: missing definition {rel}")
        actual_hash = digest(path)
        if actual_hash != expected_hash:
            raise SchemaError(f"{manifest_path}: digest mismatch for {rel}: {actual_hash}")
        document = load_json(path)
        definition_id = document.get("id")
        if not isinstance(definition_id, str) or definition_id in seen_ids:
            raise SchemaError(f"{manifest_path}: duplicate definition id {definition_id!r}")
        if entry.get("id") != definition_id or entry.get("version") != document.get("version"):
            raise SchemaError(f"{manifest_path}: manifest metadata mismatch for {rel}")
        seen_ids.add(definition_id)
        loaded.append((path, document, records(document, path)))
    return manifest, loaded


def macro_name(name: str, suffix: str) -> str:
    return "MF_SCHEMA_" + re.sub(r"[^A-Za-z0-9]", "_", name).upper() + suffix


def c_type(field_type: str, width: int) -> str:
    if field_type in TYPE_SIZES:
        if TYPE_SIZES[field_type] != width:
            raise SchemaError(f"type {field_type} width mismatch: {width}")
        return {"u16": "MF_SCHEMA_U16", "u32": "MF_SCHEMA_U32", "u64": "MF_SCHEMA_U64", "s32": "MF_SCHEMA_S32"}[field_type]
    match = re.fullmatch(r"(bytes|u64)\[(\d+)\]", field_type)
    if match:
        base, count_text = match.groups()
        count = int(count_text)
        unit = 1 if base == "bytes" else 8
        if count * unit != width:
            raise SchemaError(f"type {field_type} width mismatch: {width}")
        return f"{'MF_SCHEMA_U8' if base == 'bytes' else 'MF_SCHEMA_U64'} {count}"
    raise SchemaError(f"unsupported C field type {field_type!r}")


def emit_field_lines(record: dict[str, Any]) -> list[str]:
    lines: list[str] = []
    cursor = 0
    padding_index = 0
    for field in sorted(record["fields"], key=lambda item: item["offset"]):
        offset = field["offset"]
        if offset > cursor:
            lines.append(f"  MF_SCHEMA_U8 _padding_{padding_index}[{offset - cursor}];")
            padding_index += 1
        type_text = c_type(field["type"], field["width"])
        if " " in type_text and type_text.rsplit(" ", 1)[0] in {"MF_SCHEMA_U8", "MF_SCHEMA_U64"}:
            base, count = type_text.rsplit(" ", 1)
            lines.append(f"  {base} {field['name']}[{count}];")
        else:
            lines.append(f"  {type_text} {field['name']};")
        cursor = offset + field["width"]
    if cursor < record["size"]:
        lines.append(f"  MF_SCHEMA_U8 _padding_{padding_index}[{record['size'] - cursor}];")
    return lines


def pack_at(buffer: bytearray, offset: int, field_type: str, value: Any) -> None:
    if value is None and field_type in {"u16", "u32", "u64", "s32"}:
        value = offset + 1
    if field_type == "u16":
        buffer[offset : offset + 2] = struct.pack("<H", int(value))
    elif field_type == "u32":
        buffer[offset : offset + 4] = struct.pack("<I", int(value))
    elif field_type == "u64":
        buffer[offset : offset + 8] = struct.pack("<Q", int(value))
    elif field_type == "s32":
        buffer[offset : offset + 4] = struct.pack("<i", int(value))
    else:
        match = re.fullmatch(r"(bytes|u64)\[(\d+)\]", field_type)
        if match is None:
            raise SchemaError(f"unsupported golden field type {field_type!r}")
        base, count_text = match.groups()
        count = int(count_text)
        if base == "bytes":
            if value is None:
                value = bytes((index + offset) & 0xFF for index in range(count))
            if len(value) != count:
                raise SchemaError(f"golden value width mismatch for {field_type}")
            buffer[offset : offset + count] = value
        else:
            values = list(value or range(1, count + 1))
            if len(values) != count:
                raise SchemaError(f"golden value count mismatch for {field_type}")
            for index, item in enumerate(values):
                buffer[offset + index * 8 : offset + (index + 1) * 8] = struct.pack(
                    "<Q", int(item)
                )


def golden_name(record_name: str) -> str:
    if record_name == "mf_transport_negotiate_v0":
        return "mf_transport_negotiate_golden_v0"
    return "mf_schema_" + record_name.lower() + "_golden"


def emit_golden_array(record: dict[str, Any], name: str) -> list[str]:
    buffer = bytearray(record["size"])
    values: dict[str, Any] = {
        "magic": 0x3054464D,
        "major": 0,
        "minor": 1,
        "struct_size": record["size"],
        "required_features": 1,
        "daemon_incarnation": 0x0102030405060708,
        "view_serial": 9,
        "device_generation": 7,
        "descriptor_version": 1,
        "ring_version": 1,
        "max_queues": 2,
        "ring_order": 8,
        "dma_width": 64,
        "dma_alignment": 4096,
        "max_regions": 64,
        "max_inflight": 256,
        "max_bytes": 0x10000000,
    }
    for field in record["fields"]:
        field_name = field["name"]
        if field_name in values:
            pack_at(buffer, field["offset"], field["type"], values[field_name])
        elif not field.get("reserved"):
            pack_at(buffer, field["offset"], field["type"], None)
    values_text = ", ".join(f"0x{byte:02x}" for byte in buffer)
    return [f"static const MF_SCHEMA_U8 {name}[{record['size']}] = {{", f"  {values_text}", "};"]


def emit_header(root: Path, manifest_path: Path, manifest: dict[str, Any], loaded: list[tuple[Path, dict[str, Any], list[dict[str, Any]]]], output: Path) -> None:
    manifest_hash = digest(manifest_path)
    lines = [
        "/* Generated by tools/validate-transport-schema.py; do not edit. */",
        "#ifndef METAFLUX_TRANSPORT_SCHEMA_GENERATED_H",
        "#define METAFLUX_TRANSPORT_SCHEMA_GENERATED_H",
        "",
        "#ifdef __KERNEL__",
        "#include <linux/stddef.h>",
        "#include <linux/ioctl.h>",
        "#include <linux/types.h>",
        "#define MF_SCHEMA_U8 __u8",
        "#define MF_SCHEMA_U16 __u16",
        "#define MF_SCHEMA_U32 __u32",
        "#define MF_SCHEMA_U64 __u64",
        "#define MF_SCHEMA_S32 __s32",
        "#define MF_SCHEMA_ALIGNOF __alignof__",
        "#define MF_SCHEMA_STATIC_ASSERT static_assert",
        "#else",
        "#include <stddef.h>",
        "#include <stdint.h>",
        "#include <stdalign.h>",
        "#include <linux/ioctl.h>",
        '#include "metaflux/shared/device.h"',
        "#define MF_SCHEMA_U8 uint8_t",
        "#define MF_SCHEMA_U16 uint16_t",
        "#define MF_SCHEMA_U32 uint32_t",
        "#define MF_SCHEMA_U64 uint64_t",
        "#define MF_SCHEMA_S32 int32_t",
        "#define MF_SCHEMA_ALIGNOF _Alignof",
        "#ifdef __cplusplus",
        "#define MF_SCHEMA_STATIC_ASSERT static_assert",
        "#else",
        "#define MF_SCHEMA_STATIC_ASSERT _Static_assert",
        "#endif",
        "#endif",
        "",
        f'#define MF_TRANSPORT_SCHEMA_MANIFEST_SHA256 "{manifest_hash}"',
        "#define MF_TRANSPORT_SCHEMA_BYTE_ORDER_LITTLE 1",
        "",
    ]
    all_records: list[dict[str, Any]] = []
    emitted_record_names: set[str] = set()
    for _path, document, document_records in loaded:
        lines.append(f"/* {document['id']} {document['version']} */")
        for record in document_records:
            all_records.append(record)
            name = record["name"]
            if name in emitted_record_names:
                continue
            emitted_record_names.add(name)
            lines.append(f"#define {macro_name(name, '_SIZE')} {record['size']}u")
            lines.append(f"#define {macro_name(name, '_ALIGNMENT')} {record['alignment']}u")
            for field in record["fields"]:
                lines.append(f"#define {macro_name(name, '_OFFSET_' + field['name'])} {field['offset']}u")
            if document.get("kind") not in {"inherited-layout"} and not record.get("inherited"):
                lines.append("")
                lines.append(f"typedef struct {name} {{")
                lines.extend(emit_field_lines(record))
                lines.append(f"}} {name};")
                lines.append(
                    f"#ifdef __cplusplus\nMF_SCHEMA_STATIC_ASSERT(alignof({name}) == {record['alignment']}u, \"{name} alignment\");\n#else\nMF_SCHEMA_STATIC_ASSERT(_Alignof({name}) == {record['alignment']}u, \"{name} alignment\");\n#endif"
                )
                lines.append(f"MF_SCHEMA_STATIC_ASSERT(sizeof({name}) == {record['size']}u, \"{name} size\");")
                for field in record["fields"]:
                    lines.append(
                        f"MF_SCHEMA_STATIC_ASSERT(offsetof({name}, {field['name']}) == {field['offset']}u, \"{name}.{field['name']} offset\");"
                    )
            elif document.get("kind") == "inherited-layout":
                lines.append(
                    f"#ifndef __KERNEL__\n#ifdef __cplusplus\nMF_SCHEMA_STATIC_ASSERT(alignof({name}) == {record['alignment']}u, \"{name} alignment\");\n#else\nMF_SCHEMA_STATIC_ASSERT(_Alignof({name}) == {record['alignment']}u, \"{name} alignment\");\n#endif\nMF_SCHEMA_STATIC_ASSERT(sizeof({name}) == {record['size']}u, \"{name} size\");"
                )
                for field in record["fields"]:
                    lines.append(
                        f"MF_SCHEMA_STATIC_ASSERT(offsetof({name}, {field['name']}) == {field['offset']}u, \"{name}.{field['name']} offset\");"
                    )
                lines.append("#endif")
            lines.append("")
        for constant_name, value in document.get("constants", {}).items():
            if isinstance(value, str):
                value = value.replace("<<", "<<")
                lines.append(f"#define {constant_name} ({value})")
            else:
                lines.append(f"#define {constant_name} {value}")
        for ioctl in document.get("ioctls", []):
            if not isinstance(ioctl, dict):
                raise SchemaError(f"{document['id']}: ioctl must be an object")
            record_name = ioctl.get("record")
            if record_name not in {record["name"] for record in document_records}:
                raise SchemaError(f"{document['id']}: ioctl references unknown record {record_name}")
            direction = {"readwrite": "_IOWR", "read": "_IOR", "write": "_IOW"}.get(ioctl.get("direction"))
            if direction is None:
                raise SchemaError(f"{document['id']}: invalid ioctl direction")
            lines.append(f"#define {ioctl['name']} {direction}('M', {int(ioctl['number'])}, {record_name})")
        lines.append("")
    lines.append("/* Canonical little-endian bytes used by C/C++ layout fixtures. */")
    emitted_golden_names: set[str] = set()
    for record in all_records:
        name = golden_name(record["name"])
        if name in emitted_golden_names:
            continue
        emitted_golden_names.add(name)
        lines.append(f"#define {macro_name(record['name'], '_GOLDEN_SIZE')} {record['size']}u")
        lines.extend(emit_golden_array(record, name))
        lines.append("")
    lines.append("#endif")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, default=Path("contracts/protocol/transport/v1/schema/manifest.json"))
    parser.add_argument("--generate-header", type=Path)
    args = parser.parse_args()
    root = args.root.resolve()
    manifest_path = (root / args.manifest).resolve() if not args.manifest.is_absolute() else args.manifest.resolve()
    try:
        manifest, loaded = load_manifest(root, manifest_path)
        if args.generate_header is not None:
            emit_header(root, manifest_path, manifest, loaded, args.generate_header.resolve())
    except (OSError, SchemaError) as error:
        print(f"transport schema: error: {error}", file=sys.stderr)
        return 1
    print(f"transport schema: ok ({len(loaded)} definitions, {sum(len(item[2]) for item in loaded)} records)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
