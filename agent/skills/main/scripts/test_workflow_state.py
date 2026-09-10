#!/usr/bin/env python3
"""Independent current-operation, evidence, commit, and recovery scenarios."""

from __future__ import annotations

import copy
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parent))
import main as controller
import workflow_state as ws


def fixture(root: Path) -> str:
    root.mkdir(parents=True)
    ws.git(root, "init", "-q", "-b", "main")
    ws.git(root, "config", "user.name", "Fixture")
    ws.git(root, "config", "user.email", "fixture@example.invalid")
    (root / ".gitignore").write_text("/agent/tmp/\n")
    (root / "product.txt").write_text("baseline\n")
    lanes = [{"id": "lane-one", "work_item": "work-item-0.2.0.1", "iteration": "iteration-0002",
              "status": "planned", "depends_on": [], "outcome": "First result", "acceptance": ["Pass gate"]},
             {"id": "lane-two", "work_item": "work-item-0.2.0.2", "iteration": "iteration-0003",
              "status": "planned", "depends_on": ["lane-one"], "outcome": "Second result", "acceptance": ["Pass gate"]}]
    goal = {"schema_version": 4, "epoch": "epoch-0001", "batch": {"id": "batch-0001", "status": "open"},
            "target": {"milestone": "milestone-0.2.0.0", "work_item": "work-item-0.2.0.1"},
            "objective": "Fixture delivery", "references": [], "lanes": lanes}
    (root / "agent").mkdir()
    (root / "agent/goal.json").write_text(json.dumps(goal, indent=2) + "\n")
    work = root / "agent/plan/milestone-0.2.0.0-fixture/work"
    work.mkdir(parents=True)
    for number, status in ((1, "Active"), (2, "Queued")):
        (work / f"work-item-0.2.0.{number}-fixture.md").write_text(
            f"---\nid: work-item-0.2.0.{number}\nstatus: {status}\nupdated: 2026-09-10\n---\n\n## Exit Gate\n\nPass gate.\n")
    ws.git(root, "add", ".")
    ws.git(root, "commit", "-qm", "fixture baseline")
    return ws.oid(root)


def checks() -> list[dict]:
    return [{"id": "gate", "argv": [sys.executable, "-B", "-c",
             "from pathlib import Path; assert Path('product.txt').read_text().strip()"]}]


def receipt(root: Path, kind: str, base: str) -> dict:
    plan = checks()
    return ws.evaluate(root, kind, base, plan, ws.review(root, "Reviewed fixture behavior and gate coverage", plan))


def request(root: Path, kind: str = "maintenance") -> dict:
    value = {"schema_version": 1, "kind": kind, "objective": "Bounded fixture update",
             "base_revision": ws.oid(root), "allowed_paths": ["product.txt", "agent/goal.json", "agent/plan/"],
             "checks": checks(), "publication": "auto"}
    if kind in {"iteration", "batch"}:
        goal = ws.read_json(root / "agent/goal.json")
        lane = next(x for x in goal["lanes"] if x["work_item"] == goal["target"]["work_item"])
        value["assignment"] = {"epoch": goal["epoch"], "batch": goal["batch"]["id"],
                               "iteration": lane["iteration"], "lane": lane["id"]}
    return value


def commit_operation(root: Path, kind: str = "maintenance") -> str:
    with ws.lock(root):
        controller.step(root, "review", {"summary": "Fixture reviewed against bounded behavior"})
        controller.step(root, "evaluate", {})
        ws.git(root, "add", "product.txt", "agent/goal.json", "agent/plan")
        controller.step(root, "deliver", {"agent_tool": "fixture-agent", "message": "Verified fixture delivery"})
    return ws.oid(root)


