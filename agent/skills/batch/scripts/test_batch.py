#!/usr/bin/env python3
"""Slice, whole-item, replay, and interrupted acceptance scenarios."""
from __future__ import annotations

import copy
import contextlib
import io
import shlex
import json
import shutil
import sys
import tempfile
import unittest
from unittest.mock import patch
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[4] / "agent/lib"))
sys.path.insert(0, str(Path(__file__).resolve().parents[4] / "agent/skills/main/scripts"))
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "main/scripts"))
import test_workflow_state as fixture
import main as controller
import workflow_state as ws
import batch
import tool_gate as gate


class BatchScenarios(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="metaflux-batch-")
        self.root = Path(self.temp.name) / "repo"
        self.base = fixture.fixture(self.root)
        (self.root / "product.txt").write_text("candidate\n")
        evidence = fixture.receipt(self.root, "iteration", self.base)
        ws.git(self.root, "add", "product.txt")
        ws.git(self.root, "commit", "-qm", "candidate")
        self.tip = ws.oid(self.root)
        self.delivery = {"schema_version": 2, "epoch": "epoch-0001", "batch": "batch-0001",
            "iteration": "iteration-0002", "lane": "lane-one", "base_revision": self.base, "tip_revision": self.tip,
            "acceptance_kind": "slice", "slice_objective": "One observed result",
            "verification_receipt": evidence, "exit_gate": None, "blockers": [], "knowledge_candidates": [],
            "tests": [{"command": x["argv"], "status": "passed"} for x in evidence["results"]]}

    def tearDown(self):
        self.temp.cleanup()

    def test_first_batch_intake_and_hook_keep_the_delivery_baseline(self):
        delivery_path = ws.local_path(self.root, "delivery.json")
        ws.atomic_json(delivery_path, self.delivery)
        def event(action):
            argv = ["python3", "-B", "agent/skills/batch/scripts/batch.py",
                    action, str(delivery_path), "--root", str(self.root)]
            return {"hook_event_name": "PreToolUse", "tool_name": "Bash",
                    "tool_input": {"command": "nix develop . --ignore-environment --keep HOME --keep USER --command " + shlex.join(argv)},
                    "cwd": str(self.root), "session_id": "fixture-session", "turn_id": "first-intake"}
        self.assertEqual(gate.handle(self.root, event("check")), {})
        self.assertFalse(ws.local_path(self.root, "state.json").exists())
        self.assertEqual(batch.check_delivery(self.delivery, self.root)["action"], "in-place")
        controller.begin(self.root, fixture.request(self.root, "batch"))
        fixture.prepare(self.root)
        self.assertEqual(gate.handle(self.root, event("load-rules")), {})
        with contextlib.redirect_stdout(io.StringIO()):
            batch.load_integration_rules(self.root, self.delivery)
        injected = gate.handle(self.root, event("verify"))["hookSpecificOutput"]
        self.assertEqual(injected["permissionDecision"], "deny")
        self.assertIn("MetaFlux action: integration", injected["additionalContext"])
        self.assertEqual(ws.read_json(ws.local_path(self.root, "rules.json"))["base_revision"], self.base)
        self.assertNotEqual(self.base, self.tip)
        self.assertEqual(gate.handle(self.root, event("verify")), {})
        result = batch.verify_delivery(self.root, self.delivery, fixture.checks(), "Review exact combined behavior")
        evidence = ws.read_json(Path(result["receipt"]))
        self.assertEqual(evidence["base_revision"], self.base)
        self.assertEqual(evidence["input"]["head"], self.tip)
        self.assertEqual(gate.handle(self.root, event("advance")), {})
        self.assertEqual(ws.read_json(ws.local_path(self.root, "rules.json"))["digest"], evidence["verification_rules"]["digest"])
        batch.advance_delivery(self.delivery, self.root, integration_receipt=evidence, state_validator=lambda root: None)
        self.assertEqual(controller.inspect(self.root)["action_card"]["action"], "review")

    def test_final_plan_is_generated_and_product_plan_rejected_before_execution(self):
        task = fixture.request(self.root, "batch")
        wrong = {**task, "checks": fixture.checks()}
        with self.assertRaisesRegex(ws.WorkflowError, "Batch final checks"):
            controller.begin(self.root, wrong)
        self.assertFalse(ws.local_path(self.root, "state.json").exists())
        controller.begin(self.root, task)
        state_path = ws.local_path(self.root, "state.json")
        self.assertEqual(ws.read_json(state_path)["request"]["checks"], controller.batch_metadata_checks())
        fixture.prepare(self.root)
        before = state_path.read_bytes()
        with patch.object(controller.verification, "preflight", side_effect=AssertionError("product enumeration started")):
            with self.assertRaisesRegex(ws.WorkflowError, "Batch final checks"):
                controller.preflight(self.root, fixture.checks())
        with self.assertRaisesRegex(ws.WorkflowError, "Batch final checks"):
            controller.step(self.root, "repair", {"checks": fixture.checks()})
        self.assertEqual(state_path.read_bytes(), before)

    def advance(self, *, validator=lambda root: None):
        evidence = fixture.receipt(self.root, "integration", self.base)
        return batch.advance_delivery(self.delivery, self.root, integration_receipt=evidence, state_validator=validator)

    def test_slice_preserves_target_and_number_and_replay(self):
        goal = ws.read_json(self.root / "agent/goal.json")
        work = batch.find_work_item(self.root, "work-item-0.2.0.1")
        before_work = work.read_bytes()
        controller.begin(self.root, fixture.request(self.root, "batch"))
        fixture.prepare(self.root)
        result = self.advance()
        after = ws.read_json(self.root / "agent/goal.json")
        self.assertEqual(after["target"], goal["target"])
        self.assertEqual(after["lanes"][0]["status"], "planned")
        self.assertEqual(after["lanes"][0]["iteration"], "iteration-0004")
        self.assertEqual(result["next"]["id"], "lane-one")
        self.assertEqual(before_work, work.read_bytes())
        txn = ws.read_json(ws.local_path(self.root, "acceptance.json"))
        revision = fixture.commit_operation(self.root, "batch")
        # A published/completed Batch must not constrain the next task's
        # explicit read-only preflight (even before the next begin).
        state_path = ws.local_path(self.root, "state.json")
        state = ws.read_json(state_path)
        state["stage"] = "complete"
        ws.atomic_json(state_path, state)
        before_preflight = state_path.read_bytes()
        self.assertIn("checks", controller.preflight(self.root, fixture.checks()))
        self.assertEqual(state_path.read_bytes(), before_preflight)
        self.assertEqual(batch.check_delivery(self.delivery, self.root)["action"], "no-op")
        ws.atomic_json(ws.local_path(self.root, "acceptance.json"), txn)
        controller.recover(self.root)
        self.assertFalse(ws.local_path(self.root, "acceptance.json").exists())
        shutil.rmtree(self.root / "agent/tmp")
        self.assertEqual(batch.advance_delivery(self.delivery, self.root)["accepted_revision"], revision)
        self.assertEqual(ws.oid(self.root), revision)

    def test_work_item_requires_complete_gate(self):
        self.delivery["acceptance_kind"] = "work-item"
        with self.assertRaises(ws.WorkflowError):
            batch.check_delivery(self.delivery, self.root)
        work = batch.find_work_item(self.root, "work-item-0.2.0.1")
        self.delivery["exit_gate"] = {"digest": ws.digest(batch.exit_gate(work.read_text())), "checks": ["gate"]}
        result = self.advance()
        goal = ws.read_json(self.root / "agent/goal.json")
        self.assertEqual(goal["lanes"][0]["status"], "integrated")
        self.assertIn("status: Complete", work.read_text())
        self.assertEqual(result["next"]["id"], "lane-two")
        self.assertEqual(goal["epoch"], "epoch-0001")

    def test_missing_forged_and_stale_evidence(self):
        before = (self.root / "agent/goal.json").read_bytes()
        with self.assertRaises(ws.WorkflowError):
            batch.advance_delivery(self.delivery, self.root)
        bad = copy.deepcopy(self.delivery)
        bad["verification_receipt"] = {"results": [{"argv": ["true"], "returncode": 0}]}
        with self.assertRaises(ws.WorkflowError):
            batch.check_delivery(bad, self.root)
        evidence = fixture.receipt(self.root, "integration", self.base)
        (self.root / "product.txt").write_text("changed after checks\n")
        with self.assertRaises(ws.WorkflowError):
            batch.advance_delivery(self.delivery, self.root, integration_receipt=evidence)
        self.assertEqual(before, (self.root / "agent/goal.json").read_bytes())

    def test_validation_failure_restores_owned_writes(self):
        before = ws.snapshot(self.root)
        def fail(root):
            raise ws.WorkflowError("fixture state gate failed")
        with self.assertRaises(ws.WorkflowError):
            self.advance(validator=fail)
        self.assertEqual(before, ws.snapshot(self.root))
        self.assertFalse(ws.local_path(self.root, "acceptance.json").exists())

    def test_array_order_cycles_exhaustion_and_batch_completion(self):
        goal = ws.read_json(self.root / "agent/goal.json")
        goal["lanes"][0]["iteration"] = "iteration-0008"
        goal["lanes"][1]["depends_on"] = []
        self.assertEqual(batch.next_ready(goal)["id"], "lane-one")
        cyclic = copy.deepcopy(goal)
        cyclic["lanes"][0]["depends_on"] = ["lane-two"]
        cyclic["lanes"][1]["depends_on"] = ["lane-one"]
        with self.assertRaises(ws.WorkflowError):
            batch.validate_goal(cyclic)
        goal["lanes"][0]["iteration"] = "iteration-0002"
        goal["lanes"][1]["iteration"] = "iteration-9999"
        with self.assertRaises(ws.WorkflowError):
            batch.transition(goal, self.delivery)
        goal["lanes"] = goal["lanes"][:1]
        whole = copy.deepcopy(self.delivery)
        whole["acceptance_kind"] = "work-item"
        final = batch.transition(goal, whole)
        self.assertEqual(final["batch"], {"id": "batch-0001", "status": "integrated"})
        self.assertEqual(final["epoch"], goal["epoch"])
        self.assertEqual(final["target"], goal["target"])

    def test_unrelated_trailer_and_stale_ancestor(self):
        ws.git(self.root, "commit", "--allow-empty", "-qm", "MetaFlux-Acceptance: " + json.dumps({"delivery": ws.digest(self.delivery)}))
        with self.assertRaises(ws.WorkflowError):
            batch.check_delivery(self.delivery, self.root)
        self.assertIsNone(batch.accepted_revision(self.root, self.delivery))

    def test_prepared_merge_and_knowledge_promotion_replay(self):
        work = batch.find_work_item(self.root, "work-item-0.2.0.1")
        work.write_text(work.read_text() + "\n## Implementation\n\nCandidate fact.\n")
        candidate_evidence = fixture.receipt(self.root, "iteration", self.base)
        ws.git(self.root, "add", "agent/plan")
        ws.git(self.root, "commit", "-qm", "candidate knowledge")
        candidate = ws.oid(self.root)
        self.delivery["tip_revision"] = candidate
        self.delivery["verification_receipt"] = candidate_evidence
        self.delivery["acceptance_kind"] = "work-item"
        self.delivery["exit_gate"] = {"digest": ws.digest(batch.exit_gate(work.read_text())), "checks": ["gate"]}
        ws.git(self.root, "checkout", "-q", "--detach", self.base)
        (self.root / "main.txt").write_text("Independent main change\n")
        ws.git(self.root, "add", "main.txt")
        ws.git(self.root, "commit", "-qm", "new integration parent")
        parent = ws.oid(self.root)
        controller.begin(self.root, fixture.request(self.root, "batch"))
        fixture.prepare(self.root)
        ws.git(self.root, "merge", "--no-commit", "--no-ff", candidate)
        self.assertEqual(ws.oid(self.root), parent)
        work.write_text(work.read_text() + "\nIntegration knowledge.\n")
        self.advance()
        revision = fixture.commit_operation(self.root, "batch")
        self.assertEqual(batch.check_delivery(self.delivery, self.root)["accepted_revision"], revision)
        self.assertEqual(len(ws.git(self.root, "show", "-s", "--format=%P", revision).split()), 2)

    def test_post_transition_tampering_rejected_at_commit(self):
        self.advance()
        document = ws.read_json(self.root / "agent/goal.json")
        document["objective"] = "Unrelated altered objective"
        ws.atomic_json(self.root / "agent/goal.json", document)
        with self.assertRaises(ws.WorkflowError):
            batch.validate_pending(self.root)

    def test_lost_merge_state_rejected_at_commit(self):
        ws.git(self.root, "checkout", "-q", "--detach", self.base)
        controller.begin(self.root, fixture.request(self.root, "batch"))
        fixture.prepare(self.root)
        ws.git(self.root, "merge", "--no-commit", "--no-ff", self.tip)
        self.advance()
        batch.validate_pending(self.root)
        before = ws.snapshot(self.root)
        ws.git(self.root, "merge", "--quit")
        self.assertEqual(ws.snapshot(self.root), before)
        with self.assertRaisesRegex(ws.WorkflowError, "candidate merge identity changed"):
            fixture.commit_operation(self.root, "batch")
        self.assertEqual(ws.oid(self.root), self.base)

    def test_integration_cli_derives_delivery_base_and_rejects_wrong_rules_before_execution(self):
        controller.begin(self.root, fixture.request(self.root, "batch"))
        fixture.prepare(self.root)
        marker = ws.local_path(self.root, "executed")
        checks = [{"id": "gate", "argv": [sys.executable, "-B", "-c",
                   f"from pathlib import Path; Path({str(marker)!r}).write_text('executed once')"]}]
        with self.assertRaisesRegex(ws.WorkflowError, "another baseline|Rule action|current action"):
            batch.verify_delivery(self.root, self.delivery, checks, "Review integration")
        self.assertFalse(marker.exists())
        batch.load_integration_rules(self.root, self.delivery)
        result = batch.verify_delivery(self.root, self.delivery, checks, "Review integration")
        self.assertEqual(result["base_revision"], self.base)
        self.assertEqual(result["head"], self.tip)
        evidence = ws.read_json(Path(result["receipt"]))
        batch.advance_delivery(self.delivery, self.root, integration_receipt=evidence, state_validator=lambda root: None)
        before = marker.stat().st_mtime_ns
        checked = batch.check_metadata(self.root)
        self.assertEqual(checked["changed_paths"], ["agent/goal.json"])
        self.assertEqual(checked["integration_receipt"], evidence["digest"])
        self.assertEqual(before, marker.stat().st_mtime_ns)
        # Run the real final commands after advance. Integration's product
        # sentinel must not execute again, including at guarded delivery.
        fixture.load_operation_rules(self.root)
        controller.step(self.root, "review", {"summary": "Review exact metadata-only transition"})
        controller.step(self.root, "evaluate", {})
        final = ws.read_json(ws.local_path(self.root, "state.json"))["receipt"]
        self.assertEqual(final["checks"], controller.batch_metadata_checks())
        self.assertEqual(len(final["results"]), 3)
        self.assertTrue(all(result["returncode"] == 0 for result in final["results"]))
        self.assertEqual(before, marker.stat().st_mtime_ns)
        fixture.load_operation_rules(self.root, action="deliver")
        ws.git(self.root, "add", "--", "agent/goal.json")
        with ws.lock(self.root):
            controller.step(self.root, "deliver", {"agent_tool": "fixture-agent", "message": "Accept metadata only"})
        self.assertEqual(before, marker.stat().st_mtime_ns)
        self.assertEqual(controller.inspect(self.root)["stage"], "publication")

    def test_bad_candidate_and_missing_transaction_fail_before_checks(self):
        controller.begin(self.root, fixture.request(self.root, "batch"))
        fixture.prepare(self.root)
        broken = copy.deepcopy(self.delivery)
        broken["base_revision"] = self.tip
        with patch.object(ws, "evaluate", side_effect=AssertionError("long check started")):
            with self.assertRaisesRegex(ws.WorkflowError, "Empty or invalid"):
                batch.verify_delivery(self.root, broken, fixture.checks(), "Review")
            controller.step(self.root, "review", {"summary": "Review final transaction"})
            with self.assertRaisesRegex(ws.WorkflowError, "Missing acceptance transaction"):
                controller.step(self.root, "evaluate", {})

    def test_metadata_gate_rejects_source_mode_and_exit_gate_changes(self):
        self.advance()
        work = batch.find_work_item(self.root, "work-item-0.2.0.1")
        product = self.root / "product.txt"
        for path in (product, work):
            before = path.read_bytes()
            path.write_bytes(before + b"\nChanged semantic input.\n")
            with self.assertRaises(ws.WorkflowError):
                batch.check_metadata(self.root)
            path.write_bytes(before)
        mode = product.stat().st_mode
        product.chmod(0o755)
        with self.assertRaises(ws.WorkflowError):
            batch.check_metadata(self.root)
        product.chmod(mode)
        batch.check_metadata(self.root)

    def test_final_evaluation_rejects_semantic_tampering_before_commands(self):
        controller.begin(self.root, fixture.request(self.root, "batch"))
        fixture.prepare(self.root)
        self.advance()
        fixture.load_operation_rules(self.root)
        product = self.root / "product.txt"
        goal = self.root / "agent/goal.json"
        work = batch.find_work_item(self.root, "work-item-0.2.0.1")
        for path in (product, goal, work):
            before = path.read_bytes()
            for change in (lambda: path.write_bytes(before + b"\n"), lambda: path.chmod(0o755)):
                change()
                controller.step(self.root, "review", {"summary": "Review final gate rejection fixture"})
                with patch.object(ws, "evaluate", side_effect=AssertionError("final commands started")):
                    with self.assertRaises(ws.WorkflowError):
                        controller.step(self.root, "evaluate", {})
                path.write_bytes(before)
                path.chmod(0o644)
        batch.check_metadata(self.root)

    def test_skipped_ctest_is_not_whole_exit_gate_evidence(self):
        work = batch.find_work_item(self.root, "work-item-0.2.0.1")
        delivery = copy.deepcopy(self.delivery)
        delivery["acceptance_kind"] = "work-item"
        delivery["exit_gate"] = {"digest": ws.digest(batch.exit_gate(work.read_text())), "checks": ["whole"]}
        with self.assertRaisesRegex(ws.WorkflowError, "actually pass"):
            batch.require_exit_gate(self.root, delivery, {"work_item": "work-item-0.2.0.1"},
                                    {"results": [{"id": "whole", "returncode": 0, "ctest": {"skipped": ["required-device"]}}]})


if __name__ == "__main__":
    unittest.main()
