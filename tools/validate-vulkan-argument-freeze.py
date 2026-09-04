#!/usr/bin/env python3
"""Record the host-independent packed-argument ABI freeze evidence."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
ARGUMENTS_H = ROOT / "contracts/plugin/backend/v1/include/metaflux/backend/vulkan_arguments.h"
ARGUMENTS_TEST = ROOT / "contracts/plugin/backend/v1/tests/vulkan_arguments.c"
BACKEND_README = ROOT / "plugins/backend/vulkan/README.md"
LOWERING = ROOT / "plugins/backend/vulkan/compiler/src/lowering.cpp"


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def require(path: Path) -> None:
    if not path.is_file():
        raise SystemExit(f"missing required freeze input: {path}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=None)
    arguments = parser.parse_args()

    for path in (ARGUMENTS_H, ARGUMENTS_TEST, BACKEND_README, LOWERING):
        require(path)

    header = ARGUMENTS_H.read_text(encoding="utf-8")
    required_tokens = (
        "MF_VULKAN_ARGUMENT_ABI_VERSION_1",
        "MF_VULKAN_ARGUMENT_BLOCK_MAGIC_V1",
        "mf_vulkan_argument_block_header_v1",
        "mf_vulkan_argument_entry_v1",
        "mf_vulkan_argument_block_validate_v1",
        "MF_VULKAN_ARGUMENT_KIND_DEVICE_ADDRESS_V1",
    )
    for token in required_tokens:
        if token not in header:
            raise SystemExit(f"argument header missing freeze token: {token}")

    # Layout lock: 64-byte header and 48-byte entry are the freeze constants.
    if "uint8_t target_digest[32]" not in header:
        raise SystemExit("argument header missing 32-byte target digest")
    header_fields = (
        "uint32_t magic",
        "uint32_t abi_version",
        "uint32_t header_size",
        "uint32_t entry_size",
        "uint32_t entry_count",
        "uint32_t flags",
        "uint64_t total_size",
    )
    for field in header_fields:
        if field not in header:
            raise SystemExit(f"argument header missing field: {field}")

    lowering = LOWERING.read_text(encoding="utf-8")
    if "mf_vulkan_argument_block_size_v1" not in lowering:
        raise SystemExit("lowering does not bind packed argument block size")
    if "argument_target_digest" not in lowering:
        raise SystemExit("lowering does not bind argument target digest")

    readme = BACKEND_README.read_text(encoding="utf-8")
    if "packed argument" not in readme.lower() and "packed argument" not in readme:
        if "vulkan_arguments.h" not in readme:
            raise SystemExit("backend README does not document packed arguments")

    # Detect ABI version constant value remains 1.
    match = re.search(r"MF_VULKAN_ARGUMENT_ABI_VERSION_1\s+UINT32_C\((\d+)\)", header)
    if match is None or match.group(1) != "1":
        raise SystemExit("packed argument ABI version is not locked to 1")

    report = {
        "id": "vulkan.packed-argument-freeze.v1",
        "status": "frozen-host-independent",
        "abi_version": 1,
        "layout": {
            "header_bytes": 64,
            "entry_bytes": 48,
            "kinds": ["scalar", "device_address"],
            "device_address_requires_nonzero_generation": True,
            "target_digest_bytes": 32,
        },
        "evidence": {
            "header": ARGUMENTS_H.relative_to(ROOT).as_posix(),
            "header_sha256": digest(ARGUMENTS_H),
            "contract_test": ARGUMENTS_TEST.relative_to(ROOT).as_posix(),
            "contract_test_sha256": digest(ARGUMENTS_TEST),
            "lowering": LOWERING.relative_to(ROOT).as_posix(),
            "lowering_sha256": digest(LOWERING),
            "ctest": [
                "metaflux.contract.backend-vulkan-arguments.v1",
                "metaflux.backend.vulkan-lowering (when vulkan preset enabled)",
            ],
        },
        "host_pending_non_reopening": [
            "physical dual-driver Add/Copy/barrier with validation layers",
            "reset/device-loss validation-layer soak before release promotion",
        ],
        "policy": {
            "dual_driver_does_not_reopen_layout": True,
            "product_semver_promotion_requires_dual_driver": True,
        },
    }
    if arguments.output is not None:
        arguments.output.parent.mkdir(parents=True, exist_ok=True)
        arguments.output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps({"status": "ok", "freeze": report["id"]}, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
