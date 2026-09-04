#!/usr/bin/env python3
"""Self-test the combined guest PCI identity + BAR fixture generator."""

from __future__ import annotations

import importlib.util
import re
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = Path(__file__).resolve().with_name("generate-pci-guest-profile.py")

# Consumers that previously hardcoded CI Type-0 identity / BAR sizes.
HANDWRITTEN_TARGETS = (
    ROOT / "kernel/pci/metaflux_pci_main.c",
    ROOT / "transports/vfio-user/live/src/metaflux_vfu_live_server.c",
)

# Raw literals that must not reappear as competing layout owners once the
# generated profile is the sole source. Trigger words that intentionally embed
# the vendor mnemonic remain allowed only when they do not redefine VID/DID/class
# or BAR sizes.
FORBIDDEN = (
    re.compile(r"#define\s+MF_PCI_VENDOR_ID\s+0x4[dD]46\b"),
    re.compile(r"#define\s+MF_PCI_DEVICE_ID\s+0x0001\b"),
    re.compile(r"#define\s+MF_PCI_CLASS\s+0x120000\b"),
    re.compile(r"#define\s+MF_PCI_BAR0_SIZE\s+\(64U\s*\*\s*1024U\)"),
    re.compile(r"#define\s+MF_LIVE_VENDOR_ID\s+UINT64_C\(0x4[dD]46\)"),
    re.compile(r"#define\s+MF_LIVE_DEVICE_ID\s+UINT64_C\(0x0001\)"),
)


def load_generator():
    spec = importlib.util.spec_from_file_location("metaflux_pci_guest_profile", SCRIPT)
    if spec is None or spec.loader is None:
        raise AssertionError("pci-guest profile generator cannot be loaded")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main() -> int:
    generator = load_generator()
    fixture = generator.compose(ROOT)
    assert fixture["vendor_id"] == 0x4D46
    assert fixture["device_id"] == 0x0001
    assert fixture["class_code"] == 0x120000
    assert fixture["bar0_size"] == 65536
    assert fixture["bar2_size"] == 4096
    assert fixture["bar4_size"] == 4096
    assert fixture["msix_vectors"] == 2

    with tempfile.TemporaryDirectory(prefix="metaflux-pci-guest-", dir=ROOT) as directory:
        userspace = Path(directory) / "userspace.h"
        kernel = Path(directory) / "kernel.h"
        userspace.write_text(generator.header_text(fixture, kernel=False), encoding="utf-8")
        kernel.write_text(generator.header_text(fixture, kernel=True), encoding="utf-8")
        userspace_text = userspace.read_text(encoding="utf-8")
        kernel_text = kernel.read_text(encoding="utf-8")
        assert "MF_PCI_GUEST_VENDOR_ID 0x4d46U" in userspace_text
        assert "MF_PCI_GUEST_BAR0_SIZE" in userspace_text
        assert "#include <stdint.h>" in userspace_text
        assert "#include <linux/types.h>" in kernel_text
        assert "((u64)(65536ULL))" in kernel_text

    for path in HANDWRITTEN_TARGETS:
        text = path.read_text(encoding="utf-8")
        assert (
            "MF_PCI_GUEST_" in text
            or "generated_guest_profile.h" in text
            or "metaflux/pci/generated_guest_profile.h" in text
        ), f"{path} must consume the generated profile"
        for pattern in FORBIDDEN:
            assert pattern.search(text) is None, (
                f"{path} still owns handwritten layout matching {pattern.pattern}"
            )

    print("pci-guest profile self-test: 3/3 passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
