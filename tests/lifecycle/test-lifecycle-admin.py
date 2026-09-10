#!/usr/bin/env python3
"""Self-test the frozen mf_admin_lifecycle_v1 validator and header projection."""

from __future__ import annotations

import importlib.util
import json
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "tools/validate-lifecycle-admin.py"
EXTENSION = ROOT / "contracts/protocol/transport/v1/schema/extensions/lifecycle/v1/manifest.json"
ADMIN = ROOT / "contracts/protocol/transport/v1/schema/extensions/lifecycle/v1/admin.json"
MODEL = ROOT / "contracts/protocol/transport/v1/schema/extensions/lifecycle/v1/model.json"


def load_module():
    spec = importlib.util.spec_from_file_location("metaflux_lifecycle_admin", SCRIPT)
    if spec is None or spec.loader is None:
        raise AssertionError("lifecycle-admin validator cannot be loaded")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main() -> int:
    module = load_module()
    admin = module.validate(ROOT, EXTENSION, ADMIN, MODEL)
    assert admin["records"][0]["size"] == 80
    assert admin["records"][1]["size"] == 64
    assert admin["constants"]["MF_ADMIN_LIFECYCLE_OP_TRANSPORT_LOSS"] == 4

    with tempfile.TemporaryDirectory(prefix="metaflux-lifecycle-admin-", dir=ROOT) as directory:
        header = Path(directory) / "admin.h"
        header.write_text(module.header_text(admin), encoding="utf-8")
        text = header.read_text(encoding="utf-8")
        assert "mf_admin_lifecycle_request_v1" in text
        assert "sizeof(mf_admin_lifecycle_request_v1) == 80" in text
        assert "MF_ADMIN_LIFECYCLE_ABI_VERSION_1" in text

        # Tampered hash must fail.
        bad_manifest = Path(directory) / "bad-manifest.json"
        document = json.loads(EXTENSION.read_text(encoding="utf-8"))
        document["admin"]["sha256"] = "0" * 64
        bad_manifest.write_text(json.dumps(document), encoding="utf-8")
        try:
            module.validate(ROOT, bad_manifest, ADMIN, MODEL)
        except module.AdminError as error:
            assert "hash mismatch" in str(error)
        else:
            raise AssertionError("tampered admin hash was accepted")

    print("lifecycle-admin self-test: 3/3 passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
