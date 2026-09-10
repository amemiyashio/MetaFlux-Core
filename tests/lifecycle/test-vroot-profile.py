#!/usr/bin/env python3
"""Self-test the canonical vroot profile validator."""

from __future__ import annotations

import hashlib
import importlib.util
import json
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "tools/validate-vroot-profile.py"
MANIFEST = ROOT / "contracts/protocol/transport/v1/schema/extensions/vroot/v1/manifest.json"


def load_validator():
    spec = importlib.util.spec_from_file_location("metaflux_vroot_profile_validator", SCRIPT)
    if spec is None or spec.loader is None:
        raise AssertionError("vroot profile validator cannot be loaded")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main() -> int:
    validator = load_validator()
    profile, image, writable = validator.validate(ROOT, MANIFEST)
    assert profile["config_size"] == 256
    assert image[0:4] == bytes((0x46, 0x4D, 0x01, 0x00))
    assert image[11] == 0x12
    assert len(image) == len(writable) == 256
    assert not any(writable)
    userspace_header = validator.header_text(profile, image, writable)
    kernel_header = validator.header_text(profile, image, writable, kernel=True)
    assert "#include <stdint.h>" in userspace_header
    assert "static const uint8_t mf_vroot_profile_config_template" in userspace_header
    assert "#include <linux/types.h>" in kernel_header
    assert "static const u8 mf_vroot_profile_config_template" in kernel_header

    with tempfile.TemporaryDirectory(prefix="metaflux-vroot-profile-", dir=ROOT) as directory:
        temporary = Path(directory)
        invalid_profile = temporary / "invalid-profile.json"
        source_profile = ROOT / "contracts/protocol/transport/v1/schema/extensions/vroot/v1/profile.json"
        profile_document = json.loads(source_profile.read_text(encoding="utf-8"))
        profile_document["identity"]["class_code"] = 0x020000
        invalid_profile.write_text(json.dumps(profile_document), encoding="utf-8")
        invalid_manifest = temporary / "invalid-manifest.json"
        manifest_document = json.loads(MANIFEST.read_text(encoding="utf-8"))
        manifest_document["profile"]["path"] = invalid_profile.relative_to(ROOT).as_posix()
        manifest_document["profile"]["sha256"] = hashlib.sha256(invalid_profile.read_bytes()).hexdigest()
        invalid_manifest.write_text(json.dumps(manifest_document), encoding="utf-8")
        try:
            validator.validate(ROOT, invalid_manifest)
        except validator.ProfileError as error:
            assert "identity" in str(error)
        else:
            raise AssertionError("tampered vroot identity was accepted")
    print("vroot profile self-test: 2/2 passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
