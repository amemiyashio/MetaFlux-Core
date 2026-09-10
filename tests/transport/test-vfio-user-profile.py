#!/usr/bin/env python3
"""Self-test the canonical vfio-user BAR profile validator."""

from __future__ import annotations

import importlib.util
import json
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "tools/validate-vfio-user-profile.py"
SCHEMA = ROOT / "contracts/protocol/transport/v1/schema/vfio_user.json"
MANIFEST = ROOT / "contracts/protocol/transport/v1/schema/manifest.json"


def load_validator():
    spec = importlib.util.spec_from_file_location("metaflux_vfio_user_profile_validator", SCRIPT)
    if spec is None or spec.loader is None:
        raise AssertionError("vfio-user profile validator cannot be loaded")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main() -> int:
    validator = load_validator()
    profile = validator.validate(ROOT, MANIFEST)
    assert profile["bars"][0]["size"] == 65536
    assert profile["bars"][2]["offset"] == 69632
    with tempfile.TemporaryDirectory(prefix="metaflux-vfio-user-profile-", dir=ROOT) as directory:
        invalid = Path(directory) / "invalid-vfio-user.json"
        document = json.loads(SCHEMA.read_text(encoding="utf-8"))
        document["profile"]["bars"][1]["size"] = 8192
        invalid.write_text(json.dumps(document), encoding="utf-8")
        try:
            validator.validate_profile(invalid)
        except validator.ProfileError as error:
            assert "canonical layout" in str(error)
        else:
            raise AssertionError("tampered BAR profile was accepted")
    print("vfio-user profile self-test: 2/2 passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
