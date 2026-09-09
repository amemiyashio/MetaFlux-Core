#!/usr/bin/env python3
"""Self-tests for pinned stock PyTorch concat evidence parsing."""

from __future__ import annotations

import importlib.util
from pathlib import Path
import subprocess
import unittest
from unittest import mock


RUNNER = Path(__file__).with_name("run_pytorch_cuda_cat.py")
SPEC = importlib.util.spec_from_file_location("pytorch_cuda_cat", RUNNER)
assert SPEC is not None and SPEC.loader is not None
cat = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(cat)


class CatEvidenceTests(unittest.TestCase):
    def test_provider_evidence_extracts_neutral_request(self) -> None:
        evidence = cat.parse_provider_evidence(
            "MF_LAUNCH f=0x1 grid=1x128\n"
            "MF_PYTORCH_BASELINE_REQUEST profile=baseline operation=concat-u32 "
            "version=1 kernel-ir=2 lifetime=module-load\n"
        )
        self.assertEqual(evidence["requests"], [cat.REQUEST])
        self.assertEqual(evidence["launches"], 1)
        self.assertEqual(evidence["local_execution"], 0)

    def test_provider_evidence_exposes_forbidden_local_execution(self) -> None:
        evidence = cat.parse_provider_evidence("MF_LAUNCH f=0x1 grid=1x128\nMF_SEMANTIC concat\n")
        self.assertEqual(evidence["local_execution"], 1)

    def test_daemon_statistics_require_one_record(self) -> None:
        line = (
            "metafluxd: cpu-execution mode=interpreter compiler-requests=0 cache-hits=0 "
            "cache-misses=0 loaded-modules=1 host-address-space-registrations=1 "
            "direct-host-source-operations=5 direct-host-source-bytes=36 "
            "direct-host-destination-operations=2 direct-host-destination-bytes=68\n"
        )
        statistics = cat.parse_daemon_statistics(line)
        self.assertEqual(statistics["loaded_modules"], 1)
        self.assertEqual(statistics["source_operations"], 5)
        self.assertEqual(statistics["destination_operations"], 2)
        with self.assertRaisesRegex(RuntimeError, "exactly one"):
            cat.parse_daemon_statistics("")

    def test_source_provenance_binds_revision_and_tree_state(self) -> None:
        revision = "f92304e55d9849de5c3694a4755dbcf22418b3e5"
        with mock.patch.object(
            cat.subprocess,
            "run",
            side_effect=(
                subprocess.CompletedProcess(["git"], 0, stdout=f"{revision}\n", stderr=""),
                subprocess.CompletedProcess(["git"], 0, stdout=" M provider.c\n", stderr=""),
            ),
        ):
            self.assertEqual(
                cat.source_provenance(),
                {"revision": revision, "tree_state": "dirty"},
            )


if __name__ == "__main__":
    unittest.main()
