#!/usr/bin/env python3
"""Validate and optionally emit the versioned PyTorch CUDA provider matrix."""

from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import sys


SYMBOL_RE = re.compile(
    r"^MF_CUDA_SYMBOL\(([^,]+),\s*([^,]+),\s*([^,]+),\s*([^,]+),",
    re.MULTILINE,
)
UUID_RE = re.compile(
    r"static const unsigned char mf_uuid_([a-z0-9]+)\[16\]\s*=\s*\{(.*?)\};",
    re.DOTALL,
)
EXPECTED_HANDLE_CASES = {
    "repeated-live-lookup",
    "invalid-module",
    "destroyed-module",
    "stale-generation",
    "cross-module-name-collision",
    "duplicate-teardown",
    "capacity-reuse",
}


def fail(message: str) -> None:
    raise ValueError(message)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def function_body(source: str, name: str) -> str | None:
    match = re.search(rf"\bCUresult\s+{re.escape(name)}\s*\(", source)
    if match is None:
        return None
    opening = source.find("{", match.end())
    if opening < 0:
        return None
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1 : index]
    return None


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path)
    parser.add_argument("--profile", type=Path)
    parser.add_argument("--json", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    root = (args.root or Path(__file__).resolve().parents[6]).resolve()
    profile_path = args.profile or root / "plugins/compat/cuda/abi/driver/profiles/pytorch-cuda-cpu-v1.json"
    profile = json.loads(profile_path.read_text(encoding="utf-8"))
    if profile.get("schema_version") != 1:
        fail("profile schema_version must be 1")

    surface = profile["driver_surface"]
    symbol_path = root / surface["authority"]
    symbol_text = symbol_path.read_text(encoding="utf-8")
    rows = [
        {
            "name": name.strip(),
            "minimum_cuda_api_version": int(version.strip()),
            "status": status.strip(),
            "route": route.strip(),
        }
        for name, version, status, route in SYMBOL_RE.findall(symbol_text)
    ]
    names = [row["name"] for row in rows]
    if len(rows) != surface["expected_rows"] or len(names) != len(set(names)):
        fail("driver symbol row count or uniqueness does not match the profile")
    if Counter(row["status"] for row in rows) != Counter(surface["status_counts"]):
        fail("driver status counts do not match the profile")
    if Counter(row["route"] for row in rows) != Counter(surface["route_counts"]):
        fail("driver route counts do not match the profile")
    if any(row["minimum_cuda_api_version"] > surface["cuda_api_version"] for row in rows):
        fail("driver surface contains an entry newer than the pinned CUDA API")

    source_paths = sorted((symbol_path.parent / "src").glob("provider*.c"))
    source = "\n".join(path.read_text(encoding="utf-8") for path in source_paths)
    for row in rows:
        body = function_body(source, row["name"])
        if body is None:
            fail(f"missing managed definition for {row['name']}")
        if row["status"] == "TYPED_STUB":
            if "return CUDA_ERROR_NOT_SUPPORTED;" not in body or "return CUDA_SUCCESS;" in body:
                fail(f"typed stub {row['name']} does not fail stably as unsupported")

    index_path = root / surface["header_index"]
    header_index = json.loads(index_path.read_text(encoding="utf-8"))
    selected = [
        entry for entry in header_index["manifests"]
        if entry["driver_family"] == surface["driver_family"]
    ]
    if len(selected) != 1:
        fail("pinned header family is missing or duplicated")
    header_path = root / surface["header_manifest"]
    if selected[0]["path"] != surface["header_manifest"]:
        fail("header index path does not match the profile")
    if sha256(header_path) != surface["header_manifest_sha256"] or selected[0]["sha256"] != surface["header_manifest_sha256"]:
        fail("pinned header manifest digest does not match the profile")
    header = json.loads(header_path.read_text(encoding="utf-8"))
    if header["driver_family"] != surface["driver_family"] or header["cuda_api_version"] != surface["cuda_api_version"]:
        fail("pinned header identity does not match the profile")

    stub_path = symbol_path.parent / "src/provider_stubs.c"
    stub_text = stub_path.read_text(encoding="utf-8")
    parsed_uuids: dict[str, str] = {}
    for table_id, body in UUID_RE.findall(stub_text):
        octets = bytes(int(value, 16) for value in re.findall(r"0x([0-9a-fA-F]{2})", body))
        if len(octets) != 16:
            fail(f"internal table {table_id} does not contain 16 UUID octets")
        parsed_uuids[table_id] = (
            octets[:4].hex() + "-" + octets[4:6].hex() + "-" + octets[6:8].hex() + "-" +
            octets[8:10].hex() + "-" + octets[10:].hex()
        )
    table_ids = [table["id"] for table in profile["internal_tables"]]
    if len(table_ids) != 6 or len(set(table_ids)) != 6:
        fail("the profile must classify exactly six unique internal tables")
    for table in profile["internal_tables"]:
        expected_uuid = table.get("uuid")
        if expected_uuid is None:
            octets = bytes(int(value, 16) for value in table["uuid_bytes"])
            if len(octets) != 16:
                fail(f"profile UUID for {table['id']} does not contain 16 octets")
            expected_uuid = (
                octets[:4].hex() + "-" + octets[4:6].hex() + "-" + octets[6:8].hex() + "-" +
                octets[8:10].hex() + "-" + octets[10:].hex()
            )
        if parsed_uuids.get(table["id"]) != expected_uuid:
            fail(f"internal table UUID drift for {table['id']}")
        handler = table.get("default_handler")
        if handler is not None and handler not in stub_text:
            fail(f"internal table default handler is missing for {table['id']}")
        client_sha = table.get("client_library_sha256")
        if client_sha is not None and client_sha not in stub_text:
            fail(f"internal table client-library digest is missing for {table['id']}")
        for entry in table["entries"]:
            if entry["classification"] not in {"observed", "strengthened"}:
                fail(f"invalid internal-table classification for {table['id']}")
            index = entry.get("slot", entry.get("nested_slot", entry.get("u32_index")))
            assignment = re.search(
                rf"{re.escape(entry['array'])}\[{index}\]\s*=\s*([^;]+);", stub_text
            )
            expected = str(entry.get("binding", entry.get("value")))
            if assignment is None or expected not in assignment.group(1):
                fail(f"internal table binding drift for {table['id']} entry {index}")

    declared_cases = {case["id"] for case in profile["handle_cases"]}
    if declared_cases != EXPECTED_HANDLE_CASES:
        fail("handle case matrix is incomplete")
    test_text = (symbol_path.parent / "tests/provider_test.c").read_text(encoding="utf-8")
    tested_cases = set(re.findall(r"MF_HANDLE_CASE:\s*([a-z0-9-]+)", test_text))
    tested_cases.update(
        case.strip()
        for group in re.findall(r"MF_HANDLE_CASE:\s*([^*\n]+)", test_text)
        for case in group.split(",")
    )
    if not EXPECTED_HANDLE_CASES.issubset(tested_cases):
        fail(f"handle tests are missing markers: {sorted(EXPECTED_HANDLE_CASES - tested_cases)}")

    matrix = {
        "schema_version": profile["schema_version"],
        "profile_id": profile["profile_id"],
        "client": profile["client"],
        "driver_surface": rows,
        "internal_tables": profile["internal_tables"],
        "handle_cases": profile["handle_cases"],
    }
    if args.json:
        json.dump(matrix, sys.stdout, indent=2, sort_keys=True)
        sys.stdout.write("\n")
    else:
        print(
            f"surface profile: ok profile={profile['profile_id']} "
            f"symbols={len(rows)} internal_tables={len(profile['internal_tables'])} "
            f"handle_cases={len(profile['handle_cases'])}"
        )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (KeyError, TypeError, ValueError, json.JSONDecodeError) as error:
        print(f"surface profile: error: {error}", file=sys.stderr)
        raise SystemExit(1)
