#!/usr/bin/env python3
"""Self-test the lifecycle checker with valid and tampered inputs."""

from __future__ import annotations

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


def command(extension: Path, output: Path) -> list[str]:
    return [
        sys.executable,
        str(CHECKER),
        "--base-manifest",
        str(BASE),
        "--manifest",
        str(extension),
        "--model",
        str(MODEL),
        "--bounds",
        str(BOUNDS),
        "--output",
        str(output),
    ]


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="metaflux-lifecycle-check-") as directory:
        temporary = Path(directory)
        valid_output = temporary / "valid.json"
        valid = subprocess.run(command(EXTENSION, valid_output), cwd=ROOT, text=True,
                               capture_output=True, check=False)
        if valid.returncode != 0 or not valid_output.is_file():
            print(valid.stdout, end="")
            print(valid.stderr, end="", file=sys.stderr)
            return 1
        evidence = json.loads(valid_output.read_text(encoding="utf-8"))
        if evidence.get("status") != "pass" or evidence.get("counterexamples") != []:
            return 1

        tampered = temporary / "tampered-manifest.json"
        document = json.loads(EXTENSION.read_text(encoding="utf-8"))
        document["imports"][0]["sha256"] = "0" * 64
        tampered.write_text(json.dumps(document), encoding="utf-8")
        invalid = subprocess.run(command(tampered, temporary / "invalid.json"), cwd=ROOT,
                                 text=True, capture_output=True, check=False)
        if invalid.returncode == 0 or "hash mismatch" not in invalid.stderr:
            return 1
    print("lifecycle model checker self-test: 2/2 passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
