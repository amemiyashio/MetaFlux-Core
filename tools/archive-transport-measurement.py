#!/usr/bin/env python3
"""Build an offline transport measurement archive skeleton.

Validates the canonical milestone-0.1.1.0 measurement contract and writes a
fingerprint-ready archive document. Live latency samples remain host-gated; this
tool freezes the archive schema and contract binding used by release rows.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import platform
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CONTRACT = ROOT / "tests/performance/milestone-0.1.1.0-measurement.json"
VALIDATOR = ROOT / "tools/validate-transport-measurement.py"


class ArchiveError(RuntimeError):
    """Archive construction failed."""


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load_validator():
    import importlib.util

    spec = importlib.util.spec_from_file_location("metaflux_transport_measurement", VALIDATOR)
    if spec is None or spec.loader is None:
        raise ArchiveError("cannot load validate-transport-measurement.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--contract", type=Path, default=DEFAULT_CONTRACT)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument(
        "--lifecycle-core",
        action="store_true",
        help="mark the archive as lifecycle-core enabled (still offline)",
    )
    parser.add_argument(
        "--note",
        action="append",
        default=[],
        help="optional free-form note retained in the archive",
    )
    return parser.parse_args()


def build_archive(contract_path: Path, lifecycle_core: bool, notes: list[str]) -> dict[str, Any]:
    if not contract_path.is_file():
        raise ArchiveError(f"missing measurement contract: {contract_path}")
    validator = load_validator()
    contract = validator.load_json(contract_path) if hasattr(validator, "load_json") else json.loads(
        contract_path.read_text(encoding="utf-8")
    )
    if hasattr(validator, "validate"):
        validator.validate(contract, contract_path)
    elif hasattr(validator, "validate_document"):
        validator.validate_document(contract, contract_path)
    else:
        # Fall back to CLI validator behavior via required fields already checked by self-test.
        if contract.get("id") != "transport.measurement.v0":
            raise ArchiveError("unexpected measurement contract id")

    return {
        "id": "transport.measurement-archive.v0",
        "status": "skeleton",
        "scope": "host-independent-schema",
        "created_utc": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "lifecycle_core_enabled": lifecycle_core,
        "contract": {
            "path": contract_path.resolve().relative_to(ROOT).as_posix()
            if contract_path.resolve().is_relative_to(ROOT)
            else contract_path.as_posix(),
            "sha256": digest(contract_path),
            "id": contract.get("id"),
            "version": contract.get("version"),
            "workloads": [entry.get("id") for entry in contract.get("workloads", [])],
        },
        "fingerprints": {
            "kernel_release": platform.release(),
            "machine": platform.machine(),
            "python": platform.python_version(),
            "platform": platform.platform(),
            "qemu": None,
            "compiler": None,
            "cpu_model": None,
            "topology": None,
            "note": "live fingerprint fields remain null until host qualification fills them",
        },
        "samples": {
            "status": "not-collected",
            "warmup_count": contract.get("warmup", {}).get("count"),
            "sample_count": contract.get("samples", {}).get("count"),
            "retain_raw": contract.get("samples", {}).get("retain_raw"),
            "summaries": contract.get("samples", {}).get("summaries"),
        },
        "notes": notes,
        "host_pending": [
            "collect CLOCK_MONOTONIC_RAW raw samples on reference hosts",
            "fill qemu/compiler/cpu/topology fingerprints",
            "compare lifecycle-core enabled vs disabled latency archives",
        ],
    }


def main() -> int:
    arguments = parse_arguments()
    try:
        archive = build_archive(
            arguments.contract.resolve(),
            arguments.lifecycle_core,
            list(arguments.note),
        )
    except Exception as error:  # noqa: BLE001
        print(f"archive-transport-measurement: {error}", file=sys.stderr)
        return 1
    arguments.output.parent.mkdir(parents=True, exist_ok=True)
    arguments.output.write_text(json.dumps(archive, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(
        json.dumps(
            {
                "status": "ok",
                "archive": archive["id"],
                "lifecycle_core_enabled": archive["lifecycle_core_enabled"],
            },
            sort_keys=True,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
