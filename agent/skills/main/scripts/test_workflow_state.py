#!/usr/bin/env python3
"""Independent current-operation, evidence, commit, and recovery scenarios."""

from __future__ import annotations

import copy
import io
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
import commit_as_agent_tool as commit_helper
import rule_loading as rules
import workflow_state as ws

SOURCE_ROOT = Path(__file__).resolve().parents[4]


def fixture(root: Path) -> str:
    root.mkdir(parents=True)
    ws.git(root, "init", "-q", "-b", "main")
    ws.git(root, "config", "user.name", "Fixture")
    ws.git(root, "config", "user.email", "fixture@example.invalid")
    (root / ".gitignore").write_text("/agent/tmp/\n")
    (root / "product.txt").write_text("baseline\n")
    shutil.copy2(SOURCE_ROOT / "AGENTS.md", root / "AGENTS.md")
    for name in ("main", "epoch", "batch", "iteration"):
        for source in (SOURCE_ROOT / "agent/skills" / name).rglob("*.md"):
            destination = root / source.relative_to(SOURCE_ROOT)
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, destination)
    # Exercise the actual metadata/transaction command in the fixture repo.
    # The fixture's small state/routing owners check its own reduced schema.
    for relative in ("agent/skills/main/scripts/main.py", "agent/skills/main/scripts/workflow_state.py",
                     "agent/skills/main/scripts/rule_loading.py", "agent/skills/main/scripts/verification.py",
                     "agent/skills/batch/scripts/batch.py"):
        destination = root / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(SOURCE_ROOT / relative, destination)
    (root / "tools").mkdir()
    shutil.copy2(SOURCE_ROOT / "tools/agent_diagnostics.py", root / "tools/agent_diagnostics.py")
    (root / "tools/check-agent-state.py").write_text(
        "import json\nfrom pathlib import Path\n"
        "goal = json.loads(Path('agent/goal.json').read_text())\n"
        "assert goal['schema_version'] == 4 and goal['lanes']\n")
    (root / "tools/check-skill-routing.py").write_text(
        "from pathlib import Path\n"
        "assert all((Path('agent/skills') / name / 'SKILL.md').is_file() "
        "for name in ('main', 'epoch', 'batch', 'iteration'))\n")
    lanes = [{"id": "lane-one", "work_item": "work-item-0.2.0.1", "iteration": "iteration-0002",
              "status": "planned", "depends_on": [], "outcome": "First result", "acceptance": ["Pass gate"]},
             {"id": "lane-two", "work_item": "work-item-0.2.0.2", "iteration": "iteration-0003",
              "status": "planned", "depends_on": ["lane-one"], "outcome": "Second result", "acceptance": ["Pass gate"]}]
    goal = {"schema_version": 4, "epoch": "epoch-0001", "batch": {"id": "batch-0001", "status": "open"},
            "target": {"milestone": "milestone-0.2.0.0", "work_item": "work-item-0.2.0.1"},
            "objective": "Fixture delivery", "references": [], "lanes": lanes}
    (root / "agent").mkdir(exist_ok=True)
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
    rules.load(root, kind, base, output=io.StringIO())
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
    if kind == "batch":
        value.pop("checks")
    return value


def load_operation_rules(root: Path) -> None:
    state = ws.read_json(ws.local_path(root, "state.json"))
    task = state["request"]
    committed = bool(state.get("commit"))
    rules.load(root, task["kind"], ws.oid(root) if committed else task["base_revision"],
               skills=task.get("skills", []), paths=[] if committed else task["allowed_paths"],
               output=io.StringIO())


def prepare(root: Path) -> dict:
    load_operation_rules(root)
    return controller.step(root, "prepared", {})


