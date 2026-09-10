#!/usr/bin/env python3
"""Validate packaging-owned metaflux-backend-vulkan ownership metadata."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
NOTES = ROOT / "packaging/docs/metaflux-backend-vulkan.md"
CONTRACT_HEADERS = (
    ROOT / "contracts/plugin/backend/v1/include/metaflux/backend/vulkan.h",
    ROOT / "contracts/plugin/backend/v1/include/metaflux/backend/vulkan_arguments.h",
    ROOT / "contracts/plugin/backend/v1/include/metaflux/backend/vulkan_memory.h",
    ROOT / "contracts/plugin/backend/v1/include/metaflux/backend/api.h",
)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=None)
    arguments = parser.parse_args()

    if not NOTES.is_file():
        print("missing packaging notes", file=sys.stderr)
        return 1
    missing = [path.as_posix() for path in CONTRACT_HEADERS if not path.is_file()]
    if missing:
        print(f"missing backend contract headers: {missing}", file=sys.stderr)
        return 1

    text = NOTES.read_text(encoding="utf-8")
    for required in (
        "metaflux-backend-vulkan",
        "/nix/store",
        "coexist",
        "External-memory",
    ):
        if required not in text:
            print(f"packaging notes missing required token: {required}", file=sys.stderr)
            return 1

    report = {
        "package": "metaflux-backend-vulkan",
        "status": "ok",
        "notes": NOTES.relative_to(ROOT).as_posix(),
        "contract_headers": [path.relative_to(ROOT).as_posix() for path in CONTRACT_HEADERS],
        "policy": {
            "nix_store_runtime_forbidden": True,
            "vendor_libraries_not_installed": True,
            "idle_without_icd_allowed": True,
        },
    }
    if arguments.output is not None:
        arguments.output.parent.mkdir(parents=True, exist_ok=True)
        arguments.output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps({"status": "ok", "package": report["package"]}, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