class WorkflowScenarios(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="metaflux-workflow-")
        self.root = Path(self.temp.name) / "repo"
        self.base = fixture(self.root)

    def tearDown(self):
        self.temp.cleanup()

    def test_read_only_preserves_active_task_and_creates_nothing(self):
        before = ws.snapshot(self.root)
        controller.inspect(self.root)
        controller.begin(self.root, {"kind": "read-only"})
        self.assertFalse((self.root / "agent/tmp").exists())
        controller.begin(self.root, request(self.root))
        saved = ws.local_path(self.root, "state.json").read_bytes()
        controller.begin(self.root, {"kind": "read-only"})
        self.assertEqual(saved, ws.local_path(self.root, "state.json").read_bytes())
        self.assertEqual(before, ws.snapshot(self.root))

    def test_receipt_binds_tree_plan_tools_logs_and_kind(self):
        evidence = receipt(self.root, "maintenance", self.base)
        ws.validate_receipt(self.root, evidence, kind="maintenance")
        for key in ("kind", "digest"):
            altered = copy.deepcopy(evidence)
            altered[key] = "wrong"
            with self.assertRaises(ws.WorkflowError):
                ws.validate_receipt(self.root, altered, kind="maintenance")
        (self.root / "product.txt").write_text("changed\n")
        with self.assertRaises(ws.WorkflowError):
            ws.validate_receipt(self.root, evidence, kind="maintenance")
        (self.root / "product.txt").write_text("baseline\n")
        Path(evidence["results"][0]["log"]).write_text("tampered")
        with self.assertRaises(ws.WorkflowError):
            ws.validate_receipt(self.root, evidence, kind="maintenance")

    def test_failed_checks_return_to_current_scope(self):
        task = request(self.root)
        task["checks"] = [{"id": "fail", "argv": [sys.executable, "-B", "-c", "raise SystemExit(3)"]}]
        controller.begin(self.root, task)
        controller.step(self.root, "prepared", {})
        controller.step(self.root, "review", {"summary": "Reviewed failure fixture"})
        with self.assertRaises(ws.WorkflowError):
            controller.step(self.root, "evaluate", {})
        self.assertEqual(controller.inspect(self.root)["stage"], "implementation")
        self.assertEqual(ws.oid(self.root), self.base)

    def test_maintenance_commit_auto_publication_and_postcommit_recovery(self):
        goal = (self.root / "agent/goal.json").read_bytes()
        controller.begin(self.root, request(self.root))
        controller.step(self.root, "prepared", {})
        (self.root / "product.txt").write_text("maintained\n")
        revision = commit_operation(self.root)
        self.assertNotEqual(revision, self.base)
        self.assertEqual(controller.inspect(self.root)["stage"], "publication")
        self.assertEqual((self.root / "agent/goal.json").read_bytes(), goal)
        # Interruption after commit but before updating the cache recovers from Git.
        state = ws.read_json(ws.local_path(self.root, "state.json"))
        state.pop("commit")
        state["stage"] = "delivery"
        ws.atomic_json(ws.local_path(self.root, "state.json"), state)
        self.assertEqual(controller.recover(self.root)["stage"], "publication")
        shutil.rmtree(self.root / "agent/tmp")
        self.assertEqual(controller.recover(self.root, revision)["stage"], "publication")
        real_run = subprocess.run
        calls = []
        def publication(command, **kwargs):
            if any(str(x).endswith("push_repository.py") for x in command):
                calls.append(command)
                return subprocess.CompletedProcess(command, 0, json.dumps({"published_revision": revision,
                    "remote_main_revision": revision, "context_refresh_required": False}) + "\n", "")
            return real_run(command, **kwargs)
        with patch.object(controller.subprocess, "run", side_effect=publication):
            controller.step(self.root, "publish", {})
        self.assertEqual(len(calls), 1)
        self.assertIn(revision, calls[0])
        controller.step(self.root, "handoff", {"assignment_request": "Application supplies the next exact context"})
        self.assertEqual(ws.oid(self.root), revision)

    def test_local_delivery_skips_publication(self):
        task = request(self.root)
        task["publication"] = "local"
        controller.begin(self.root, task)
        controller.step(self.root, "prepared", {})
        (self.root / "product.txt").write_text("local result\n")
        commit_operation(self.root)
        self.assertEqual(controller.inspect(self.root)["stage"], "handoff")
        with self.assertRaises(ws.WorkflowError):
            controller.step(self.root, "publish", {})

    def test_stale_head_and_staging_reject_delivery(self):
        (self.root / "product.txt").write_text("reviewed\n")
        evidence = receipt(self.root, "maintenance", self.base)
        ws.git(self.root, "add", "product.txt")
        tree = ws.oid(self.root, ":")
        (self.root / "product.txt").write_text("unreviewed\n")
        with self.assertRaises(ws.WorkflowError):
            ws.commit_guard(self.root, self.base, tree, evidence, kind="maintenance")
        controller.begin(self.root, request(self.root))
        ws.git(self.root, "add", "product.txt")
        ws.git(self.root, "commit", "-qm", "external change")
        with self.assertRaises(ws.WorkflowError):
            controller.recover(self.root)

    def test_transaction_partial_write_staging_modes_and_external_edits(self):
        path = "agent/goal.json"
        original = (self.root / path).read_bytes()
        txn = ws.begin_transaction(self.root, {path: original.decode() + "\n"}, {"fixture": True})
        ws.restore_blob(self.root, path, txn["files"][path]["after"])
        ws.git(self.root, "add", path)
        self.assertIn("rolled-back", ws.recover_transaction(self.root))
        self.assertEqual(original, (self.root / path).read_bytes())
        self.assertEqual(ws.oid(self.root, ":"), txn["index"])
        txn = ws.begin_transaction(self.root, {path: original.decode() + "\n"}, {"fixture": True})
        ws.apply_transaction(self.root, txn)
        (self.root / path).write_text("external edit")
        with self.assertRaises(ws.WorkflowError):
            ws.recover_transaction(self.root)
        self.assertEqual((self.root / path).read_text(), "external edit")

    def test_common_lock_excludes_other_process(self):
        with ws.lock(self.root):
            result = subprocess.run([sys.executable, "-B", "-c",
                "import sys; from pathlib import Path; sys.path.insert(0, sys.argv[1]); import workflow_state as w; "
                "c=w.lock(Path(sys.argv[2])); c.__enter__()", str(Path(ws.__file__).parent), str(self.root)], capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(b"repository lock", result.stderr)

    def test_invalid_transition_and_precommit_cache_loss(self):
        controller.begin(self.root, request(self.root))
        with self.assertRaises(ws.WorkflowError):
            controller.step(self.root, "deliver", {})
        shutil.rmtree(self.root / "agent/tmp")
        result = controller.recover(self.root)
        self.assertIn("review and evaluate", result["next_operation"])
        self.assertEqual(ws.oid(self.root), self.base)


if __name__ == "__main__":
    unittest.main()
