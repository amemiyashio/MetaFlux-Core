#!/usr/bin/env python3
"""Probe the running kernel for debug/sanitizer CONFIGs.

Reads /proc/config.gz (preferred) or /boot/config-$(uname -r) and reports the
status of each required kernel debug symbol. Used by the kernel qualification
gate to skip tests when the host does not run a debug kernel.

Exit codes
----------
 0  normal reporter mode (always succeeds, prints JSON)
 77 --require-qualification and at least one required config is not 'y'
 1  unexpected error (missing config file, malformed data, etc.)
"""

from __future__ import annotations

import argparse
import gzip
import json
import os
import subprocess
import sys
from pathlib import Path

REQUIRED_CONFIGS = (
    "CONFIG_KASAN",
    "CONFIG_KCSAN",
    "CONFIG_PROVE_LOCKING",
    "CONFIG_DEBUG_KMEMLEAK",
    "CONFIG_KUNIT",
)

# Where to look for the kernel config, in priority order.
CONFIG_SOURCES = (
    "/proc/config.gz",
)


def _boot_config_path() -> Path | None:
    """Return the uncompressed boot config path for the running kernel."""
    try:
        release = subprocess.run(
            ["uname", "-r"],
            check=True,
            capture_output=True,
            text=True,
        ).stdout.strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        return None
    path = Path(f"/boot/config-{release}")
    try:
        return path if path.exists() else None
    except OSError:
        return None


def _read_config_bytes(source: str) -> bytes:
    """Read config bytes, decompressing gzip when needed."""
    path = Path(source)
    if source.endswith(".gz") or path.suffix == ".gz":
        with path.open("rb") as fh:
            return gzip.decompress(fh.read())
    with path.open("rb") as fh:
        return fh.read()


def probe() -> dict[str, str]:
    """Return a mapping of config name -> 'y'|'m'|'n'|'absent'."""
    # Allow test fixtures to override the config source.
    override = os.environ.get("METAFLUX_KERNEL_CONFIG")
    if override:
        sources: list[str] = [override]
    else:
        sources = list(CONFIG_SOURCES)
        boot = _boot_config_path()
        if boot is not None:
            sources.append(str(boot))

    text: str | None = None
    for source in sources:
        try:
            raw = _read_config_bytes(source)
            text = raw.decode("utf-8", errors="replace")
            break
        except (OSError, gzip.BadGzipFile):
            continue

    if text is None:
        return {cfg: "absent" for cfg in REQUIRED_CONFIGS}

    values: dict[str, str] = {}
    for line in text.splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        if "=" not in line:
            continue
        key, _, value = line.partition("=")
        key = key.strip()
        value = value.strip().strip('"')
        if key in REQUIRED_CONFIGS:
            if value in ("y", "m"):
                values[key] = "y"
            elif value == "n":
                values[key] = "n"
            else:
                values[key] = "absent"

    # Fill missing entries as absent.
    for cfg in REQUIRED_CONFIGS:
        values.setdefault(cfg, "absent")

    return values


def main() -> int:
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "--require-qualification",
        action="store_true",
        help="Exit 77 (CTest SKIP_RETURN_CODE) if any required config is not 'y'.",
    )
    parser.add_argument(
        "--output",
        type=str,
        default=None,
        help="Write JSON report to this path (in addition to stdout).",
    )
    args = parser.parse_args()

    values = probe()
    output = {
        "kernel_config": values,
        "qualified": all(v == "y" for v in values.values()),
    }
    print(json.dumps(output, indent=2, sort_keys=True))

    output_path = getattr(args, "output", None)
    if output_path:
        Path(output_path).write_text(
            json.dumps(output, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )

    if args.require_qualification and not output["qualified"]:
        missing = [k for k, v in values.items() if v != "y"]
        print(
            f"kernel not qualified: {', '.join(missing)}",
            file=sys.stderr,
        )
        return 77

    return 0


if __name__ == "__main__":
    sys.exit(main())
