#!/usr/bin/env python3
"""Real CTest selection/reporting and isolated verification lifecycle scenarios."""
from __future__ import annotations

import copy
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[4] / "agent/lib"))
sys.path.insert(0, str(Path(__file__).resolve().parents[4] / "agent/skills/main/scripts"))
import main as controller
import rule_loading
import test_workflow_state as fixture
import verification
import workflow_state as ws


class VerificationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="metaflux-verification-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / "repo"
        fixture.fixture(self.root)
        with (self.root / ".gitignore").open("a") as stream:
            stream.write("/tmp/\n")
        self.source = """cmake_minimum_required(VERSION 3.25)
project(VerificationFixture LANGUAGES NONE)
enable_testing()
add_test(NAME alpha COMMAND "${CMAKE_COMMAND}" -E touch "${CMAKE_SOURCE_DIR}/tmp/alpha-ran")
set_tests_properties(alpha PROPERTIES LABELS "focused")
add_test(NAME beta COMMAND "${CMAKE_COMMAND}" -E true)
add_test(NAME skipped COMMAND """ + sys.executable + """ -c "raise SystemExit(77)")
set_tests_properties(skipped PROPERTIES SKIP_RETURN_CODE 77)
add_test(NAME producer COMMAND "${CMAKE_COMMAND}" -E touch "${CMAKE_SOURCE_DIR}/tmp/produced")
add_test(NAME consumer COMMAND """ + sys.executable + """ -c "from pathlib import Path; assert Path('""" + str(self.root / "tmp/produced") + """').exists()")
set_tests_properties(consumer PROPERTIES DEPENDS producer)
"""
        (self.root / "CMakeLists.txt").write_text(self.source)
        self.build = self.root / "tmp/build/test"
        subprocess.run(["cmake", "-S", str(self.root), "-B", str(self.build)], check=True, capture_output=True)
        ws.git(self.root, "add", ".")
        ws.git(self.root, "commit", "-qm", "test inventory")
        self.base = ws.oid(self.root)

    def check(self, name="suite", *selection):
        return {"id": name, "argv": ["ctest", "--test-dir", str(self.build), "--output-on-failure", *selection]}

    def evaluate(self, checks):
        rule_loading.load(self.root, "maintenance", self.base, action="review", output=io.StringIO())
        return ws.evaluate(self.root, "maintenance", self.base, checks,
                           ws.review(self.root, "Review actual fixture coverage", checks))

    def test_overlap_rejected_before_any_command_runs(self):
        plans = [
            [self.check("full"), self.check("focused", "-R", "^alpha$")],
            [self.check("label", "-L", "focused"), self.check("regex", "-R", "alpha")],
        ]
        for plan in plans:
            sentinel = {"id": "sentinel", "argv": ["cmake", "-E", "touch", str(self.root / "tmp/sentinel")]}
            with self.assertRaisesRegex(ws.WorkflowError, "Repeated CTest coverage"):
                self.evaluate([sentinel, *plan])
            self.assertFalse((self.root / "tmp/sentinel").exists())
            self.assertFalse((self.root / "tmp/alpha-ran").exists())
        with self.assertRaisesRegex(ws.WorkflowError, "Duplicate check command"):
            verification.preflight(self.root, [self.check("one"), self.check("two")])

    def test_empty_and_missing_prerequisite_rejected(self):
        for selection, error in ((("-R", "nonexistent"), "no tests"), (("-R", "^consumer$"), "prerequisite")):
            with self.assertRaisesRegex(ws.WorkflowError, error):
                self.evaluate([self.check("required", *selection)])
        with self.assertRaises(ws.WorkflowError):
            self.evaluate([self.check("hidden", "--rerun-failed")])
        for option in (("--repeat", "until-pass:2"), ("--repeat-until-fail", "2")):
            with self.assertRaises(ws.WorkflowError):
                self.evaluate([self.check("retry", *option)])
        with self.assertRaisesRegex(ws.WorkflowError, "Declare CTest directly"):
            verification.preflight(self.root, [{"id": "wrapped", "argv": ["nix", "develop", ".", "--command", "ctest"]}])

    def test_equivalent_selector_spellings_overlap(self):
        first = self.check("first", "-R", "^alpha$", "-j1")
        second = {"id": "second", "argv": ["ctest", "--progress", "-j", "4", "--tests-regex=^alpha$", "--test-dir=tmp/build/test"]}
        with self.assertRaisesRegex(ws.WorkflowError, "Repeated CTest coverage"):
            verification.preflight(self.root, [first, second])

    def test_preset_and_directory_share_coverage_but_environment_does_not(self):
        document = {"version": 6, "configurePresets": [
            {"name": "base", "hidden": True, "generator": "Ninja", "binaryDir": "${sourceDir}/tmp/build/${presetName}"},
            {"name": "test", "inherits": "base"}], "testPresets": [
            {"name": "test", "configurePreset": "test", "output": {"outputOnFailure": True}},
            {"name": "device", "inherits": "test", "environment": {"TEST_DEVICE": "fixture"}}]}
        (self.root / "CMakePresets.json").write_text(json.dumps(document))
        preset = {"id": "preset", "argv": ["ctest", "--preset", "test", "-R", "alpha"]}
        with self.assertRaisesRegex(ws.WorkflowError, "Repeated CTest coverage"):
            verification.preflight(self.root, [preset, self.check("directory", "-R", "alpha")])
        verification.require_covered_removal(self.root, self.check("directory", "-R", "alpha"), [preset])
        device = {"id": "device", "argv": ["ctest", "--preset", "device", "-R", "alpha"]}
        self.assertEqual(len(verification.preflight(self.root, [preset, device])), 2)

    def test_repair_removes_only_proven_redundant_ctest_coverage(self):
        narrow = self.check("focused", "-R", "^alpha$")
        broad = self.check("combined", "-R", "alpha|beta")
        task = fixture.request(self.root)
        task["checks"] = [narrow, broad]
        controller.begin(self.root, task)
        fixture.prepare(self.root)
        controller.step(self.root, "repair", {"checks": [broad]})
        current = ws.read_json(ws.local_path(self.root, "state.json"))
        self.assertEqual(current["request"]["checks"], [broad])
        weakened = {**broad, "allowed_ctest_skips": ["alpha"]}
        with self.assertRaisesRegex(ws.WorkflowError, "additional CTest skips"):
            controller.step(self.root, "repair", {"checks": [weakened]})
        with self.assertRaisesRegex(ws.WorkflowError, "remove required CTest coverage"):
            controller.step(self.root, "repair", {"checks": [narrow]})

    def test_distinct_configuration_and_disjoint_selections_retained(self):
        checks = [self.check("alpha", "-R", "^alpha$"), self.check("beta", "-R", "^beta$")]
        result = verification.preflight(self.root, checks)
        self.assertEqual(set(result), {"alpha", "beta"})
        result = verification.preflight(self.root, [
            self.check("debug", "-C", "Debug", "-R", "alpha"),
            self.check("release", "-C", "Release", "-R", "alpha")])
        self.assertEqual(len(result), 2)
        evidence = self.evaluate(checks)
        ws.validate_receipt(self.root, evidence, kind="maintenance")
        self.assertEqual(evidence["results"][0]["ctest"]["passed"], ["alpha"])

    def test_repeated_attempts_keep_both_receipts_valid(self):
        checks = [self.check("alpha", "-R", "^alpha$")]
        first = self.evaluate(checks)
        contents = Path(first["results"][0]["log"]).read_bytes()
        second = self.evaluate(checks)
        self.assertNotEqual(first["attempt"], second["attempt"])
        self.assertEqual(contents, Path(first["results"][0]["log"]).read_bytes())
        for receipt in (first, second):
            ws.validate_receipt(self.root, receipt, kind="maintenance")
        changed = copy.deepcopy(second)
        changed["attempt"] = first["attempt"]
        changed["digest"] = ws.digest({key: value for key, value in changed.items() if key != "digest"})
        with self.assertRaisesRegex(ws.WorkflowError, "another attempt"):
            ws.validate_receipt(self.root, changed, kind="maintenance")

    def test_receipt_validates_from_another_checkout(self):
        checks = [{"id": "relative", "argv": ["ctest", "--test-dir", "tmp/build/test", "-R", "^alpha$"]}]
        receipt = self.evaluate(checks)
        other = Path(self.temp.name) / "integrator"
        ws.git(self.root, "clone", "--no-hardlinks", str(self.root), str(other))
        ws.validate_receipt(other, receipt, kind="maintenance")
        self.assertEqual(receipt["results"][0]["ctest"]["context"], ["--test-dir=" + str(self.build)])

    def test_skips_and_actual_test_set_are_checked(self):
        plan = [self.check("skip", "-R", "^skipped$")]
        with self.assertRaisesRegex(ws.WorkflowError, "Undeclared CTest skips"):
            self.evaluate(plan)
        plan[0]["allowed_ctest_skips"] = ["skipped"]
        receipt = self.evaluate(plan)
        self.assertEqual(receipt["results"][0]["ctest"]["skipped"], ["skipped"])
        ws.validate_receipt(self.root, receipt, kind="maintenance")
        report = Path(receipt["results"][0]["ctest"]["report"])
        report.write_text("<testsuite/>")
        with self.assertRaisesRegex(ws.WorkflowError, "Executed CTest set"):
            ws.validate_receipt(self.root, receipt, kind="maintenance")

    def test_inventory_is_refreshed_after_configure(self):
        with (self.root / "CMakeLists.txt").open("a") as stream:
            stream.write('\nif(ADDITIONAL_TEST)\nadd_test(NAME extra COMMAND "${CMAKE_COMMAND}" -E true)\nendif()\n')
        ws.git(self.root, "add", "CMakeLists.txt")
        ws.git(self.root, "commit", "-qm", "conditional inventory")
        self.base = ws.oid(self.root)
        plan = [{"id": "configure", "argv": ["cmake", "-S", str(self.root), "-B", str(self.build), "-DADDITIONAL_TEST=ON"]},
                self.check("extra", "-R", "^extra$")]
        receipt = self.evaluate(plan)
        self.assertEqual(receipt["results"][1]["ctest"]["passed"], ["extra"])

    def test_lock_rejects_duplicate_before_start_and_inspect_is_read_only(self):
        state_path = ws.local_path(self.root, "verification.json")
        # Git may finish background auto-maintenance after fixture commits;
        # compare application files and exact Git authority, not object packing.
        before = {p: p.read_bytes() for p in self.root.rglob("*") if p.is_file() and ".git" not in p.relative_to(self.root).parts}
        git_before = (ws.oid(self.root), ws.git(self.root, "ls-files", "--stage"), (self.root / ".git/config").read_bytes())
        controller.inspect(self.root)
        self.assertEqual(before, {p: p.read_bytes() for p in self.root.rglob("*") if p.is_file() and ".git" not in p.relative_to(self.root).parts})
        self.assertEqual(git_before, (ws.oid(self.root), ws.git(self.root, "ls-files", "--stage"), (self.root / ".git/config").read_bytes()))
        with verification.execution_lock(self.root):
            with self.assertRaisesRegex(ws.WorkflowError, "already running"):
                self.evaluate([self.check("alpha", "-R", "^alpha$")])
            self.assertTrue(verification.status(self.root)["lock_busy"])
            self.assertIn("shared repository", controller.inspect(self.root)["next_operation"])
        self.assertFalse(state_path.exists())
        self.assertFalse((self.root / "tmp/alpha-ran").exists())

    def test_running_evaluation_points_to_owned_session_without_mutation(self):
        task = fixture.request(self.root)
        task["checks"] = [self.check("live-alpha", "-R", "^alpha$")]
        controller.begin(self.root, task)
        fixture.prepare(self.root)
        controller.step(self.root, "review", {"summary": "Review live-session guidance"})
        original = verification.junit_result
        observations = []

        def observe(*args):
            paths = [ws.local_path(self.root, name) for name in ("state.json", "verification.json")]
            before = [path.read_bytes() for path in paths]
            observed = controller.begin(self.root, {"kind": "read-only"})
            self.assertEqual([path.read_bytes() for path in paths], before)
            attempt = observed["evidence"]["verification"]
            self.assertTrue(attempt["lock_busy"])
            self.assertIn("existing verification tool session", observed["next_operation"])
            self.assertIn("live-alpha", observed["next_operation"])
            self.assertIn(attempt["log"], observed["next_operation"])
            observations.append(attempt["attempt"])
            return original(*args)

        with patch.object(verification, "junit_result", side_effect=observe):
            controller.step(self.root, "evaluate", {})
        self.assertEqual(len(observations), 1)
        self.assertEqual(controller.inspect(self.root)["stage"], "delivery")
        self.assertIn("guarded commit", controller.inspect(self.root)["next_operation"])
        self.assertEqual(len(list(ws.local_path(self.root, "receipts").glob("*.json"))), 1)

    def test_shared_lock_with_old_local_record_does_not_name_old_session(self):
        controller.begin(self.root, fixture.request(self.root))
        fixture.prepare(self.root)
        path = ws.local_path(self.root, "verification.json")
        current = ws.read_json(ws.local_path(self.root, "state.json"))["run"]
        for owner, outcome in (("old-operation", "running"), (current, "passed"), (current, "failed")):
            ws.atomic_json(path, {"request": owner, "status": outcome,
                                  "log": "old-log", "check": "old-check"})
            before = path.read_bytes()
            with verification.execution_lock(self.root):
                observed = controller.inspect(self.root)
                self.assertIn("shared repository", observed["next_operation"])
                self.assertNotIn("old-log", observed["next_operation"])
            self.assertEqual(path.read_bytes(), before)

    def test_failed_new_preflight_supersedes_old_integration_hint(self):
        controller.begin(self.root, fixture.request(self.root, "batch"))
        fixture.prepare(self.root)
        rule_loading.load(self.root, "integration", self.base, output=io.StringIO())
        plan = [self.check("alpha", "-R", "^alpha$")]
        evidence = ws.evaluate(self.root, "integration", self.base, plan,
                               ws.review(self.root, "Review first integration plan", plan))
        self.assertIn(evidence["digest"], controller.inspect(self.root)["next_operation"])
        marker = (self.root / "tmp/alpha-ran").stat().st_mtime_ns
        bad_plan = [*plan, self.check("duplicate", "-L", "focused")]
        with self.assertRaisesRegex(ws.WorkflowError, "Repeated CTest coverage"):
            ws.evaluate(self.root, "integration", self.base, bad_plan,
                        ws.review(self.root, "Review replacement plan", bad_plan))
        observed = controller.inspect(self.root)
        self.assertIn("preflight", observed["next_operation"])
        self.assertNotIn(evidence["digest"], observed["next_operation"])
        self.assertEqual(observed["evidence"]["verification"]["completed_checks"], 0)
        self.assertEqual((self.root / "tmp/alpha-ran").stat().st_mtime_ns, marker)
        self.assertEqual(len(list(ws.local_path(self.root, "receipts").glob("*.json"))), 1)

    def test_strict_preflight_failure_does_not_name_successful_configure(self):
        with (self.root / "CMakeLists.txt").open("a") as stream:
            stream.write('\nif(ADDITIONAL_TEST)\nadd_test(NAME extra COMMAND "${CMAKE_COMMAND}" -E true)\n'
                         'set_tests_properties(extra PROPERTIES LABELS "delayed")\nendif()\n')
        ws.git(self.root, "add", "CMakeLists.txt")
        ws.git(self.root, "commit", "-qm", "delayed inventory")
        task = fixture.request(self.root)
        task["checks"] = [
            {"id": "configure", "argv": ["cmake", "-S", str(self.root), "-B", str(self.build), "-DADDITIONAL_TEST=ON"]},
            self.check("by-name", "-R", "^extra$"),
            self.check("by-label", "-L", "delayed"),
        ]
        controller.begin(self.root, task)
        fixture.prepare(self.root)
        controller.step(self.root, "review", {"summary": "Review strict post-configuration preflight"})
        with self.assertRaisesRegex(ws.WorkflowError, "Repeated CTest coverage"):
            controller.step(self.root, "evaluate", {})
        observed = controller.inspect(self.root)
        attempt = observed["evidence"]["verification"]
        self.assertEqual(attempt["completed_checks"], 1)
        self.assertEqual(attempt["check"], "preflight")
        self.assertNotIn("log", attempt)
        self.assertIn("preflight", observed["next_operation"])
        self.assertFalse((self.root / "tmp/alpha-ran").exists())

    def test_current_interrupted_attempt_names_cause_without_retry(self):
        task = fixture.request(self.root)
        task["checks"] = [self.check("interrupted-alpha", "-R", "^alpha$")]
        controller.begin(self.root, task)
        fixture.prepare(self.root)
        controller.step(self.root, "review", {"summary": "Review interrupted-attempt guidance"})
        with patch.object(verification, "junit_result", side_effect=KeyboardInterrupt):
            with self.assertRaises(KeyboardInterrupt):
                controller.step(self.root, "evaluate", {})
        path = ws.local_path(self.root, "verification.json")
        value = ws.read_json(path)
        value["status"] = "running"
        ws.atomic_json(path, value)
        before = path.read_bytes()
        observed = controller.inspect(self.root)
        self.assertEqual(observed["evidence"]["verification"]["status"], "interrupted")
        self.assertIn("interrupted-alpha", observed["next_operation"])
        self.assertIn(value["log"], observed["next_operation"])
        self.assertIn(task["objective"], observed["next_operation"])
        self.assertEqual(path.read_bytes(), before)
        self.assertFalse(list(ws.local_path(self.root, "receipts").glob("*.json")))
        # A crash can leave evaluation while the attempt has already recorded failure.
        value["status"] = "failed"
        value["error"] = "interrupted after failure recording"
        ws.atomic_json(path, value)
        operation_path = ws.local_path(self.root, "state.json")
        operation = ws.read_json(operation_path)
        operation["stage"] = "evaluation"
        ws.atomic_json(operation_path, operation)
        self.assertIn(value["error"], controller.inspect(self.root)["next_operation"])

    def test_interruption_has_no_success_receipt_and_no_automatic_retry(self):
        plan = [self.check("alpha", "-R", "^alpha$")]
        with patch.object(verification, "junit_result", side_effect=KeyboardInterrupt):
            with self.assertRaises(KeyboardInterrupt):
                self.evaluate(plan)
        self.assertEqual(verification.status(self.root)["status"], "failed")
        self.assertFalse(list(ws.local_path(self.root, "receipts").glob("*.json")))
        state_path = ws.local_path(self.root, "verification.json")
        state = ws.read_json(state_path)
        state["status"] = "running"
        ws.atomic_json(state_path, state)
        before = state_path.read_bytes()
        self.assertEqual(verification.status(self.root)["status"], "interrupted")
        self.assertEqual(before, state_path.read_bytes())


if __name__ == "__main__":
    unittest.main()
