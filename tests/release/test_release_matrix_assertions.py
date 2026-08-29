#!/usr/bin/env python3
"""Regression tests for release-matrix negative shell assertions."""

from __future__ import annotations

import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile


RELEASE_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(RELEASE_DIR))

import run_provider_package_matrix as provider_matrix  # noqa: E402
import run_release_package_matrix as release_matrix  # noqa: E402

TEST_SHELL = os.environ.get("METAFLUX_TEST_SHELL") or shutil.which("sh")


def run_probe(helper: str, pattern: str, fixture: Path, sentinel: Path) -> subprocess.CompletedProcess[str]:
    if TEST_SHELL is None:
        raise RuntimeError("sh is required to test the release-matrix shell assertions")
    script = provider_matrix.NEGATIVE_ASSERTION_HELPERS + f"""
set -eu
probe() {{
  {helper} "$1" "$2"
  printf reached > "$3"
}}
probe "$@"
"""
    return subprocess.run(
        [TEST_SHELL, "-c", script, "release-assertion-probe", pattern, str(fixture), str(sentinel)],
        check=False,
        capture_output=True,
        text=True,
    )


def run_statistics_probe(fixture: Path) -> subprocess.CompletedProcess[str]:
    if TEST_SHELL is None:
        raise RuntimeError("sh is required to test shutdown-statistics assertions")
    script = release_matrix.SHUTDOWN_STATISTICS_HELPERS + """
set -eu
verify_shutdown_statistics "$1" interpreter 0 0 0 1
"""
    return subprocess.run(
        [TEST_SHELL, "-c", script, "shutdown-statistics-probe", str(fixture)],
        check=False,
        capture_output=True,
        text=True,
    )


def main() -> int:
    mutable_images = ["registry.example.invalid/ubuntu:20.04"]
    try:
        provider_matrix.require_digest_pinned_images(mutable_images)
    except ValueError:
        pass
    else:
        raise AssertionError("tag-only image reference passed the qualification gate")
    provider_matrix.require_digest_pinned_images(
        ["registry.example.invalid/ubuntu@sha256:" + "0" * 64]
    )

    image_evidence = provider_matrix.parse_image_runtime_evidence(
        {
            "returncode": 0,
            "stdout": [
                "METAFLUX_IMAGE_EVIDENCE kernel_sysname Linux",
                "METAFLUX_IMAGE_EVIDENCE kernel_release 6.12.0",
                "METAFLUX_IMAGE_EVIDENCE kernel_version #1 fixture kernel",
                "METAFLUX_IMAGE_EVIDENCE kernel_machine x86_64",
                "METAFLUX_IMAGE_EVIDENCE package_manager dpkg",
                "METAFLUX_IMAGE_EVIDENCE package_count 42",
                "METAFLUX_IMAGE_EVIDENCE package_set_sha256 " + "1" * 64,
                "METAFLUX_REPOSITORY_FILE " + "2" * 64 + " /etc/apt/sources.list",
            ],
            "stderr": [],
        }
    )
    if image_evidence["base_package_state"]["package_count"] != 42:
        raise AssertionError("image package-count evidence was not parsed")
    if image_evidence["qualification_updates"] != []:
        raise AssertionError("offline image probe reported qualification updates")

    invalid_evidence = {
        "returncode": 0,
        "stdout": [
            "METAFLUX_IMAGE_EVIDENCE kernel_sysname Linux",
            "METAFLUX_IMAGE_EVIDENCE kernel_release 6.12.0",
        ],
        "stderr": [],
    }
    try:
        provider_matrix.parse_image_runtime_evidence(invalid_evidence)
    except RuntimeError:
        pass
    else:
        raise AssertionError("incomplete image runtime evidence passed the parser gate")

    payloads = (
        provider_matrix.DEB_SCRIPT,
        provider_matrix.RPM_SCRIPT,
        provider_matrix.TAR_SCRIPT,
        release_matrix.DEB_SCRIPT,
        release_matrix.RPM_SCRIPT,
        release_matrix.TAR_SCRIPT,
    )
    for payload in payloads:
        if re.search(r"(?m)^\s*!\s+grep\b", payload):
            raise AssertionError("release payload contains a bare '! grep' assertion")

    with tempfile.TemporaryDirectory(prefix="metaflux-release-assertions-") as temporary:
        root = Path(temporary)
        fixture = root / "fixture.log"
        fixture.write_text("allowed\nFORBIDDEN value\n", encoding="ascii")
        probes = (
            ("assert_no_fixed_match", "FORBIDDEN"),
            ("assert_no_extended_match", "FORBID.* value"),
            ("assert_no_recursive_match", "FORBIDDEN"),
        )
        for helper, pattern in probes:
            sentinel = root / f"{helper}.reached"
            rejected = run_probe(helper, pattern, fixture, sentinel)
            if rejected.returncode == 0 or sentinel.exists():
                raise AssertionError(f"{helper} accepted an injected forbidden match")

            accepted = run_probe(helper, "ABSENT", fixture, sentinel)
            if accepted.returncode != 0 or not sentinel.exists():
                raise AssertionError(f"{helper} rejected a clean fixture: {accepted.stderr}")
            sentinel.unlink()

        missing = run_probe(
            "assert_no_fixed_match", "FORBIDDEN", root / "missing.log", root / "missing.reached"
        )
        if missing.returncode == 0:
            raise AssertionError("negative assertion hid an input read error")

        valid_statistics = (
            "metafluxd: cpu-execution mode=interpreter compiler-requests=0 "
            "cache-hits=0 cache-misses=0 loaded-modules=1 "
            "host-address-space-registrations=1 "
            "direct-host-source-operations=3 direct-host-source-bytes=768 "
            "direct-host-destination-operations=2 direct-host-destination-bytes=512 "
            "staged-host-source-operations=0 staged-host-source-bytes=0 "
            "staged-host-destination-operations=0 staged-host-destination-bytes=0"
        )
        fixture.write_text(valid_statistics + "\n", encoding="ascii")
        accepted_statistics = run_statistics_probe(fixture)
        if (
            accepted_statistics.returncode != 0
            or accepted_statistics.stdout.strip() != valid_statistics
        ):
            raise AssertionError(
                "valid direct-only shutdown statistics were rejected: "
                + accepted_statistics.stderr
            )

        rejected_statistics = {
            "zero direct source operations": valid_statistics.replace(
                "direct-host-source-operations=3",
                "direct-host-source-operations=0",
            ),
            "zero direct destination bytes": valid_statistics.replace(
                "direct-host-destination-bytes=512",
                "direct-host-destination-bytes=0",
            ),
            "staged source operation": valid_statistics.replace(
                "staged-host-source-operations=0",
                "staged-host-source-operations=1",
            ),
            "staged destination bytes": valid_statistics.replace(
                "staged-host-destination-bytes=0",
                "staged-host-destination-bytes=4",
            ),
            "extra field": valid_statistics + " unexpected=1",
            "duplicate metrics line": valid_statistics + "\n" + valid_statistics,
        }
        for label, statistics in rejected_statistics.items():
            fixture.write_text(statistics + "\n", encoding="ascii")
            rejected = run_statistics_probe(fixture)
            if rejected.returncode == 0:
                raise AssertionError(f"{label} passed the shutdown-statistics gate")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
