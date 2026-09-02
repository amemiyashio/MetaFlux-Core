#!/usr/bin/env python3
"""Self-test the lifecycle checker with valid and tampered inputs."""

from __future__ import annotations

import hashlib
import json
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
CHECKER = ROOT / "tools" / "check-lifecycle-model.py"
BASE = ROOT / "contracts/protocol/transport/v1/schema/manifest.json"
EXTENSION = ROOT / "contracts/protocol/transport/v1/schema/extensions/lifecycle/v1/manifest.json"
MODEL = ROOT / "contracts/protocol/transport/v1/schema/extensions/lifecycle/v1/model.json"
BOUNDS = ROOT / "tests/lifecycle/model-bounds.json"


def command(extension: Path, output: Path, model: Path = MODEL, bounds: Path = BOUNDS) -> list[str]:
    return [
        sys.executable,
        str(CHECKER),
        "--base-manifest",
        str(BASE),
        "--manifest",
        str(extension),
        "--model",
        str(model),
        "--bounds",
        str(bounds),
        "--output",
        str(output),
    ]


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="metaflux-lifecycle-check-", dir=ROOT) as directory:
        temporary = Path(directory)
        valid_output = temporary / "valid.json"
        valid = subprocess.run(command(EXTENSION, valid_output), cwd=ROOT, text=True,
                               capture_output=True, check=False)
        if valid.returncode != 0 or not valid_output.is_file():
            print(valid.stdout, end="")
            print(valid.stderr, end="", file=sys.stderr)
            return 1
        evidence = json.loads(valid_output.read_text(encoding="utf-8"))
        publication = evidence.get("publication", {})
        if (evidence.get("status") != "pass" or evidence.get("counterexamples") != [] or
                publication.get("scenario_checks") != 6 or
                publication.get("exploration", {}).get("state_count", 0) <= 0):
            return 1

        tampered_model = temporary / "tampered-publication-model.json"
        model_document = json.loads(MODEL.read_text(encoding="utf-8"))
        model_document["publication_model"]["stale_online_forbidden"] = False
        tampered_model.write_text(json.dumps(model_document), encoding="utf-8")
        tampered_extension = temporary / "tampered-publication-extension.json"
        extension_document = json.loads(EXTENSION.read_text(encoding="utf-8"))
        extension_document["model"]["path"] = tampered_model.relative_to(ROOT).as_posix()
        extension_document["model"]["sha256"] = hashlib.sha256(tampered_model.read_bytes()).hexdigest()
        tampered_extension.write_text(json.dumps(extension_document), encoding="utf-8")
        invalid_model = subprocess.run(
            command(tampered_extension, temporary / "invalid-publication-model.json", model=tampered_model),
            cwd=ROOT, text=True, capture_output=True, check=False
        )
        if invalid_model.returncode == 0 or "stale ONLINE" not in invalid_model.stderr:
            return 1

        tampered_contract_model = temporary / "tampered-contract-model.json"
        contract_document = json.loads(MODEL.read_text(encoding="utf-8"))
        contract_document["transitions"][0]["owner"] = ""
        tampered_contract_model.write_text(json.dumps(contract_document), encoding="utf-8")
        tampered_contract_extension = temporary / "tampered-contract-extension.json"
        contract_extension_document = json.loads(EXTENSION.read_text(encoding="utf-8"))
        contract_extension_document["model"]["path"] = tampered_contract_model.relative_to(ROOT).as_posix()
        contract_extension_document["model"]["sha256"] = hashlib.sha256(
            tampered_contract_model.read_bytes()
        ).hexdigest()
        tampered_contract_extension.write_text(
            json.dumps(contract_extension_document), encoding="utf-8"
        )
        invalid_contract = subprocess.run(
            command(
                tampered_contract_extension,
                temporary / "invalid-contract-model.json",
                model=tampered_contract_model,
            ),
            cwd=ROOT,
            text=True,
            capture_output=True,
            check=False,
        )
        if (invalid_contract.returncode == 0 or
                "ownership, guard, deadline, or commit contract" not in invalid_contract.stderr):
            return 1

        tampered_bounds = temporary / "tampered-publication-bounds.json"
        bounds_document = json.loads(BOUNDS.read_text(encoding="utf-8"))
        bounds_document["fence_telemetry"]["initial"]["telemetry_latch"] = 3
        tampered_bounds.write_text(json.dumps(bounds_document), encoding="utf-8")
        invalid_bounds = subprocess.run(
            command(EXTENSION, temporary / "invalid-publication-bounds.json", bounds=tampered_bounds),
            cwd=ROOT, text=True, capture_output=True, check=False
        )
        if invalid_bounds.returncode == 0 or "initial latches" not in invalid_bounds.stderr:
            return 1

        tampered = temporary / "tampered-manifest.json"
        document = json.loads(EXTENSION.read_text(encoding="utf-8"))
        document["imports"][0]["sha256"] = "0" * 64
        tampered.write_text(json.dumps(document), encoding="utf-8")
        invalid = subprocess.run(command(tampered, temporary / "invalid.json"), cwd=ROOT,
                                 text=True, capture_output=True, check=False)
        if invalid.returncode == 0 or "hash mismatch" not in invalid.stderr:
            return 1
    print("lifecycle model checker self-test: 6/6 passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
