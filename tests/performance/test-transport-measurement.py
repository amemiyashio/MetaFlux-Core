#!/usr/bin/env python3
"""Self-test the milestone-0.1.1.0 measurement validator."""

from __future__ import annotations

import importlib.util
import json
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "tools/validate-transport-measurement.py"
CONTRACT = (
    ROOT
    / "tests"
    / "performance"
    / "milestone-0.1.1.0-measurement.json"
).resolve()


def load_module():
    spec = importlib.util.spec_from_file_location("metaflux_transport_measurement", SCRIPT)
    if spec is None or spec.loader is None:
        raise AssertionError("measurement validator cannot be loaded")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


VALIDATOR = load_module()


def main() -> int:
    document = json.loads(CONTRACT.read_text(encoding="utf-8"))
    VALIDATOR.validate(document, CONTRACT)
    document["samples"]["count"] = 999
    with tempfile.TemporaryDirectory(prefix="metaflux-measurement-") as temp:
        invalid = Path(temp) / "invalid.json"
        invalid.write_text(json.dumps(document), encoding="utf-8")
        try:
            VALIDATOR.validate(document, invalid)
        except VALIDATOR.MeasurementError as error:
            assert "samples count/unit" in str(error)
        else:
            raise AssertionError("invalid sample count was accepted")
    print("transport measurement self-test: 2/2 passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
