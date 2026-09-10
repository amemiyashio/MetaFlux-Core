#!/usr/bin/env python3
"""Exercise actual rule emission, version invalidation, and workflow boundaries."""
from __future__ import annotations

import io
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

import main as controller
import rule_loading as rules
import test_workflow_state as fixture
import workflow_state as ws


class RuleLoadingTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="metaflux-rule-loading-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / "repository"
        self.base = fixture.fixture(self.root)

    def load(self, kind="maintenance", **kwargs):
        self.output = io.StringIO()
        return rules.load(self.root, kind, self.base, output=self.output, **kwargs)

    def test_prepared_requires_emitted_current_bodies(self):
        controller.begin(self.root, fixture.request(self.root))
        before = ws.snapshot(self.root)
        with self.assertRaisesRegex(ws.WorkflowError, "Load rule bodies"):
            controller.step(self.root, "prepared", {})
        self.assertEqual(ws.snapshot(self.root), before)
        certificate = self.load()
        for name in certificate["files"]:
            self.assertIn((self.root / name).read_text(), self.output.getvalue())
        self.assertEqual(controller.step(self.root, "prepared", {})["stage"], "implementation")

    def test_boolean_marker_is_not_rule_evidence(self):
        controller.begin(self.root, fixture.request(self.root))
        ws.atomic_json(ws.local_path(self.root, "rules.json"), {"loaded": True})
        with self.assertRaises(ws.WorkflowError):
            controller.step(self.root, "prepared", {})

    def test_changed_rule_requires_reload_and_fresh_review(self):
        task = fixture.request(self.root)
        task["allowed_paths"].append("AGENTS.md")
        controller.begin(self.root, task)
        fixture.prepare(self.root)
        controller.step(self.root, "review", {"summary": "Initial parent review"})
        path = self.root / "AGENTS.md"
        path.write_text(path.read_text() + "\nUpdated fixture rule.\n")
        with self.assertRaisesRegex(ws.WorkflowError, "rule version changed"):
            controller.step(self.root, "evaluate", {})
        self.load()
        with self.assertRaises(ws.WorkflowError):
            controller.step(self.root, "evaluate", {})
        controller.step(self.root, "review", {"summary": "Reviewed changed fixture rule"})
        self.assertEqual(controller.step(self.root, "evaluate", {})["stage"], "delivery")

    def test_mode_change_invalidates_loaded_rule(self):
        self.load()
        (self.root / "AGENTS.md").chmod(0o755)
        with self.assertRaisesRegex(ws.WorkflowError, "rule version changed"):
            rules.current(self.root)

    def test_edit_between_begin_and_prepare_preserved_but_rejected(self):
        controller.begin(self.root, fixture.request(self.root))
        self.load()
        path = self.root / "product.txt"
        path.write_text("premature write\n")
        with self.assertRaisesRegex(ws.WorkflowError, "before preparation"):
            controller.step(self.root, "prepared", {})
        self.assertEqual(path.read_text(), "premature write\n")

    def test_existing_user_edit_at_begin_is_preserved(self):
        path = self.root / "product.txt"
        path.write_text("existing user edit\n")
        controller.begin(self.root, fixture.request(self.root))
        fixture.prepare(self.root)
        self.assertEqual(path.read_text(), "existing user edit\n")

    def test_new_request_at_same_head_needs_its_own_load(self):
        controller.begin(self.root, fixture.request(self.root))
        self.load()
        ws.local_path(self.root, "state.json").unlink()
        task = fixture.request(self.root)
        task["objective"] = "A different bounded request"
        controller.begin(self.root, task)
        with self.assertRaisesRegex(ws.WorkflowError, "another operation"):
            controller.step(self.root, "prepared", {})
        fixture.prepare(self.root)

    def test_domain_selection_uses_actual_repository_owners(self):
        cases = {
            "plugins/compat/cuda/compiler/ptx/src/parser.cpp": "ptx-simt-semantics",
            "compiler/core/src/kernel_ir.cpp": "ptx-simt-semantics",
            "transports/vfio-user/": "gpu-virtualization-vfio-user",
            "transports/cdev/": "linux-device-driver-uapi",
            "runtime/core/src/lifecycle.cpp": "device-lifecycle-resilience",
            "flake.lock": "manage-toolchain",
        }
        for path, skill in cases.items():
            with self.subTest(path=path):
                self.assertIn(skill, rules.required(fixture.SOURCE_ROOT, "iteration", [path]))
        self.assertEqual(rules.required(fixture.SOURCE_ROOT, "maintenance", ["tools/check-agent-state.py"]), ["main"])

    def test_exact_new_and_removed_skill_paths_keep_a_usable_workflow(self):
        old = self.root / "agent/skills/retired/SKILL.md"
        old.parent.mkdir()
        old.write_text("Retired fixture rule.\n")
        ws.git(self.root, "add", str(old.relative_to(self.root)))
        ws.git(self.root, "commit", "-qm", "fixture old skill")
        self.base = ws.oid(self.root)
        task = fixture.request(self.root)
        task["allowed_paths"] = ["agent/skills/new-skill/", "agent/skills/retired/"]
        controller.begin(self.root, task)
        fixture.prepare(self.root)
        new = self.root / "agent/skills/new-skill/SKILL.md"
        new.parent.mkdir()
        new.write_text("New fixture rule.\n")
        old.unlink()
        fixture.load_operation_rules(self.root)
        controller.step(self.root, "review", {"summary": "Reviewed package creation and removal"})
        self.assertEqual(controller.step(self.root, "evaluate", {})["stage"], "delivery")

    def test_wrong_workflow_and_missing_scope_rules_rejected(self):
        controller.begin(self.root, fixture.request(self.root, "iteration"))
        self.load()
        with self.assertRaisesRegex(ws.WorkflowError, "another workflow"):
            controller.step(self.root, "prepared", {})
        self.load("iteration")
        controller.step(self.root, "prepared", {})
        path = self.root / "plugins/compat/cuda/abi/unloaded.cpp"
        path.parent.mkdir(parents=True)
        path.write_text("// changed domain without loading its skill\n")
        with self.assertRaisesRegex(ws.WorkflowError, "additional skill bodies"):
            rules.current(self.root, kind="iteration")

    def test_candidate_validates_its_rule_versions_not_current_checkout(self):
        (self.root / "product.txt").write_text("candidate result\n")
        evidence = fixture.receipt(self.root, "iteration", self.base)
        ws.git(self.root, "add", "product.txt")
        ws.git(self.root, "commit", "-qm", "fixture candidate")
        tip = ws.oid(self.root)
        path = self.root / "AGENTS.md"
        path.write_text(path.read_text() + "\nNew integration rule.\n")
        ws.validate_receipt(self.root, evidence, kind="iteration", revision=tip)
        with self.assertRaises(ws.WorkflowError):
            ws.validate_receipt(self.root, evidence, kind="iteration")

    def test_output_failure_and_missing_file_leave_no_receipt(self):
        class FailedOutput(io.StringIO):
            def flush(self):
                raise OSError("fixture output failed")
        with self.assertRaises(OSError):
            rules.load(self.root, "maintenance", self.base, output=FailedOutput())
        self.assertFalse(ws.local_path(self.root, "rules.json").exists())
        (self.root / "agent/skills/main/references/controller.md").unlink()
        with self.assertRaises(ws.WorkflowError):
            self.load()
        self.assertFalse(ws.local_path(self.root, "rules.json").exists())

    def test_rule_symlink_is_rejected(self):
        path = self.root / "AGENTS.md"
        external = Path(self.temp.name) / "external.md"
        external.write_text(path.read_text())
        path.unlink()
        path.symlink_to(external)
        with self.assertRaisesRegex(ws.WorkflowError, "regular file"):
            self.load()

    def test_resume_invalidates_precommit_loading(self):
        controller.begin(self.root, fixture.request(self.root))
        fixture.prepare(self.root)
        controller.recover(self.root)
        with self.assertRaisesRegex(ws.WorkflowError, "Load rule bodies"):
            controller.step(self.root, "review", {"summary": "Resumed review"})
        self.load()
        controller.step(self.root, "review", {"summary": "Reviewed after reloading rules"})

    def test_inline_bootstrap_creates_no_request_file(self):
        result = subprocess.run([sys.executable, "-B", str(Path(controller.__file__)), "--root", str(self.root),
                                 "begin", "--request-json", json.dumps(fixture.request(self.root))],
                                env=ws.environment(self.root), capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout)["stage"], "preparation")
        self.assertEqual([p.name for p in (self.root / "agent/tmp/main").iterdir()], ["state.json"])


if __name__ == "__main__":
    unittest.main()
