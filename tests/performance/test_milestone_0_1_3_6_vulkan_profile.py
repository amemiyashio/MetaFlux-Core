#!/usr/bin/env python3
"""Self-test the milestone-0.1.3.6 Vulkan stage profile harness."""

from __future__ import annotations

import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = Path(__file__).resolve().with_name("run_milestone_0_1_3_6_vulkan_profile.py")


def find_benchmark() -> Path:
    env = os.environ.get("METAFLUX_VULKAN_STAGE_BENCHMARK")
    if env and Path(env).is_file():
        return Path(env)
    candidates = list(
        (ROOT.parent / ".metaflux-build" / "MetaFlux-Core").glob(
            "**/metaflux_milestone_0_1_3_6_vulkan_stage_profile"
        )
    )
    for candidate in candidates:
        if candidate.is_file():
            return candidate
    raise AssertionError("set METAFLUX_VULKAN_STAGE_BENCHMARK to the stage profile binary")


def main() -> int:
    benchmark = find_benchmark()
    with tempfile.TemporaryDirectory(prefix="metaflux-0136-profile-") as directory:
        output = Path(directory)
        result = subprocess.run(
            [
                sys.executable,
                "-B",
                str(SCRIPT),
                "--stage-benchmark",
                str(benchmark),
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
        report = json.loads((output / "vulkan-profile-report.json").read_text(encoding="utf-8"))
        assert report["status"] == "measured-host-independent"
        assert "vulkan_provider_enqueue_plan_ns" in report["metrics"]
        assert "vulkan_worker_dequeue_ledger_ns" in report["metrics"]
        assert report["comparisons"]["dual_family_differential"]["status"] == "pass"
        assert (output / "raw-samples.csv").is_file()
        assert summary["status"] == "measured-host-independent"
    print("milestone-0.1.3.6 vulkan profile self-test: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