def commit_operation(root: Path, kind: str = "maintenance") -> str:
    with ws.lock(root):
        load_operation_rules(root)
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
        prepare(self.root)
        controller.step(self.root, "review", {"summary": "Reviewed failure fixture"})
        with self.assertRaises(ws.WorkflowError):
            controller.step(self.root, "evaluate", {})
        observed = controller.inspect(self.root)
        self.assertEqual(observed["stage"], "implementation")
        attempt = observed["evidence"]["verification"]
        self.assertEqual(attempt["request"], ws.read_json(ws.local_path(self.root, "state.json"))["run"])
        self.assertIn(task["objective"], observed["next_operation"])
        self.assertIn(attempt["log"], observed["next_operation"])
        self.assertIn("Required verification failed: fail", observed["next_operation"])
        self.assertEqual(ws.oid(self.root), self.base)

    def test_old_attempt_does_not_redirect_new_objective(self):
        task = request(self.root)
        controller.begin(self.root, task)
        prepare(self.root)
        ordinary = controller.inspect(self.root)["next_operation"]
        self.assertIn(task["objective"], ordinary)
        path = ws.local_path(self.root, "verification.json")
        for outcome in ("failed", "passed", "running"):
            ws.atomic_json(path, {"request": "another-operation", "status": outcome,
                                  "check": "old-check", "log": "old-log", "error": "old failure"})
            before = path.read_bytes()
            observed = controller.inspect(self.root)
            self.assertEqual(observed["next_operation"], ordinary)
            self.assertEqual(path.read_bytes(), before)

    def test_integration_guidance_uses_current_input_without_advancing(self):
        task = request(self.root, "batch")
        controller.begin(self.root, task)
        prepare(self.root)
        state = ws.read_json(ws.local_path(self.root, "state.json"))
        plan = checks()
        rules.load(self.root, "integration", self.base, output=io.StringIO())
        evidence = ws.evaluate(self.root, "integration", self.base, plan,
                               ws.review(self.root, "Review integration input and behavior", plan))
        before = ws.local_path(self.root, "state.json").read_bytes()
        observed = controller.inspect(self.root)
        self.assertEqual(observed["stage"], "implementation")
        self.assertIn(evidence["digest"], observed["next_operation"])
        self.assertEqual(ws.local_path(self.root, "state.json").read_bytes(), before)
        self.assertFalse(ws.local_path(self.root, "acceptance.json").exists())
        self.assertEqual(ws.read_json(self.root / "agent/goal.json")["lanes"][0]["iteration"], "iteration-0002")
        (self.root / "product.txt").write_text("new unverified content\n")
        changed = controller.inspect(self.root)
        self.assertNotIn(evidence["digest"], changed["next_operation"])
        self.assertIn("changed", changed["next_operation"])
        self.assertEqual(state["run"], ws.read_json(ws.local_path(self.root, "state.json"))["run"])
        # A pending file directs transaction checking, never assumes acceptance.
        ws.atomic_json(ws.local_path(self.root, "acceptance.json"), {})
        self.assertIn("check the pending acceptance metadata", controller.inspect(self.root)["next_operation"])
        self.assertEqual(ws.local_path(self.root, "state.json").read_bytes(), before)

    def test_staging_only_failure_preserves_receipt_and_delivers_without_retesting(self):
        controller.begin(self.root, request(self.root))
        prepare(self.root)
        (self.root / "product.txt").write_text("reviewed update\n")
        controller.step(self.root, "review", {"summary": "Review staging recovery behavior"})
        controller.step(self.root, "evaluate", {})
        state_path = ws.local_path(self.root, "state.json")
        self.assertIn("guarded commit", controller.inspect(self.root)["next_operation"])
        before_state = state_path.read_bytes()
        evidence = ws.read_json(state_path)["receipt"]
        verification_path = ws.local_path(self.root, "verification.json")
        before_verification = verification_path.read_bytes()
        logs = {result["log"]: Path(result["log"]).read_bytes() for result in evidence["results"]}
        with patch.object(ws, "evaluate", side_effect=AssertionError("unnecessary verification")):
            with self.assertRaisesRegex(ws.WorkflowError, "not fully staged.*product.txt.*same receipt"):
                controller.step(self.root, "deliver", {"agent_tool": "fixture-agent", "message": "Deliver"})
            with self.assertRaises(commit_helper.DiagnosticError) as failed_gate:
                commit_helper.check_commit_gate(self.root, {
                    "METAFLUX_EXPECTED_HEAD": self.base,
                    "METAFLUX_EXPECTED_TREE": ws.oid(self.root, ":"),
                    "METAFLUX_VERIFICATION_RECEIPT": str(ws.local_path(self.root, "receipts/" + evidence["digest"] + ".json")),
                    "METAFLUX_COMMIT_KIND": "maintenance",
                })
            self.assertIn("not fully staged", str(failed_gate.exception.diagnostic.evidence))
            self.assertNotIn("resume", failed_gate.exception.diagnostic.required_action)
            helper_result = subprocess.run([
                sys.executable, "-B", str(SOURCE_ROOT / "agent/skills/main/scripts/commit_as_agent_tool.py"),
                "--agent-tool", "fixture-agent", "--expected-head", self.base,
                "--expected-tree", ws.oid(self.root, ":"), "--kind", "maintenance",
                "--receipt", str(ws.local_path(self.root, "receipts/" + evidence["digest"] + ".json")),
                "--diagnostic-format", "json", "--", "-m", "Must remain unstaged",
            ], cwd=self.root, env=ws.environment(self.root), capture_output=True, text=True)
            self.assertEqual(helper_result.returncode, 2)
            diagnostic = next(json.loads(line)["errors"][0] for line in helper_result.stderr.splitlines() if line.startswith("{"))
            self.assertIn("not fully staged", str(diagnostic["evidence"]))
            self.assertNotIn("resume", diagnostic["required_action"])
            self.assertEqual(state_path.read_bytes(), before_state)
            ws.git(self.root, "add", "--", "product.txt")
            with ws.lock(self.root):
                controller.step(self.root, "deliver", {"agent_tool": "fixture-agent", "message": "Deliver staged content"})
        self.assertEqual(controller.inspect(self.root)["stage"], "publication")
        self.assertEqual(ws.read_json(state_path)["receipt"], evidence)
        self.assertEqual(verification_path.read_bytes(), before_verification)
        self.assertTrue(all(Path(path).read_bytes() == data for path, data in logs.items()))

    def test_unreviewed_index_is_not_committed_or_overwritten(self):
        controller.begin(self.root, request(self.root))
        prepare(self.root)
        (self.root / "product.txt").write_text("reviewed update\n")
        controller.step(self.root, "review", {"summary": "Review index isolation"})
        controller.step(self.root, "evaluate", {})
        bad_blob = ws.git(self.root, "hash-object", "-w", "--stdin", data=b"unreviewed staged content\n").decode().strip()
        ws.git(self.root, "update-index", "--cacheinfo", "100644", bad_blob, "product.txt")
        index = ws.git(self.root, "ls-files", "--stage", "-z")
        with self.assertRaisesRegex(ws.WorkflowError, "not fully staged"):
            controller.step(self.root, "deliver", {"agent_tool": "fixture-agent", "message": "Must reject"})
        self.assertEqual(ws.oid(self.root), self.base)
        self.assertEqual(ws.git(self.root, "ls-files", "--stage", "-z"), index)

    def test_stale_content_or_mode_is_not_reported_as_staging_only(self):
        controller.begin(self.root, request(self.root))
        prepare(self.root)
        product = self.root / "product.txt"
        product.write_text("reviewed update\n")
        controller.step(self.root, "review", {"summary": "Review stale content rejection"})
        controller.step(self.root, "evaluate", {})
        for mutation in (lambda: product.write_text("unreviewed update\n"), lambda: product.chmod(0o755)):
            mutation()
            with self.assertRaisesRegex(ws.WorkflowError, "Tested content or toolchain has changed"):
                controller.step(self.root, "deliver", {"agent_tool": "fixture-agent", "message": "Must reject"})
            product.write_text("reviewed update\n")
            product.chmod(0o644)

    def test_maintenance_commit_auto_publication_and_postcommit_recovery(self):
        goal = (self.root / "agent/goal.json").read_bytes()
        controller.begin(self.root, request(self.root))
        prepare(self.root)
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
        load_operation_rules(self.root)
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

    def test_revised_checks_require_new_rules_review_and_execution(self):
        task = request(self.root)
        controller.begin(self.root, task)
        prepare(self.root)
        controller.step(self.root, "review", {"summary": "Original check coverage reviewed"})
        controller.step(self.root, "evaluate", {})
        path = ws.local_path(self.root, "state.json")
        original = ws.read_json(path)
        revised = copy.deepcopy(task["checks"])
        revised[0]["argv"][-1] += "; assert 2 + 2 == 4"
        controller.step(self.root, "repair", {"checks": revised})
        current = ws.read_json(path)
        self.assertEqual(current["request"], {**task, "checks": revised})
        self.assertNotEqual(current["run"], original["run"])
        self.assertNotIn("review", current)
        self.assertNotIn("receipt", current)
        with self.assertRaises(ws.WorkflowError):
            controller.step(self.root, "review", {"summary": "Old rules must fail"})
        load_operation_rules(self.root)
        with self.assertRaises(ws.WorkflowError):
            controller.step(self.root, "evaluate", {})
        controller.step(self.root, "review", {"summary": "Revised coverage reviewed"})
        controller.step(self.root, "evaluate", {})
        self.assertNotEqual(ws.read_json(path)["receipt"]["digest"], original["receipt"]["digest"])
        saved = path.read_bytes()
        for invalid in ({"checks": []}, {"checks": [{"id": "replacement", "argv": ["true"]}]},
                        {"checks": [{**revised[0], "optional_skip_reason": "downgraded"}]},
                        {"checks": revised, "publication": "local"}):
            with self.assertRaises(ws.WorkflowError):
                controller.step(self.root, "repair", invalid)
            self.assertEqual(path.read_bytes(), saved)

    def test_local_delivery_skips_publication(self):
        task = request(self.root)
        task["publication"] = "local"
        controller.begin(self.root, task)
        prepare(self.root)
        (self.root / "product.txt").write_text("local result\n")
        commit_operation(self.root)
        self.assertEqual(controller.inspect(self.root)["stage"], "handoff")
        load_operation_rules(self.root)
        with self.assertRaises(ws.WorkflowError):
            controller.step(self.root, "repair", {"checks": checks()})
        with self.assertRaises(ws.WorkflowError):
            controller.step(self.root, "publish", {})

    def revise(self, *, paths=None, request=None, token=None):
        token = token or controller.inspect(self.root)["evidence"]["recovery_token"]
        with ws.lock(self.root):
            return controller.replace_precommit(self.root, token, "Necessary companion or confirmed governance",
                                                paths=paths, request=request)

    def test_omitted_summary_recovers_through_real_gate_and_candidate_commit(self):
        companion = "agent/plan/milestone-0.2.0.0-fixture/work/work-item-0.2.0.1-fixture.md"
        doc = self.root / companion
        doc.write_text(doc.read_text() + "\nCoverage: baseline\n")
        ws.git(self.root, "add", companion)
        ws.git(self.root, "commit", "-qm", "fixture summary authority")
        task = request(self.root, "iteration")
        task["allowed_paths"] = ["product.txt"]
        task["checks"] = [{"id": "coverage", "argv": [sys.executable, "-B", "-c",
            "from pathlib import Path; assert 'Coverage: ' + Path('product.txt').read_text().strip() in Path(" + repr(companion) + ").read_text()"]}]
        goal = (self.root / "agent/goal.json").read_bytes()
        controller.begin(self.root, task)
        prepare(self.root)
        (self.root / "product.txt").write_text("expanded\n")
        controller.step(self.root, "review", {"summary": "Review product and coverage consistency"})
        with self.assertRaisesRegex(ws.WorkflowError, "Required verification failed"):
            controller.step(self.root, "evaluate", {})
        self.assertEqual(self.revise(paths=["product.txt", companion])["stage"], "preparation")
        current = ws.read_json(ws.local_path(self.root, "state.json"))
        self.assertEqual(current["request"], {**task, "allowed_paths": ["product.txt", companion]})
        self.assertNotIn("review", current)
        self.assertNotIn("receipt", current)
        with self.assertRaises(ws.WorkflowError):
            controller.step(self.root, "prepared", {})
        prepare(self.root)
        doc.write_text(doc.read_text().replace("Coverage: baseline", "Coverage: expanded"))
        revision = commit_operation(self.root, "iteration")
        self.assertEqual(controller.inspect(self.root)["stage"], "handoff")
        self.assertEqual((self.root / "agent/goal.json").read_bytes(), goal)
        self.assertEqual(ws.read_json(ws.local_path(self.root, "state.json"))["request"]["assignment"], task["assignment"])
        self.assertIn(companion, ws.git(self.root, "diff-tree", "--no-commit-id", "--name-only", "-r", revision).decode())

    def test_scope_amendment_preserves_existing_outside_edits_and_modes(self):
        task = request(self.root)
        task["allowed_paths"] = ["product.txt"]
        controller.begin(self.root, task)
        prepare(self.root)
        outside = self.root / "summary.txt"
        outside.write_text("companion\n")
        outside.chmod(0o755)
        before = ws.snapshot(self.root)
        index = ws.git(self.root, "ls-files", "--stage", "-z")
        with self.assertRaisesRegex(ws.WorkflowError, "declared file scope"):
            self.revise(paths=["product.txt", "different.txt"])
        self.revise(paths=["product.txt", "summary.txt"])
        self.assertEqual(before, ws.snapshot(self.root))
        self.assertEqual(index, ws.git(self.root, "ls-files", "--stage", "-z"))
        prepare(self.root)

    def test_scope_amendment_discards_receipts_and_loads_new_domain_rules(self):
        # A real new ownership boundary requires its skill, even without edits there yet.
        src = SOURCE_ROOT / "agent/skills/runtime-contracts-registry/SKILL.md"
        dst = self.root / src.relative_to(SOURCE_ROOT)
        dst.parent.mkdir(parents=True)
        shutil.copy2(src, dst)
        ws.git(self.root, "add", str(dst.relative_to(self.root)))
        ws.git(self.root, "commit", "-qm", "fixture domain rules")
        task = request(self.root)
        controller.begin(self.root, task)
        prepare(self.root)
        controller.step(self.root, "review", {"summary": "Original candidate"})
        controller.step(self.root, "evaluate", {})
        old = ws.read_json(ws.local_path(self.root, "state.json"))
        self.revise(paths=[*task["allowed_paths"], "runtime/companion.txt"])
        for name in ("rules.json", "hook-context.json"):
            self.assertFalse(ws.local_path(self.root, name).exists())
        with self.assertRaises(ws.WorkflowError):
            ws.commit_guard(self.root, ws.oid(self.root), ws.oid(self.root, ":"), old["receipt"], kind="maintenance")
        prepare(self.root)
        self.assertIn("runtime-contracts-registry", ws.read_json(ws.local_path(self.root, "rules.json"))["skills"])
        with self.assertRaises(ws.WorkflowError):
            controller.step(self.root, "evaluate", {})

    def test_recovery_token_rejects_stale_index_content_state_and_replay(self):
        task = request(self.root)
        controller.begin(self.root, task)
        prepare(self.root)
        paths = [*task["allowed_paths"], "summary.txt"]
        token = controller.inspect(self.root)["evidence"]["recovery_token"]
        (self.root / "product.txt").write_text("changed\n")
        with self.assertRaisesRegex(ws.WorkflowError, "Recovery input changed"):
            self.revise(paths=paths, token=token)
        token = controller.inspect(self.root)["evidence"]["recovery_token"]
        before = ws.snapshot(self.root)
        ws.git(self.root, "add", "product.txt")
        self.assertEqual(before, ws.snapshot(self.root))
        with self.assertRaisesRegex(ws.WorkflowError, "Recovery input changed"):
            self.revise(paths=paths, token=token)
        token = controller.inspect(self.root)["evidence"]["recovery_token"]
        self.revise(paths=paths, token=token)
        saved = ws.local_path(self.root, "state.json").read_bytes()
        with self.assertRaisesRegex(ws.WorkflowError, "Recovery input changed"):
            self.revise(paths=[*paths, "second.txt"], token=token)
        self.assertEqual(saved, ws.local_path(self.root, "state.json").read_bytes())

    def test_scope_amendment_rejects_goal_escape_and_dropped_paths(self):
        task = request(self.root, "iteration")
        task["allowed_paths"] = ["product.txt"]
        controller.begin(self.root, task)
        (self.root / "external").symlink_to(self.root.parent, target_is_directory=True)
        saved = ws.local_path(self.root, "state.json").read_bytes()
        for paths in (["replacement.txt"], ["product.txt"], ["product.txt", "agent/goal.json"],
                      ["product.txt", "agent/"], ["product.txt", "../escape"], ["product.txt", "external/escape"],
                      ["product.txt", ".git/config"], ["product.txt", "agent/tmp/main/state.json"], ["product.txt", "./"]):
            with self.subTest(paths=paths), self.assertRaises(ws.WorkflowError):
                self.revise(paths=paths)
            self.assertEqual(saved, ws.local_path(self.root, "state.json").read_bytes())

    def test_recovery_rejects_tampered_request_and_changes_during_validation(self):
        task = request(self.root)
        controller.begin(self.root, task)
        state_path = ws.local_path(self.root, "state.json")
        original = ws.read_json(state_path)
        altered = copy.deepcopy(original)
        altered["request"]["objective"] = "Unrelated objective"
        ws.atomic_json(state_path, altered)
        with self.assertRaisesRegex(ws.WorkflowError, "Current request changed"):
            self.revise(paths=[*task["allowed_paths"], "summary.txt"])
        ws.atomic_json(state_path, original)
        validate = controller.validate_request
        def race(root, value):
            validate(root, value)
            (root / "product.txt").write_text("concurrent change\n")
        with patch.object(controller, "validate_request", race), self.assertRaisesRegex(ws.WorkflowError, "during validation"):
            self.revise(paths=[*task["allowed_paths"], "summary.txt"])
        self.assertEqual(ws.read_json(state_path), original)
        self.assertEqual((self.root / "product.txt").read_text(), "concurrent change\n")

    def test_explicit_epoch_replacement_preserves_git_candidate_and_resets_evidence(self):
        task = request(self.root, "iteration")
        controller.begin(self.root, task)
        prepare(self.root)
        product = self.root / "product.txt"
        product.write_text("staged portion\n")
        ws.git(self.root, "add", "product.txt")
        product.write_text("staged and unstaged portions\n")
        product.chmod(0o755)
        (self.root / "new-product.txt").write_text("untracked candidate\n")
        before = ws.entries(self.root)
        index = ws.git(self.root, "ls-files", "--stage", "-z")
        governance = {**request(self.root, "epoch"), "allowed_paths": ["AGENTS.md", "agent/goal.json"],
                      "confirmation": "User explicitly requested governance"}
        with self.assertRaisesRegex(ws.WorkflowError, "declared file scope"):
            self.revise(request=governance)
        ws.git(self.root, "stash", "push", "--include-untracked", "--", "product.txt", "new-product.txt")
        preserved = ws.oid(self.root, "refs/stash")
        self.revise(request=governance)
        self.assertEqual(controller.inspect(self.root)["stage"], "preparation")
        current = ws.read_json(ws.local_path(self.root, "state.json"))
        self.assertEqual(current["request"], governance)
        self.assertNotIn("assignment", current["request"])
        prepare(self.root)
        ws.git(self.root, "stash", "apply", "--index", preserved)
        self.assertEqual(before, ws.entries(self.root))
        self.assertEqual(index, ws.git(self.root, "ls-files", "--stage", "-z"))

    def test_replacement_rejects_unconfirmed_governance_and_pending_acceptance(self):
        controller.begin(self.root, request(self.root))
        prepare(self.root)
        saved = ws.local_path(self.root, "state.json").read_bytes()
        for value in (request(self.root), request(self.root, "epoch")):
            with self.assertRaises(ws.WorkflowError):
                self.revise(request=value)
            self.assertEqual(saved, ws.local_path(self.root, "state.json").read_bytes())
        goal = (self.root / "agent/goal.json").read_text()
        ws.begin_transaction(self.root, {"agent/goal.json": goal + "\n"}, {"fixture": True})
        with self.assertRaisesRegex(ws.WorkflowError, "Pending acceptance"):
            self.revise(paths=[*request(self.root)["allowed_paths"], "summary.txt"])
        self.assertEqual(saved, ws.local_path(self.root, "state.json").read_bytes())

    def test_replacement_interruption_never_revives_old_rule_certificate(self):
        task = request(self.root)
        controller.begin(self.root, task)
        prepare(self.root)
        old = ws.read_json(ws.local_path(self.root, "rules.json"))
        original_unlink = Path.unlink
        def interrupted(path, *args, **kwargs):
            if path == ws.local_path(self.root, "rules.json"):
                raise OSError("fixture interruption after state replacement")
            return original_unlink(path, *args, **kwargs)
        with patch.object(Path, "unlink", interrupted), self.assertRaises(OSError):
            self.revise(paths=[*task["allowed_paths"], "summary.txt"])
        self.assertEqual(ws.read_json(ws.local_path(self.root, "rules.json")), old)
        self.assertEqual(controller.inspect(self.root)["stage"], "preparation")
        with self.assertRaises(ws.WorkflowError):
            controller.step(self.root, "prepared", {})
        prepare(self.root)

    def test_replacement_rejects_committed_and_interrupted_commit_before_resume(self):
        controller.begin(self.root, request(self.root))
        prepare(self.root)
        (self.root / "product.txt").write_text("delivered\n")
        revision = commit_operation(self.root)
        with self.assertRaisesRegex(ws.WorkflowError, "Committed operations"):
            self.revise(request={**request(self.root, "epoch"), "confirmation": "Govern"})
        path = ws.local_path(self.root, "state.json")
        state = ws.read_json(path)
        state.pop("commit")
        state["stage"] = "delivery"
        ws.atomic_json(path, state)
        with self.assertRaisesRegex(ws.WorkflowError, "interrupted committed delivery"):
            self.revise(request={**request(self.root, "epoch"), "confirmation": "Govern"})
        self.assertEqual(controller.recover(self.root)["stage"], "publication")
        self.assertEqual(ws.read_json(path)["commit"], revision)

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
        load_operation_rules(self.root)
        with self.assertRaises(ws.WorkflowError):
            controller.step(self.root, "deliver", {})
        shutil.rmtree(self.root / "agent/tmp")
        result = controller.recover(self.root)
        self.assertIn("review and evaluate", result["next_operation"])
        self.assertEqual(ws.oid(self.root), self.base)


if __name__ == "__main__":
    unittest.main()
