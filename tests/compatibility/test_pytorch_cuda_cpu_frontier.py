#!/usr/bin/env python3
"""Self-tests for the stock-PyTorch CUDA CPU frontier gate."""

from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest import mock


RUNNER = Path(__file__).with_name("run_pytorch_cuda_cpu_frontier.py")
SPEC = importlib.util.spec_from_file_location("pytorch_cuda_cpu_frontier", RUNNER)
assert SPEC is not None and SPEC.loader is not None
frontier = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(frontier)


class CpuFrontierEvidenceTests(unittest.TestCase):
    def test_repository_corpus_matches_pinned_profile(self) -> None:
        corpus = frontier.load_corpus(frontier.CORPUS, frontier.CLIENT_MANIFEST)
        self.assertEqual(len(corpus["cases"]), 38)
        self.assertEqual(len(corpus["gaps"]), 1)
        self.assertTrue(corpus["scope"]["library_boundary_closed"])
        self.assertFalse(corpus["scope"]["exit_gate_complete"])
        self.assertEqual(corpus["cases"][-1]["id"], "sigmoid-f32")
        self.assertEqual(corpus["cases"][-2]["id"], "softmax-f32-nonlast")
        self.assertEqual(corpus["cases"][-3]["id"], "softmax-f32")
        self.assertEqual(corpus["cases"][-4]["id"], "linear-bias-f32")
        self.assertEqual(
            corpus["cases"][-4]["expected_library_calls"], ["lt-matmul-bias-f32"]
        )
        self.assertEqual(corpus["cases"][-5]["id"], "addmm-f32")
        self.assertEqual(corpus["cases"][-5]["expected_library_calls"], ["sgemm-f32"])
        self.assertEqual(corpus["cases"][-6]["id"], "linear-no-bias-f32")
        self.assertEqual(corpus["cases"][-7]["id"], "matmul-rect-f32")
        self.assertEqual(corpus["cases"][-8]["id"], "matmul-f32")
        self.assertEqual(corpus["cases"][-9]["id"], "clamp-min-negative-f32")
        self.assertEqual(corpus["gaps"][0]["id"], "sigmoid-f64")
        self.assertEqual(corpus["gaps"][0]["status"], "frontier-gap")
        self.assertEqual(corpus["gaps"][0]["expected_error"], "operation not supported")

    def test_provider_evidence_preserves_request_order(self) -> None:
        evidence = frontier.parse_provider_evidence(
            "MF_LAUNCH f=0x1 grid=1x128\n"
            "MF_PYTORCH_BASELINE_MODULE artifact=0x1\n"
            "MF_PYTORCH_BASELINE_REQUEST profile=baseline operation=cast-copy-i64 "
            "version=1 kernel-ir=2 lifetime=module-load\n"
            "MF_PYTORCH_BASELINE_MODULE artifact=0x2\n"
            "MF_PYTORCH_BASELINE_REQUEST profile=baseline operation=reduce-sum-i64 "
            "version=1 kernel-ir=2 lifetime=module-load\n"
            "MF_CUBLAS_REQUEST operation=sgemm-f32 rows=2 columns=2 inner=2\n"
            "MF_CUBLAS_REQUEST operation=lt-matmul-bias-f32 rows=2 columns=2 inner=2\n"
        )
        self.assertEqual(
            evidence["requests"],
            [
                "baseline:cast-copy-i64:1:2:module-load",
                "baseline:reduce-sum-i64:1:2:module-load",
            ],
        )
        self.assertEqual(evidence["module_loads"], 2)
        self.assertEqual(
            evidence["library_calls"], ["sgemm-f32", "lt-matmul-bias-f32"]
        )
        self.assertEqual(evidence["local_execution"], 0)

    def test_provider_evidence_exposes_forbidden_local_execution(self) -> None:
        evidence = frontier.parse_provider_evidence("MF_SEMANTIC relu\n")
        self.assertEqual(evidence["local_execution"], 1)

    def test_daemon_statistics_require_one_record(self) -> None:
        line = (
            "metafluxd: cpu-execution mode=interpreter compiler-requests=0 cache-hits=0 "
            "cache-misses=0 loaded-modules=2 host-address-space-registrations=1 "
            "direct-host-source-operations=2 direct-host-source-bytes=24 "
            "direct-host-destination-operations=1 direct-host-destination-bytes=8\n"
        )
        statistics = frontier.parse_daemon_statistics(line)
        self.assertEqual(statistics["loaded_modules"], 2)
        self.assertEqual(statistics["destination_operations"], 1)
        with self.assertRaisesRegex(RuntimeError, "exactly one"):
            frontier.parse_daemon_statistics("")

    def test_client_cpu_pin_selects_lowest_effective_cpu(self) -> None:
        with (
            mock.patch.object(frontier.os, "sched_getaffinity", return_value={11, 3, 7}),
            mock.patch.object(frontier.os, "sched_setaffinity") as set_affinity,
        ):
            original, evidence = frontier.pin_client_cpu()
        self.assertEqual(original, {11, 3, 7})
        self.assertEqual(evidence, {"original_cpu_count": 3, "selected_cpu": 3})
        set_affinity.assert_called_once_with(0, {3})

    def test_corpus_rejects_duplicate_case_identifier(self) -> None:
        client = {"python": {"version": "3.13.15"}, "profiles": {"baseline": {
            "torch_version": "2.11.0+cu126", "cuda_version": "12.6"
        }}}
        corpus = {
            "schema_version": 1,
            "status": "frontier-not-frozen",
            "client": {"python": "3.13.15", "torch": "2.11.0+cu126", "cuda": "12.6"},
            "cases": [
                {"id": "same", "oracle": {}, "expected_requests": ["request"]},
                {"id": "same", "oracle": {}, "expected_requests": ["request"]},
            ],
            "gaps": [{"id": "gap", "status": "frontier-gap", "expected_error": "error"}],
        }
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            client_path = root / "client.json"
            corpus_path = root / "corpus.json"
            client_path.write_text(json.dumps(client), encoding="utf-8")
            corpus_path.write_text(json.dumps(corpus), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "unique"):
                frontier.load_corpus(corpus_path, client_path)

    def test_source_provenance_binds_revision_and_tree_state(self) -> None:
        revision = "f92304e55d9849de5c3694a4755dbcf22418b3e5"
        with mock.patch.object(
            frontier.subprocess,
            "run",
            side_effect=(
                subprocess.CompletedProcess(["git"], 0, stdout=f"{revision}\n", stderr=""),
                subprocess.CompletedProcess(["git"], 0, stdout=" M provider.c\n", stderr=""),
            ),
        ):
            self.assertEqual(
                frontier.source_provenance(), {"revision": revision, "tree_state": "dirty"}
            )


if __name__ == "__main__":
    unittest.main()
