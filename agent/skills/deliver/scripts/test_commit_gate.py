#!/usr/bin/env python3
"""Exercise the installed Git hook and shared guard in isolated repositories."""

from __future__ import annotations

import json
import io
import os
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[4] / "agent/lib"))
sys.path.insert(0, str(Path(__file__).resolve().parents[4] / "agent/skills/main/scripts"))
sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_commit_as_agent_tool as fixture
import main as controller
import rule_loading

SOURCE_ROOT = Path(__file__).resolve().parents[4]


class CommitGateTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory(prefix="metaflux-commit-gate-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / "repository"
        fixture.initialize_repository(self.root)
        self.environment = fixture.environment()
        for name in (*fixture.HELPER.COMMIT_GATE_ENVIRONMENT.values(), "METAFLUX_MAIN_LOCK_FD"):
            self.environment.pop(name, None)
        for folder in ("agent/lib", "agent/skills/main/scripts", "agent/skills/prepare/scripts",
                       "agent/skills/deliver/scripts", "agent/skills/publish/scripts"):
            destination = self.root / folder
            destination.mkdir(parents=True, exist_ok=True)
            for source in (SOURCE_ROOT / folder).glob("*.py"):
                if not source.name.startswith("test_"):
                    shutil.copy2(source, destination / source.name)
        (self.root / "tools").mkdir()
        shutil.copy2(SOURCE_ROOT / "tools/agent_diagnostics.py", self.root / "tools/agent_diagnostics.py")
        # Candidate checks are small fixture programs. The hook, helper, and
        # workflow guard are the actual repository implementations.
        checks = (
            "tools/check-agent-state.py",
            "tools/check-skill-routing.py",
        )
        for name in checks:
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("# Isolated candidate-check fixture.\n", encoding="utf-8")
        (self.root / checks[0]).write_text(
            "import json, os, pathlib, subprocess\n"
            "git_local = subprocess.run(['git', 'rev-parse', '--local-env-vars'], capture_output=True, text=True, check=True).stdout.splitlines()\n"
            "assert not set(git_local).intersection(os.environ), 'caller Git local environment reached candidate checks'\n"
            "root = pathlib.Path(os.environ['METAFLUX_GATE_FIXTURE_ROOT'])\n"
            "(root / '.git/gate-ran').write_text('candidate checked')\n"
            "action = os.environ.get('METAFLUX_GATE_FIXTURE_CHANGE')\n"
            "if action == 'tree':\n"
            "    (root / 'value.txt').write_text('changed during hook\\n')\n"
            "    subprocess.run(['git', 'add', 'value.txt'], cwd=root, check=True)\n"
            "elif action == 'receipt':\n"
            "    receipt = json.loads(pathlib.Path(os.environ['METAFLUX_VERIFICATION_RECEIPT']).read_text())\n"
            "    pathlib.Path(receipt['results'][0]['log']).write_text('changed during hook\\n')\n",
            encoding="utf-8",
        )
        self.run_ok("git", "add", ".")
        self.run_ok("git", "commit", "-qm", "fixture gate implementation")
        hook = self.root / ".git/hooks/pre-commit"
        shutil.copy2(SOURCE_ROOT / ".githooks/pre-commit", hook)
        hook.chmod(0o755)
        # The test already runs in Nix. Forward only the hook's precise Nix
        # invocation to those tools, without resolving a fixture flake or
        # recursively invoking the full repository test suite.
        bindir = self.root / ".git/fixture-bin"
        bindir.mkdir()
        nix = bindir / "nix"
        nix.write_text(
            "#!/usr/bin/env bash\nset -euo pipefail\n"
            '[[ "$#" -ge 4 && "$1" == develop && "$2" == . && "$3" == --command ]]\n'
            'shift 3\nexec "$@"\n', encoding="utf-8",
        )
        nix.chmod(0o755)
        self.environment["PATH"] = str(bindir) + os.pathsep + self.environment["PATH"]
        self.environment["METAFLUX_GATE_FIXTURE_ROOT"] = str(self.root)
        self.environment["METAFLUX_DIAGNOSTIC_FORMAT"] = "json"
        self.head = fixture.WS.oid(self.root)
        (self.root / "value.txt").write_text("reviewed value\n", encoding="utf-8")
        self.run_ok("git", "add", "value.txt")

    def invoke(self, *args: str, environment=None):
        return fixture.run(self.root, *args, env=self.environment if environment is None else environment)

    def run_ok(self, *args: str):
        result = self.invoke(*args)
        fixture.require(result, " ".join(args))
        return result

    def evidence(self) -> list[str]:
        arguments = fixture.evidence_arguments(self.root)
        fields = dict(zip(arguments[::2], arguments[1::2]))
        for key, name in fixture.HELPER.COMMIT_GATE_ENVIRONMENT.items():
            self.environment[name] = fields["--" + key.replace("_", "-")]
        return arguments

    def assert_rejected(self, result, code: str, *, checked: bool = False) -> None:
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn(code, result.stderr)
        self.assertEqual(fixture.WS.oid(self.root), self.head)
        self.assertEqual((self.root / ".git/gate-ran").exists(), checked)

    def test_direct_git_requires_evidence(self) -> None:
        result = self.invoke("git", "commit", "-qm", "unqualified direct commit")
        self.assert_rejected(result, "commit-gate.evidence-required")

    def test_missing_receipt_rejected_with_exact_head_and_tree(self) -> None:
        self.evidence()
        self.environment.pop("METAFLUX_VERIFICATION_RECEIPT")
        result = self.invoke("git", "commit", "-qm", "missing receipt")
        self.assert_rejected(result, "commit-gate.evidence-required")
        self.assertIn("METAFLUX_VERIFICATION_RECEIPT", result.stderr)

    def test_missing_kind_rejected(self) -> None:
        self.evidence()
        self.environment.pop("METAFLUX_COMMIT_KIND")
        result = self.invoke("git", "commit", "-qm", "missing operation kind")
        self.assert_rejected(result, "commit-gate.evidence-required")
        self.assertIn("METAFLUX_COMMIT_KIND", result.stderr)

    def test_stale_tree_rejected_before_candidate_checks(self) -> None:
        self.evidence()
        (self.root / "value.txt").write_text("later untested value\n", encoding="utf-8")
        self.run_ok("git", "add", "value.txt")
        result = self.invoke("git", "commit", "-qm", "stale tree")
        self.assert_rejected(result, "commit-gate.input-changed")
        self.assertIn("Expected staged tree changed", result.stderr)

    def test_stale_receipt_rejected_before_candidate_checks(self) -> None:
        self.evidence()
        receipt = fixture.WS.read_json(Path(self.environment["METAFLUX_VERIFICATION_RECEIPT"]))
        Path(receipt["results"][0]["log"]).write_text("changed evidence\n", encoding="utf-8")
        result = self.invoke("git", "commit", "-qm", "stale receipt")
        self.assert_rejected(result, "commit-gate.input-changed")

    def test_helper_supplies_complete_gate_inputs(self) -> None:
        arguments = self.evidence()
        for name in fixture.HELPER.COMMIT_GATE_ENVIRONMENT.values():
            self.environment[name] = "untrusted inherited value"
        result = self.invoke(sys.executable, "-B", str(self.root / "agent/skills/deliver/scripts/commit_as_agent_tool.py"),
                             *arguments, "--", "-m", "qualified helper commit")
        fixture.require(result, "real hook helper commit")
        self.assertNotEqual(fixture.WS.oid(self.root), self.head)
        self.assertTrue((self.root / ".git/gate-ran").exists())
        self.assertEqual(self.run_ok("git", "config", "user.name").stdout.strip(), "Human")

    def helper(self, arguments: list[str]):
        return self.invoke(sys.executable, "-B", str(self.root / "agent/skills/deliver/scripts/commit_as_agent_tool.py"),
                           *arguments, "--", "-m", "guarded fixture candidate")

    def test_helper_rejects_receipt_without_active_operation(self) -> None:
        arguments = self.evidence()
        fixture.WS.local_path(self.root, "state.json").unlink()
        result = self.helper(arguments)
        self.assert_rejected(result, "commit-helper.input-changed")
        self.assertIn("active $main skill delivery", result.stderr)

    def test_helper_rejects_lost_rule_loading_cache(self) -> None:
        arguments = self.evidence()
        fixture.WS.local_path(self.root, "rules.json").unlink()
        result = self.helper(arguments)
        self.assert_rejected(result, "commit-helper.input-changed")
        self.assertIn("Load rule bodies", result.stderr)

    def test_resume_and_reload_does_not_restore_old_delivery(self) -> None:
        arguments = self.evidence()
        controller.recover(self.root)
        task = fixture.WS.read_json(fixture.WS.local_path(self.root, "state.json"))["request"]
        rule_loading.load(self.root, task["kind"], task["base_revision"],
                          paths=task["allowed_paths"], output=io.StringIO())
        result = self.helper(arguments)
        self.assert_rejected(result, "commit-helper.input-changed")
        self.assertIn("current $main skill delivery stage", result.stderr)

    def test_new_request_same_head_rejects_previous_receipt(self) -> None:
        arguments = self.evidence()
        path = fixture.WS.local_path(self.root, "state.json")
        task = fixture.WS.read_json(path)["request"]
        task["objective"] = "A separately reviewed fixture request"
        path.unlink()
        controller.begin(self.root, task)
        replacement = self.evidence()
        self.assertNotEqual(arguments, replacement)
        result = self.helper(arguments)
        self.assert_rejected(result, "commit-helper.input-changed")
        self.assertIn("Current operation receipt", result.stderr)

    def check_mutated_during_hook(self, change: str) -> None:
        arguments = self.evidence()
        self.environment["METAFLUX_GATE_FIXTURE_CHANGE"] = change
        result = self.invoke(sys.executable, "-B", str(self.root / "agent/skills/deliver/scripts/commit_as_agent_tool.py"),
                             *arguments, "--", "-m", "candidate changed during checks")
        self.assert_rejected(result, "commit-gate.input-changed", checked=True)

    def test_final_guard_rejects_changed_tree(self) -> None:
        self.check_mutated_during_hook("tree")

    def test_final_guard_rejects_changed_receipt(self) -> None:
        self.check_mutated_during_hook("receipt")


if __name__ == "__main__":
    unittest.main()
