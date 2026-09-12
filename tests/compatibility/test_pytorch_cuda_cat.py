#!/usr/bin/env python3
"""Self-tests for pinned stock PyTorch concat evidence parsing."""

from __future__ import annotations

import importlib.util
from pathlib import Path
import subprocess
import tempfile
from types import SimpleNamespace
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


class CatAffinityTests(unittest.TestCase):
    def setUp(self) -> None:
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        root = Path(temporary.name)
        (root / "daemon").touch()
        (root / "libcuda.so.1").touch()
        self.arguments = SimpleNamespace(daemon=root / "daemon", provider_dir=root,
                                        client_manifest=cat.MANIFEST)
        self.original = {4, 9, 12}
        self.affinity = set(self.original)
        for name, callback in (("sched_getaffinity", lambda _: set(self.affinity)),
                               ("sched_setaffinity", self.set_affinity)):
            patcher = mock.patch.object(cat.os, name, side_effect=callback)
            setattr(self, name, patcher.start())
            self.addCleanup(patcher.stop)
        patcher = mock.patch.object(cat, "source_provenance", return_value={"revision": "fixture"})
        self.provenance = patcher.start()
        self.addCleanup(patcher.stop)
        patcher = mock.patch.object(cat, "run_case", side_effect=self.observe_case)
        self.run_case = patcher.start()
        self.addCleanup(patcher.stop)

    def set_affinity(self, pid, cpus):
        self.assertEqual(pid, 0)
        self.affinity = set(cpus)

    def observe_case(self, case, *_):
        self.assertEqual(self.affinity, {4})
        return {"case": case}

    def test_every_case_runs_pinned_and_success_restores_affinity(self):
        result = cat.runner(self.arguments)
        self.assertEqual(list(result["cases"]), ["positive", "unsupported-dimension",
                         "unsupported-layout", "unsupported-dtype", "source-capacity",
                         "unsupported-length", "unequal-length", "unsupported-right-length",
                         "unsupported-arity"])
        self.assertEqual(self.run_case.call_count, 9)
        self.assertEqual(result["affinity"], {"original_cpu_count": 3, "selected_cpu": 4})
        self.assertEqual(self.affinity, self.original)

    def test_case_failure_is_preserved_without_retry_and_restores_affinity(self):
        failure = RuntimeError("case failed")

        def fail_third(case, *args):
            result = self.observe_case(case, *args)
            if case == "unsupported-layout":
                raise failure
            return result

        self.run_case.side_effect = fail_third
        with self.assertRaises(RuntimeError) as caught:
            cat.runner(self.arguments)
        self.assertIs(caught.exception, failure)
        self.assertEqual(self.run_case.call_count, 3)
        self.assertEqual(self.affinity, self.original)

    def test_provenance_failure_also_restores_affinity(self):
        self.provenance.side_effect = RuntimeError("provenance failed")
        with self.assertRaisesRegex(RuntimeError, "provenance failed"):
            cat.runner(self.arguments)
        self.run_case.assert_not_called()
        self.assertEqual(self.affinity, self.original)

    def test_empty_affinity_stops_before_starting_cases(self):
        self.affinity = set()
        with self.assertRaisesRegex(RuntimeError, "no effective CPU affinity"):
            cat.runner(self.arguments)
        self.run_case.assert_not_called()
        self.provenance.assert_not_called()
        self.sched_setaffinity.assert_not_called()

    def test_pin_failure_stops_before_starting_cases_without_retry(self):
        self.sched_setaffinity.side_effect = OSError("affinity unavailable")
        with self.assertRaisesRegex(OSError, "affinity unavailable"):
            cat.runner(self.arguments)
        self.run_case.assert_not_called()
        self.provenance.assert_not_called()
        self.sched_setaffinity.assert_called_once_with(0, {4})
        self.assertEqual(self.affinity, self.original)


if __name__ == "__main__":
    unittest.main()
