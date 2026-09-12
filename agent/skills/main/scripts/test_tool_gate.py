#!/usr/bin/env python3
"""Fixture scenarios for rule injection and workflow-aware tool gating."""

from __future__ import annotations

import contextlib
import io
import json
import shlex
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[4] / "agent/lib"))
sys.path.insert(0, str(Path(__file__).resolve().parents[4] / "agent/skills/main/scripts"))
sys.path.insert(0, str(Path(__file__).resolve().parent))
import tool_gate as gate
import workflow_state as ws


class ToolGateScenarios(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory(prefix="metaflux-tool-gate-")
        self.root = Path(self.temp.name) / "repo"
        self.root.mkdir()
        ws.git(self.root, "init", "-q", "-b", "main")
        self.hooks = self.root / ".git/fixture-hooks"
        self.hooks.mkdir()
        ws.git(self.root, "config", "core.hooksPath", str(self.hooks))
        files = {
            ".gitignore": "/agent/tmp/\n",
            "AGENTS.md": "Fixture governance body: load skills first.\n",
            "agent/skills/main/SKILL.md": "---\nname: main\ndescription: Fixture main\n---\nEntire main rule body.\n",
            "agent/skills/main/references/controller.md": "Fixture controller contract.\n",
            "product.txt": "baseline\n",
            "outside.txt": "unrelated\n",
            "agent/goal.json": "{}\n",
        }
        source_root = Path(__file__).resolve().parents[4]
        for skill in ("main", "prepare", "review", "verify", "deliver", "publish", "recover"):
            for source in (source_root / "agent/skills" / skill).rglob("*.md"):
                files.setdefault(source.relative_to(source_root).as_posix(), source.read_text())
        for name, body in files.items():
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(body, encoding="utf-8")
        ws.git(self.root, "add", ".")
        self.commit("Fixture baseline")
        self.base = ws.oid(self.root)

    def tearDown(self) -> None:
        self.temp.cleanup()

    def commit(self, message: str) -> None:
        ws.git(self.root, "-c", "user.name=Fixture", "-c", "user.email=fixture@example.invalid",
               "commit", "--allow-empty", "-qm", message)

    def activate(self, stage: str = "implementation", **changes) -> dict:
        request = {"schema_version": 1, "kind": "maintenance", "objective": "Fixture bounded change",
                   "base_revision": self.base, "allowed_paths": ["product.txt", "docs/", "agent/skills/main/"],
                   "checks": [{"id": "fixture", "argv": ["python3", "-B", "tests/fixture.py"]}],
                   "publication": "auto"}
        request.update(changes)
        state = {"schema_version": 1, "request": request, "run": ws.digest(request), "stage": stage}
        ws.atomic_json(ws.local_path(self.root, "state.json"), state)
        return state

    def event(self, tool: str = "apply_patch", command: str | None = None, **changes) -> dict:
        if command is None:
            command = "*** Begin Patch\n*** Update File: product.txt\n@@\n-baseline\n+changed\n*** End Patch"
        event = {"hook_event_name": "PreToolUse", "tool_name": tool, "tool_input": {"command": command},
                 "cwd": str(self.root), "session_id": "fixture-session", "turn_id": "fixture-turn"}
        event.update(changes)
        return event

    def invoke(self, event: dict) -> dict:
        with contextlib.redirect_stderr(io.StringIO()):
            return gate.handle(self.root, event)

    def denied(self, event: dict) -> dict:
        response = self.invoke(event)
        self.assertEqual(response["hookSpecificOutput"]["permissionDecision"], "deny", response)
        return response["hookSpecificOutput"]

    def shell(self, argv: list[str]) -> dict:
        return self.event("Bash", "nix develop . --ignore-environment --keep HOME --keep USER --command " + shlex.join(argv))

    def controller(self, *args: str) -> dict:
        return self.shell(["python3", "-B", gate.MAIN_SCRIPT, "--root", ".", *args])

    def prime(self) -> None:
        self.denied(self.event())
        self.assertEqual(self.invoke(self.event()), {})

    def test_read_only_and_lifecycle_start_create_nothing(self) -> None:
        for event in (self.event("Bash", "git status --short"),
                      self.shell(["bash", "-c", "sed -n '1,20p' product.txt; git diff --check"]),
                      self.event("mcp__filesystem__read_file", tool_input={"path": "product.txt"})):
            self.assertEqual(self.invoke(event), {})
        for name in ("SessionStart", "SubagentStart", "PostCompact"):
            self.invoke({"hook_event_name": name})
        self.assertFalse((self.root / "agent/tmp").exists())
        state = self.activate()
        self.assertEqual(self.invoke(self.event("Bash", "git status --short")), {})
        self.assertEqual(ws.read_json(ws.local_path(self.root, "state.json")), state)
        self.assertFalse(ws.local_path(self.root, "rules.json").exists())

    def test_first_intake_and_publication_diagnostics_are_read_only(self) -> None:
        scripts = ("agent/skills/batch/scripts/batch.py", "agent/skills/publish/scripts/push_repository.py")
        for argv in ([scripts[0], "check", "DELIVERY.json", "--root", "."],
                     [scripts[0], "check-metadata", "--root", "."],
                     [scripts[1], "--root", ".", "check"],
                     [scripts[1], "check", "--remote-access"]):
            command = ["python3", "-B", *argv]
            self.assertEqual(self.invoke(self.shell(command)), {})
            self.denied(self.event("Bash", shlex.join(command)))
        self.denied(self.shell(["python3", "-B", scripts[0], "check", "DELIVERY.json", "--root", ".."]))
        self.denied(self.shell(["bash", "-c", "python3 -B " + scripts[0] + " check DELIVERY.json; touch product.txt"]))
        self.assertFalse((self.root / "agent/tmp").exists())
        state = self.activate("complete")
        self.assertEqual(self.invoke(self.shell(["python3", "-B", scripts[0], "check", "DELIVERY.json"])), {})
        self.assertEqual(ws.read_json(ws.local_path(self.root, "state.json")), state)

    def test_bootstrap_does_not_need_a_request_file_write(self) -> None:
        for event in (self.controller("inspect"), self.controller("begin", "--request-json", "{}"),
                      self.controller("preflight"), self.controller("preflight", "--checks", "agent/tmp/main/checks.json"),
                      self.controller("load-rules"), self.controller("resume"),
                      self.shell(["python3", "-B", "agent/skills/prepare/scripts/detect_agent_tool.py", "--agent-tool", "codex", "--json"])):
            self.assertEqual(self.invoke(event), {})
        self.denied(self.event())
        self.denied(self.event("Bash", "python3 agent/skills/main/scripts/main.py inspect"))
        self.denied(self.event("Bash", "cat product.txt"))
        self.denied(self.event("Bash", "cat product.txt; git status --short"))
        self.assertFalse((self.root / "agent/tmp").exists())

    def test_startup_skill_link_uses_the_current_checkout(self) -> None:
        event = {"hook_event_name": "SessionStart"}
        notice = self.invoke(event)["hookSpecificOutput"]["additionalContext"]
        self.assertIn(f"[$main]({self.root / 'agent/skills/main/SKILL.md'}) skill", notice)
        self.assertNotIn(str(gate.ROOT), notice)
        self.assertFalse((self.root / "agent/tmp").exists())

    def test_nix_entry_excludes_inherited_tool_and_loader_environment(self) -> None:
        command = "nix develop . --command python3 -B " + gate.MAIN_SCRIPT + " inspect"
        self.denied(self.event("Bash", command))
        clean = command.replace("--command", "--ignore-environment --command")
        self.assertEqual(self.invoke(self.event("Bash", clean)), {})
        for variable in ("PATH", "LD_PRELOAD", "LD_LIBRARY_PATH", "PYTHONPATH", "PYTHONHOME", "BASH_ENV", "SHELL"):
            self.denied(self.event("Bash", clean.replace("--command", "--keep " + variable + " --command")))
        self.assertFalse((self.root / "agent/tmp").exists())

    def test_full_rules_are_injected_and_first_write_never_runs(self) -> None:
        self.activate()
        response = self.denied(self.event())
        for name in ("AGENTS.md", "agent/skills/main/SKILL.md", "agent/skills/review/references/implementation-guidance.md"):
            self.assertIn((self.root / name).read_text(), response["additionalContext"])
        self.assertEqual((self.root / "product.txt").read_text(), "baseline\n")
        self.assertEqual(self.invoke(self.event()), {})
        binding = ws.local_path(self.root, "hook-context.json").read_text()
        self.assertNotIn("fixture-session", binding)
        self.assertNotIn("fixture-turn", binding)
        self.assertEqual(set(json.loads(binding)), {"schema_version", "context", "run", "rules"})

    def test_preparation_and_later_stages_do_not_admit_edits(self) -> None:
        self.activate("preparation")
        self.denied(self.event())
        self.assertFalse(ws.local_path(self.root, "rules.json").exists())
        self.assertIn("additionalContext", self.denied(self.controller("step", "prepared")))
        self.assertEqual(self.invoke(self.controller("step", "prepared")), {})
        for stage in ("review", "evaluation", "delivery", "complete"):
            self.activate(stage)
            self.denied(self.event())

    def test_changed_rule_requires_another_full_load(self) -> None:
        self.activate()
        self.prime()
        path = self.root / "agent/skills/main/SKILL.md"
        path.write_text(path.read_text() + "New rule body.\n")
        self.assertIn("New rule body.", self.denied(self.event())["additionalContext"])
        self.assertEqual(self.invoke(self.event()), {})

    def test_context_cannot_borrow_parent_or_previous_turn_receipt(self) -> None:
        self.activate()
        self.prime()
        saved = ws.local_path(self.root, "hook-context.json").read_bytes()
        self.invoke({"hook_event_name": "SubagentStart", "session_id": "fixture-session",
                     "turn_id": "fixture-turn", "agent_id": "child"})
        self.assertEqual(saved, ws.local_path(self.root, "hook-context.json").read_bytes())
        child = self.event(turn_id="child-turn")
        self.assertIn("additionalContext", self.denied(child))
        self.assertEqual(self.invoke(child), {})
        self.assertIn("additionalContext", self.denied(self.event()))
        self.assertIn("additionalContext", self.denied(self.event(agent_id="another-child")))
        self.denied(self.event(turn_id=None))

    def test_compaction_invalidates_context_and_preserves_uncommitted_state(self) -> None:
        state = self.activate()
        self.prime()
        response = self.invoke({"hook_event_name": "PostCompact"})
        self.assertEqual(set(response), {"systemMessage"})
        self.assertEqual(ws.read_json(ws.local_path(self.root, "state.json")), state)
        self.assertFalse(ws.local_path(self.root, "hook-context.json").exists())
        self.assertIn("additionalContext", self.denied(self.event()))

    def test_compaction_preserves_no_state_read_only_and_complete_records(self) -> None:
        self.invoke({"hook_event_name": "PostCompact"})
        self.assertFalse((self.root / "agent/tmp").exists())
        for stage, kind in (("implementation", "read-only"), ("complete", "maintenance")):
            state = self.activate(stage, kind=kind)
            binding_path = ws.local_path(self.root, "hook-context.json")
            ws.atomic_json(binding_path, {"fixture": "untouched"})
            saved = binding_path.read_bytes()
            self.invoke({"hook_event_name": "PostCompact"})
            self.assertEqual(binding_path.read_bytes(), saved)
            self.assertEqual(ws.read_json(ws.local_path(self.root, "state.json")), state)

    def test_targets_include_rename_destination_and_symlink_resolution(self) -> None:
        self.activate()
        self.prime()
        for target in ("outside.txt", "../outside.txt", str(self.root.parent / "escape.txt"), "agent/tmp/main/state.json", "agent/goal.json"):
            self.denied(self.event(command=f"*** Begin Patch\n*** Update File: product.txt\n*** Move to: {target}\n@@\n-baseline\n+changed\n*** End Patch"))
        (self.root / "docs").mkdir()
        (self.root / "docs/link").symlink_to(self.root.parent, target_is_directory=True)
        self.denied(self.event(command="*** Begin Patch\n*** Add File: docs/link/escape.txt\n+bad\n*** End Patch"))

    def test_stale_head_request_or_existing_out_of_scope_changes_block(self) -> None:
        self.activate()
        self.prime()
        (self.root / "outside.txt").write_text("outside change\n")
        self.denied(self.event())
        (self.root / "outside.txt").write_text("unrelated\n")
        state = ws.read_json(ws.local_path(self.root, "state.json"))
        state["request"]["objective"] = "Different request"
        ws.atomic_json(ws.local_path(self.root, "state.json"), state)
        self.denied(self.event())
        self.activate()
        self.commit("Unrelated HEAD change")
        self.denied(self.event())

    def test_shell_grammar_and_unknown_tools_are_not_assumed_read_only(self) -> None:
        for command in ("cat product.txt > outside.txt", "git status; touch product.txt", "cat product.txt\ntouch product.txt",
                        "sed -i 's/a/b/' product.txt", "find . -exec touch product.txt ';'", "python3 -c 'print(1)'"):
            self.denied(self.shell(["bash", "-c", command]))
        opaque = self.event("mcp__other__opaque", tool_input={"value": "fixture"})
        self.denied(opaque)
        self.activate()
        self.assertIn("additionalContext", self.denied(opaque))
        self.assertEqual(self.invoke(opaque), {})
        self.assertEqual(self.invoke(self.shell(["python3", "-c", "print('fixture')"])), {})
        self.denied(self.event("Bash", "python3 -c 'print(1)'"))

    def test_known_mcp_writes_check_paths(self) -> None:
        self.activate()
        self.prime()
        self.assertEqual(self.invoke(self.event("mcp__filesystem__write_file", tool_input={"path": "product.txt", "content": "fixture"})), {})
        self.denied(self.event("mcp__filesystem__move_file", tool_input={"source": "product.txt", "destination": "outside.txt"}))

    def test_callable_and_canonical_tool_input_shapes(self) -> None:
        self.assertEqual(self.invoke(self.event("exec_command", tool_input={"cmd": "git status --short"})), {})
        self.denied(self.event("Bash", tool_input={"cmd": "git status", "command": "touch product.txt"}))
        self.activate()
        patch_event = self.event()
        patch_event["tool_input"] = patch_event["tool_input"]["command"]
        self.assertIn("additionalContext", self.denied(patch_event))
        self.assertEqual(self.invoke(patch_event), {})

    def test_recovery_parser_exception_does_not_enable_writes_or_compounds(self) -> None:
        state = self.activate()
        (self.root / "outside.txt").write_text("necessary companion discovered\n")
        token = gate.controller.inspect(self.root)["evidence"]["recovery_token"]
        args = ["rescope", "--expected-state", token, "--reason", "Same-task companion",
                "--paths-json", json.dumps([*state["request"]["allowed_paths"], "outside.txt"])]
        self.assertEqual(self.invoke(self.controller(*args)), {})
        self.assertEqual(self.invoke(self.controller("supersede", "--expected-state", token,
            "--reason", "Explicit governance", "--request-json", "{}")), {})
        self.denied(self.event())
        compound = "python3 -B " + gate.MAIN_SCRIPT + " " + shlex.join(args) + "; touch product.txt"
        self.denied(self.shell(["bash", "-c", compound]))
        with ws.lock(self.root):
            gate.controller.replace_precommit(self.root, token, "Same-task companion",
                paths=[*state["request"]["allowed_paths"], "outside.txt"])
        self.denied(self.event())
        self.denied(self.controller("step", "evaluate"))
        self.denied(self.controller("step", "deliver", "--payload-json", "{}"))
        with contextlib.redirect_stdout(io.StringIO()):
            gate.controller.load_rules(self.root)
        self.assertIn("additionalContext", self.denied(self.controller("step", "prepared")))
        self.assertEqual(self.invoke(self.controller("step", "prepared")), {})
        gate.controller.step(self.root, "prepared", {})
        self.assertIn("MetaFlux action: implement", self.denied(self.event())["additionalContext"])
        self.assertEqual(self.invoke(self.event()), {})

    def test_exact_check_plan_and_delivery_commands_remain_available(self) -> None:
        self.activate()
        self.prime()
        self.activate("evaluation")
        self.assertEqual(self.invoke(self.shell(["python3", "-B", "tests/fixture.py"])), {})
        self.denied(self.shell(["python3", "-B", "tests/another.py"]))
        self.assertIn("MetaFlux action: verify", self.denied(self.controller("step", "evaluate"))["additionalContext"])
        self.assertEqual(self.invoke(self.controller("step", "evaluate")), {})
        self.denied(self.controller("step", "publish"))
        self.activate("delivery")
        self.assertIn("MetaFlux action: deliver", self.denied(self.shell(["git", "add", "--", "product.txt"]))["additionalContext"])
        self.assertEqual(self.invoke(self.shell(["git", "add", "--", "product.txt"])), {})
        self.assertEqual(self.invoke(self.shell(["git", "add", "--", "agent/skills/main/"])), {})
        self.denied(self.shell(["git", "add", "--", "outside.txt"]))
        self.denied(self.shell(["git", "add", "-A"]))
        self.assertEqual(self.invoke(self.controller("step", "deliver", "--payload-json", "{}")), {})
        self.assertEqual(self.invoke(self.shell(["python3", "-B", "agent/skills/deliver/scripts/commit_as_agent_tool.py", "--help"])), {})
        self.denied(self.shell(["git", "commit", "-m", "bypass"]))
        self.denied(self.shell(["git", "push"]))

    def test_compound_declared_check_uses_the_exact_planned_argv(self) -> None:
        argv = ["bash", "-c", "set -e; for fixture in product.txt; do test -f \"$fixture\"; done"]
        self.activate("evaluation", checks=[{"id": "loop", "argv": argv}])
        event = self.shell(argv)
        self.assertIn("additionalContext", self.denied(event))
        self.assertEqual(self.invoke(event), {})
        self.denied(self.shell(["bash", "-c", argv[2] + "; touch outside.txt"]))
        self.assertEqual(self.invoke(self.shell(["bash", "-c", "python3 -B agent/skills/main/scripts/main.py --root . inspect"])), {})

    def test_bash_working_directory_is_part_of_target_resolution(self) -> None:
        self.activate("delivery")
        event = self.shell(["git", "add", "--", "product.txt"])
        event["tool_input"]["workdir"] = str(self.root.parent)
        self.denied(event)
        event["tool_input"]["workdir"] = str(self.root / "agent")
        self.denied(event)
        event = self.event("Bash", "nix develop .. --ignore-environment --command git add -- skills/main/",
                           tool_input={"command": "nix develop .. --ignore-environment --command git add -- skills/main/", "workdir": str(self.root / "agent")})
        self.assertIn("additionalContext", self.denied(event))
        self.assertEqual(self.invoke(event), {})

    def test_project_hooks_use_sync_full_context_and_all_local_tools(self) -> None:
        config = json.loads((gate.ROOT / ".codex/hooks.json").read_text())
        self.assertEqual(set(config["hooks"]), {"SessionStart", "SubagentStart", "PreToolUse", "PostCompact"})
        for event, groups in config["hooks"].items():
            handler = groups[0]["hooks"][0]
            self.assertFalse(handler.get("async", False))
            self.assertTrue(handler["command"].startswith("nix develop . "))
            self.assertIsNotNone(gate.nix_inner(self.root, self.root, shlex.split(handler["command"])))
            self.assertIn("git rev-parse --show-toplevel", handler["command"])
            if event != "PostCompact":
                self.assertEqual(handler["additionalContextLimit"], 0)
        self.assertEqual(config["hooks"]["PreToolUse"][0]["matcher"], ".*")

    def test_publication_preserves_exact_commit_and_compaction_recovery(self) -> None:
        state = self.activate()
        tree = ws.oid(self.root, "HEAD^{tree}")
        record = {"schema_version": 1, "kind": "maintenance", "publication": "auto", "run": state["run"],
                  "head": self.base, "tree": tree, "receipt": "a" * 64}
        self.commit("Fixture delivery\n\nMetaFlux-Workflow: " + json.dumps(record))
        state.update(commit=ws.oid(self.root), stage="publication", receipt={"fixture": "preserved receipt"})
        ws.atomic_json(ws.local_path(self.root, "state.json"), state)
        event = self.shell(["python3", "-B", "agent/skills/publish/scripts/push_repository.py", "--root", ".", "push", "--revision", state["commit"]])
        self.assertIn("additionalContext", self.denied(event))
        certificate = ws.read_json(ws.local_path(self.root, "rules.json"))
        self.assertEqual(certificate["base_revision"], state["commit"])
        self.assertEqual(certificate["head"], state["commit"])
        self.assertIsNone(certificate["request"])
        gate.controller.require_rules(self.root, state)
        self.assertEqual(self.invoke(event), {})
        self.invoke({"hook_event_name": "PostCompact"})
        self.assertFalse(ws.local_path(self.root, "hook-context.json").exists())
        self.assertEqual(ws.read_json(ws.local_path(self.root, "state.json")), state)
        self.assertIn("additionalContext", self.denied(event))
        gate.controller.require_rules(self.root, state)
        self.assertEqual(self.invoke(event), {})
        self.denied(self.shell(["python3", "-B", "agent/skills/publish/scripts/push_repository.py", "push", "--revision", self.base]))
        state["stage"] = "handoff"
        ws.atomic_json(ws.local_path(self.root, "state.json"), state)
        self.invoke({"hook_event_name": "PostCompact"})
        self.assertFalse(ws.local_path(self.root, "hook-context.json").exists())
        self.assertEqual(ws.read_json(ws.local_path(self.root, "state.json")), state)
        handoff = self.controller("step", "handoff", "--payload-json", "{}")
        self.assertIn("additionalContext", self.denied(handoff))
        gate.controller.require_rules(self.root, state)
        self.assertEqual(self.invoke(handoff), {})

    def test_output_limit_issues_no_partial_receipt(self) -> None:
        self.activate()
        with patch.object(gate, "MAX_CONTEXT_BYTES", 12):
            response = self.denied(self.event())
        self.assertNotIn("additionalContext", response)
        self.assertFalse(ws.local_path(self.root, "rules.json").exists())
        self.assertFalse(ws.local_path(self.root, "hook-context.json").exists())

    def test_stdout_is_one_json_document_and_failures_use_stderr(self) -> None:
        script = Path(gate.__file__)
        output = subprocess.run([sys.executable, "-B", str(script), "--root", str(self.root)],
                                input=json.dumps(self.event()), text=True, capture_output=True,
                                env=ws.environment(self.root), cwd=self.root)
        self.assertEqual(output.returncode, 0)
        self.assertEqual(json.loads(output.stdout)["hookSpecificOutput"]["permissionDecision"], "deny")
        self.assertIn("workflow.tool-precondition", output.stderr)
        output = subprocess.run([sys.executable, "-B", str(script), "--root", str(self.root)],
                                input="bad json", text=True, capture_output=True,
                                env=ws.environment(self.root), cwd=self.root)
        self.assertEqual(output.returncode, 2)
        self.assertEqual(output.stdout, "")


if __name__ == "__main__":
    unittest.main()
