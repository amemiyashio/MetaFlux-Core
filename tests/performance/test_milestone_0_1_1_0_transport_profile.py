#!/usr/bin/env python3
"""Self-test the milestone-0.1.1.5 transport profile harness."""

from __future__ import annotations

import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = Path(__file__).resolve().with_name("run_milestone_0_1_1_0_transport_profile.py")


def find_ring_benchmark() -> Path:
    env = os.environ.get("METAFLUX_RING_BENCHMARK")
    if env:
        path = Path(env)
        if path.is_file():
            return path
    candidates = [
        ROOT.parent
        / ".metaflux-build"
        / "MetaFlux-Core"
        / "dev"
        / "tests"
        / "metaflux_milestone_0_1_0_0_ring_benchmark",
        ROOT
        / "build"
        / "tests"
        / "metaflux_milestone_0_1_0_0_ring_benchmark",
    ]
    for candidate in candidates:
        if candidate.is_file():
            return candidate
    raise AssertionError("ring benchmark binary not found; set METAFLUX_RING_BENCHMARK")


def main() -> int:
    ring = find_ring_benchmark()
    with tempfile.TemporaryDirectory(prefix="metaflux-0115-profile-") as directory:
        output = Path(directory)
        result = subprocess.run(
            [
                sys.executable,
                "-B",
                str(SCRIPT),
                "--ring-benchmark",
                str(ring),
                "--output-dir",
                str(output),
                "--warmup",
                "8",
                "--samples",
                "32",
            ],
            check=False,
            capture_output=True,
            text=True,
            cwd=str(ROOT),
        )
        if result.returncode != 0:
            raise AssertionError(result.stderr or result.stdout)
        summary = json.loads(result.stdout)
        assert summary["audit"] == "pass"
        report = json.loads((output / "transport-profile-report.json").read_text(encoding="utf-8"))
        assert report["modes"]["poll"]["status"] == "measured"
        assert report["modes"]["numa"]["status"] == "measured"
        assert report["warm_path_audit"]["status"] == "pass"
        assert report["contract"]["id"] == "transport.measurement.v0"
        raw = (output / "raw-samples.csv").read_text(encoding="utf-8").splitlines()
        assert raw[0].startswith("metric,")
        assert len(raw) > 1
        archive = json.loads((output / "measurement-archive.json").read_text(encoding="utf-8"))
        assert archive["samples"]["status"] == "collected-poll-memfd-ring"
        assert archive["fingerprints"]["cpu_model"]
    print("milestone-0.1.1.5 transport profile self-test: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
