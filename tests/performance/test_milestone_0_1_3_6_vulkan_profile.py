#!/usr/bin/env python3
"""Self-test the milestone-0.1.3.6 Vulkan stage profile harness."""

from __future__ import annotations

import json
import importlib.util
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = Path(__file__).resolve().with_name("run_milestone_0_1_3_6_vulkan_profile.py")
SPEC = importlib.util.spec_from_file_location("vulkan_profile", SCRIPT)
assert SPEC and SPEC.loader
profile = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(profile)


class EvidenceTests(unittest.TestCase):
    def complete_metrics(self):
        return {name: {"count": 2, "p50": 12, "non_positive_count": 0, "samples_valid": True} for name in (
            "vulkan_submit_ns", "vulkan_kernel_start_ns", "vulkan_completion_ns")}

    def test_models_or_unavailable_device_do_not_prove_queue_execution(self):
        for metadata in ({}, {"mode_block": "host_pending_physical_queue"},
                         {"mode_block": "measured_physical_queue"}):
            rows = profile.queue_stage_evidence({}, metadata, 2)
            self.assertTrue(all(row["status"] == "not_measured" for row in rows.values()))

    def test_complete_queue_samples_are_consistent(self):
        rows = profile.queue_stage_evidence(self.complete_metrics(), {"mode_block": "measured_physical_queue"}, 2)
        self.assertTrue(all(row["status"] == "measured_vulkan_queue" for row in rows.values()))
        self.assertTrue(all(row["samples"] == 2 for row in rows.values()))

    def test_partial_samples_or_failed_run_do_not_promote_complete_queue(self):
        for change in ("missing", "short", "non-positive", "failed"):
            with self.subTest(change=change):
                metrics = self.complete_metrics()
                metadata = {"mode_block": "measured_physical_queue"}
                if change == "missing":
                    del metrics["vulkan_completion_ns"]
                elif change == "short":
                    metrics["vulkan_completion_ns"]["count"] = 1
                elif change == "non-positive":
                    metrics["vulkan_completion_ns"]["non_positive_count"] = 1
                else:
                    metadata["mode_block"] = "host_pending_physical_queue"
                rows = profile.queue_stage_evidence(metrics, metadata, 2)
                self.assertFalse(any(row["status"] == "measured_vulkan_queue" for row in rows.values()))

    def test_duplicate_missing_or_wrong_unit_samples_are_not_complete(self):
        for indices, unit in (([0, 1], "ns"), ([0, 0], "ns"), ([1, 2], "ns"),
                              ([0], "ns"), ([0, 1], "us")):
            with self.subTest(indices=indices, unit=unit):
                output = "".join(
                    f"METAFLUX_SAMPLE\t{metric}\t{index}\t12\t{unit}\n"
                    for metric in self.complete_metrics() for index in indices
                ) + "METAFLUX_METADATA\tmode_block\tmeasured_physical_queue\n"
                rows, metadata = profile.parse_output(output)
                metrics = profile.summarize_rows(rows, 2)
                stages = profile.queue_stage_evidence(metrics, metadata, 2)
                expected = indices == [0, 1] and unit == "ns"
                self.assertEqual(all(row["status"] == "measured_vulkan_queue" for row in stages.values()), expected)

    def test_model_samples_use_the_same_index_and_unit_validation(self):
        for metric in ("vulkan_provider_enqueue_plan_ns", "vulkan_worker_dequeue_ledger_ns"):
            rows, _ = profile.parse_output(f"METAFLUX_SAMPLE\t{metric}\t0\t12\tns\n" * 2)
            self.assertFalse(profile.summarize_rows(rows, 2)[metric]["samples_valid"])

    def test_freeze_policy_preserves_layout_without_blocking_amd_foundation(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "freeze.json"
            result = subprocess.run([sys.executable, "-B", str(ROOT / "tools/validate-vulkan-argument-freeze.py"),
                                     "--output", str(output)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            report = json.loads(output.read_text())
            self.assertEqual(report["layout"]["header_bytes"], 64)
            self.assertEqual(report["layout"]["entry_bytes"], 48)
            self.assertEqual(report["policy"]["physical_qualification_owner"], "work-item-2.0.0.3")
            self.assertFalse(report["policy"]["amd_pytorch_foundation_requires_dual_driver"])


def find_benchmark() -> Path:
    env = os.environ.get("METAFLUX_VULKAN_STAGE_BENCHMARK")
    candidate = Path(env) if env else (
        ROOT / "tmp" / "build" / "vulkan" / "plugins" / "backend" / "vulkan"
        / "runtime" / "metaflux_milestone_0_1_3_6_vulkan_stage_profile"
    )
    if candidate.is_file():
        return candidate
    raise AssertionError(
        f"stage profile binary is missing: {candidate}; build the vulkan preset "
        "or set METAFLUX_VULKAN_STAGE_BENCHMARK to an explicit build's binary"
    )


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
        assert report["status"] in {"measured-models", "measured-models-and-vulkan-queue"}
        assert "vulkan_provider_enqueue_plan_ns" in report["metrics"]
        assert "vulkan_worker_dequeue_ledger_ns" in report["metrics"]
        assert report["comparisons"]["dual_family_model_differential"]["status"] == "pass"
        assert report["comparisons"]["physical_dual_driver"]["status"] == "deferred"
        assert report["qualification"]["framework_execution_verified"] is False
        assert report["qualification"]["physical_device_identity_verified"] is False
        assert (output / "raw-samples.csv").is_file()
        assert summary["status"] == report["status"]
    print("milestone-0.1.3.6 vulkan profile self-test: ok")
    return 0


if __name__ == "__main__":
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(EvidenceTests)
    if not unittest.TextTestRunner().run(suite).wasSuccessful():
        raise SystemExit(1)
    raise SystemExit(0 if "--unit-only" in sys.argv else main())
