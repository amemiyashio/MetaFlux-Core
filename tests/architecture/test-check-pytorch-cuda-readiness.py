#!/usr/bin/env python3
"""Exercise declaration drift and premature CPU-profile acceptance."""

import importlib.util
import json
from pathlib import Path
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("pytorch_readiness", ROOT / "tools/check-pytorch-cuda-readiness.py")
assert SPEC and SPEC.loader
checker = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(checker)


class ReadinessTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.corpus = {
            "schema_version": 1, "status": "frontier-not-frozen",
            "scope": {"compiled_subset": ["add"], "exit_gate_complete": False},
            "cases": [{"id": "add", "compiled": {"ptx": "inputs/add.ptx"}}, {"id": "sum"}],
            "gaps": [{"id": "unsupported", "status": "frontier-gap", "expected_error": "operation not supported"}],
        }
        self.goal = {"lanes": [{"work_item": checker.WORK_ID, "status": "planned"}]}
        self.document = """---
status: Active
---
| Corpus metric | Count |
| --- | --- |
| Supported cases | 2 |
| Compiled cases | 1 |
| Unique compiled PTX sources | 1 |
| Cases outside compiled subset | 1 |
| Classified gaps | 1 |
"""
        self.write("inputs/add.ptx", "fixture source")

    def write(self, path, content):
        destination = self.root / path
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_text(content, encoding="utf-8")

    def run_check(self):
        self.write(checker.CORPUS, json.dumps(self.corpus))
        self.write(checker.WORK_ITEM, self.document)
        self.write("agent/goal.json", json.dumps(self.goal))
        return checker.validate(self.root)

    def test_current_repository(self):
        self.assertFalse(checker.validate(ROOT)["execution_verified"])

    def test_consistent_frontier_is_not_execution_evidence(self):
        result = self.run_check()
        self.assertEqual(result["remaining_case_ids"], ["sum"])
        self.assertFalse(result["execution_verified"])

    def test_new_compiled_row_requires_updated_summary(self):
        self.corpus["cases"][1]["compiled"] = {"ptx": "inputs/add.ptx"}
        self.corpus["scope"]["compiled_subset"].append("sum")
        with self.assertRaisesRegex(ValueError, "Compiled cases"):
            self.run_check()

    def test_missing_source_is_rejected(self):
        (self.root / "inputs/add.ptx").unlink()
        with self.assertRaisesRegex(ValueError, "missing or external"):
            self.run_check()

    def test_subset_omission_or_extra_id_is_rejected(self):
        for subset in ([], ["add", "sum"], ["add", "missing"], ["add", "add"]):
            with self.subTest(subset=subset):
                self.corpus["scope"]["compiled_subset"] = subset
                with self.assertRaisesRegex(ValueError, "compiled_subset"):
                    self.run_check()

    def test_duplicate_summary_is_rejected(self):
        self.document += "| Supported cases | 2 |\n"
        with self.assertRaisesRegex(ValueError, "occur once"):
            self.run_check()

    def test_reordered_compiled_subset_is_rejected(self):
        self.corpus["cases"][1]["compiled"] = {"ptx": "inputs/add.ptx"}
        self.corpus["scope"]["compiled_subset"] = ["sum", "add"]
        with self.assertRaisesRegex(ValueError, "ordered cases"):
            self.run_check()

    def test_source_escape_is_rejected(self):
        self.corpus["cases"][0]["compiled"]["ptx"] = "../outside.ptx"
        with self.assertRaisesRegex(ValueError, "repository-relative"):
            self.run_check()

    def test_internal_source_alias_counts_once(self):
        (self.root / "inputs/alias.ptx").symlink_to("add.ptx")
        self.corpus["cases"][1]["compiled"] = {"ptx": "inputs/alias.ptx"}
        self.corpus["scope"]["compiled_subset"].append("sum")
        self.document = self.document.replace("| Compiled cases | 1 |", "| Compiled cases | 2 |")
        self.document = self.document.replace("| Cases outside compiled subset | 1 |", "| Cases outside compiled subset | 0 |")
        self.assertEqual(self.run_check()["counts"]["Unique compiled PTX sources"], 1)

    def test_goal_shape_errors_are_reported(self):
        for goal in ([], {}, {"lanes": None}, {"lanes": [None]}):
            with self.subTest(goal=goal):
                self.goal = goal
                with self.assertRaisesRegex(ValueError, "goal"):
                    self.run_check()

    def test_gap_requires_stable_error_classification(self):
        for gap in ({"id": "gap"}, {"id": "gap", "status": "frontier-gap", "expected_error": " "},
                    {"id": "gap", "status": "unknown", "expected_error": "error"}):
            with self.subTest(gap=gap):
                self.corpus["gaps"] = [gap]
                with self.assertRaisesRegex(ValueError, "classified gaps"):
                    self.run_check()

    def test_duplicate_or_overlapping_identifiers_are_rejected(self):
        self.corpus["cases"].append({"id": "add"})
        with self.assertRaisesRegex(ValueError, "duplicate cases"):
            self.run_check()
        self.corpus["cases"].pop()
        self.corpus["gaps"][0]["id"] = "add"
        with self.assertRaisesRegex(ValueError, "overlap"):
            self.run_check()

    def test_unfrozen_completion_is_rejected(self):
        self.corpus["scope"]["exit_gate_complete"] = True
        with self.assertRaisesRegex(ValueError, "frozen corpus"):
            self.run_check()

    def test_boolean_flag_is_not_a_string(self):
        self.corpus["scope"]["exit_gate_complete"] = "false"
        with self.assertRaisesRegex(ValueError, "boolean"):
            self.run_check()

    def test_goal_acceptance_or_work_item_completion_requires_full_corpus(self):
        self.goal["lanes"][0]["status"] = "integrated"
        with self.assertRaisesRegex(ValueError, "frozen corpus"):
            self.run_check()
        self.goal["lanes"][0]["status"] = "planned"
        self.document = self.document.replace("status: Active", "status: Complete")
        with self.assertRaisesRegex(ValueError, "frozen corpus"):
            self.run_check()

    def test_frozen_declaration_can_keep_classified_negative_cases(self):
        self.corpus["cases"][1]["compiled"] = {"ptx": "inputs/add.ptx"}
        self.corpus["scope"]["compiled_subset"].append("sum")
        self.corpus["status"] = "frozen"
        self.corpus["scope"]["exit_gate_complete"] = True
        self.goal["lanes"][0]["status"] = "integrated"
        self.document = self.document.replace("| Compiled cases | 1 |", "| Compiled cases | 2 |")
        self.document = self.document.replace("| Cases outside compiled subset | 1 |", "| Cases outside compiled subset | 0 |")
        result = self.run_check()
        self.assertEqual(result["counts"]["Classified gaps"], 1)
        self.assertFalse(result["execution_verified"])


if __name__ == "__main__":
    unittest.main()
