#!/usr/bin/env python3
"""Self-tests for the deterministic, read-only change inventory helper."""

from __future__ import annotations

import contextlib
import importlib.util
import io
import json
import os
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
from pathlib import Path


SCRIPT = Path(__file__).with_name("change_inventory.py")
SPEC = importlib.util.spec_from_file_location("metaflux_change_inventory", SCRIPT)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError(f"cannot load {SCRIPT}")
INVENTORY = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = INVENTORY
SPEC.loader.exec_module(INVENTORY)


def run(
    arguments: list[str],
    *,
    cwd: Path,
    check: bool = True,
    input_data: bytes | None = None,
) -> subprocess.CompletedProcess[bytes]:
    environment = os.environ.copy()
    environment.update(
        {
            "GIT_CONFIG_GLOBAL": os.devnull,
            "GIT_CONFIG_NOSYSTEM": "1",
            "GIT_CONFIG_SYSTEM": os.devnull,
            "LC_ALL": "C",
        }
    )
    result = subprocess.run(
        arguments,
        cwd=cwd,
        env=environment,
        input=input_data,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if check and result.returncode != 0:
        raise AssertionError(
            f"command failed ({result.returncode}): {arguments!r}\n"
            f"stdout={result.stdout!r}\nstderr={result.stderr!r}"
        )
    return result


def git(repo: Path, *arguments: str) -> bytes:
    return run(["git", *arguments], cwd=repo).stdout


def write(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")


def init_repo(root: Path) -> tuple[Path, str]:
    repo = root / "repo"
    repo.mkdir()
    git(repo, "init", "-q")
    git(repo, "config", "user.name", "Inventory Fixture")
    git(repo, "config", "user.email", "inventory@example.invalid")
    write(repo / "tracked.txt", "initial\n")
    git(repo, "add", "tracked.txt")
    git(repo, "commit", "-q", "-m", "initial")
    return repo, git(repo, "rev-parse", "HEAD").decode("ascii").strip()


def commit_all(repo: Path, message: str) -> str:
    git(repo, "add", "--all")
    git(repo, "commit", "-q", "-m", message)
    return git(repo, "rev-parse", "HEAD").decode("ascii").strip()


def invoke(repo: Path, *arguments: str) -> subprocess.CompletedProcess[bytes]:
    return run(
        [sys.executable, "-B", os.fspath(SCRIPT), "--repo-root", os.fspath(repo), *arguments],
        cwd=repo,
        check=False,
    )


def repository_state(repo: Path) -> tuple[bytes, bytes, bytes]:
    return (
        git(repo, "rev-parse", "HEAD"),
        git(repo, "status", "--porcelain=v1", "-z", "--untracked-files=all"),
        git(repo, "ls-files", "--stage", "-z"),
    )


def write_session(
    repo: Path,
    session_id: str,
    *,
    base: str,
    status: str,
    final: str | None,
) -> Path:
    session_dir = repo / "agent" / "sessions" / "2026" / "08" / session_id
    session_dir.mkdir(parents=True, exist_ok=True)
    document = {
        "id": session_id,
        "base_revision": base,
        "final_revision": final,
        "status": status,
    }
    (session_dir / "session.json").write_text(
        json.dumps(document, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    return session_dir


class ChangeInventoryTests(unittest.TestCase):
    def test_clean_workspace_is_empty_and_read_only(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            repo, head = init_repo(Path(directory))
            before = repository_state(repo)

            result = invoke(repo)

            self.assertEqual(result.returncode, 0, result.stderr.decode())
            self.assertEqual(result.stderr, b"")
            document = json.loads(result.stdout)
            self.assertEqual(document["workspace_revision"], head)
            self.assertEqual(document["scope"]["base_revision"], head)
            self.assertEqual(document["scope"]["end_revision"], head)
            self.assertEqual(
                document["changes"],
                {"committed": [], "staged": [], "unstaged": [], "untracked": []},
            )
            self.assertEqual(repository_state(repo), before)

    def test_all_layers_and_unusual_paths_are_stable(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            repo, base = init_repo(Path(directory))
            write(repo / "rename-source.txt", "rename payload\n")
            write(repo / "delete-me.txt", "delete payload\n")
            write(repo / "copy-source.txt", "copy payload\n")
            write(repo / "both.txt", "base payload\n")
            base = commit_all(repo, "seed inventory paths")

            renamed_path = "renamed\tline\n\u96ea.txt"
            (repo / "rename-source.txt").rename(repo / renamed_path)
            (repo / "delete-me.txt").unlink()
            write(repo / "copy-target.txt", "copy payload\n")
            write(repo / "committed-add.txt", "committed\n")
            commit_all(repo, "committed inventory range")

            write(repo / "both.txt", "staged payload\n")
            git(repo, "add", "both.txt")
            write(repo / "both.txt", "unstaged payload\n")
            write(repo / "space name.txt", "space\n")
            write(repo / "snow-\u96ea.txt", "unicode\n")
            write(repo / "line\nbreak.txt", "newline\n")
            before = repository_state(repo)

            first = invoke(repo, "--base", base)
            second = invoke(repo, "--base", base)

            self.assertEqual(first.returncode, 0, first.stderr.decode())
            self.assertEqual(first.stdout, second.stdout)
            self.assertEqual(first.stderr, b"")
            self.assertIn(b"snow-\\u96ea.txt", first.stdout)
            self.assertIn(b"line\\nbreak.txt", first.stdout)
            document = json.loads(first.stdout)
            committed = document["changes"]["committed"]
            by_status = {entry["status"] for entry in committed}
            self.assertTrue({"C", "D", "R"}.issubset(by_status), committed)
            self.assertIn(
                {
                    "old_path": "rename-source.txt",
                    "path": renamed_path,
                    "score": 100,
                    "status": "R",
                },
                committed,
            )
            self.assertIn(
                {
                    "old_path": "copy-source.txt",
                    "path": "copy-target.txt",
                    "score": 100,
                    "status": "C",
                },
                committed,
            )
            self.assertIn(
                {"path": "both.txt", "status": "M"},
                document["changes"]["staged"],
            )
            self.assertIn(
                {"path": "both.txt", "status": "M"},
                document["changes"]["unstaged"],
            )
            untracked = {entry["path"] for entry in document["changes"]["untracked"]}
            self.assertEqual(
                untracked,
                {"line\nbreak.txt", "snow-\u96ea.txt", "space name.txt"},
            )
            self.assertEqual(repository_state(repo), before)

    def test_active_and_terminal_sessions_select_different_endpoints(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            repo, base = init_repo(Path(directory))
            write(repo / "tracked.txt", "delivered\n")
            delivered = commit_all(repo, "delivered change")
            session_id = "S0100-20260830-001-inventory-fixture"
            session_dir = write_session(
                repo,
                session_id,
                base=base,
                status="in_progress",
                final=None,
            )

            active = invoke(repo, "--session", session_id)

            self.assertEqual(active.returncode, 0, active.stderr.decode())
            active_document = json.loads(active.stdout)
            self.assertEqual(active_document["scope"]["end_revision"], delivered)
            self.assertEqual(active_document["scope"]["end_source"], "workspace-head")
            self.assertEqual(active_document["scope"]["session_status"], "in_progress")

            write_session(
                repo,
                session_id,
                base=base,
                status="complete",
                final=delivered,
            )
            write(repo / "later.txt", "outside delivered range\n")
            later = commit_all(repo, "later workspace change")
            terminal = invoke(repo, "--session", session_id)

            self.assertEqual(terminal.returncode, 0, terminal.stderr.decode())
            terminal_document = json.loads(terminal.stdout)
            self.assertEqual(terminal_document["scope"]["end_revision"], delivered)
            self.assertEqual(terminal_document["scope"]["end_source"], "session-final")
            self.assertEqual(terminal_document["workspace_revision"], later)
            committed_paths = {
                entry["path"] for entry in terminal_document["changes"]["committed"]
            }
            self.assertEqual(committed_paths, {"tracked.txt"})
            self.assertTrue((session_dir / "session.json").is_file())

    def test_invalid_scopes_are_rejected_without_json(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            repo, _ = init_repo(Path(directory))

            invalid = invoke(repo, "--base", "does-not-exist")
            self.assertEqual(invalid.returncode, 3)
            self.assertEqual(invalid.stdout, b"")

            tree = git(repo, "write-tree").decode("ascii").strip()
            unrelated = git(repo, "commit-tree", tree, "-m", "unrelated").decode(
                "ascii"
            ).strip()
            nonancestor = invoke(repo, "--base", unrelated)
            self.assertEqual(nonancestor.returncode, 3)
            self.assertEqual(nonancestor.stdout, b"")
            self.assertIn(b"not an ancestor", nonancestor.stderr)

            both = invoke(repo, "--base", "HEAD", "--session", "S0100-20260830-001-x")
            self.assertEqual(both.returncode, 2)
            self.assertEqual(both.stdout, b"")

    def test_symlink_session_record_is_not_followed(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            repo, head = init_repo(root)
            session_id = "S0100-20260830-002-symlink-fixture"
            session_dir = repo / "agent" / "sessions" / "2026" / "08" / session_id
            session_dir.mkdir(parents=True)
            target = root / "outside-session.json"
            target.write_text(
                json.dumps(
                    {
                        "id": session_id,
                        "base_revision": head,
                        "final_revision": None,
                        "status": "in_progress",
                    }
                ),
                encoding="utf-8",
            )
            (session_dir / "session.json").symlink_to(target)

            result = invoke(repo, "--session", session_id)

            self.assertEqual(result.returncode, 3)
            self.assertEqual(result.stdout, b"")
            self.assertIn(b"one regular record", result.stderr)

    def test_fingerprint_detects_change_during_collection(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            repo, _ = init_repo(Path(directory))

            def mutate_during_scan(
                candidate: Path,
                _scope: object,
                _head: str,
            ) -> dict[str, object]:
                write(candidate / "tracked.txt", "changed during scan\n")
                return {
                    "committed": [],
                    "staged": [],
                    "unstaged": [],
                    "untracked": [],
                }

            with self.assertRaisesRegex(INVENTORY.DriftError, "changed while"):
                INVENTORY.collect_stable_inventory(repo, collector=mutate_during_scan)

    def test_tracked_layer_aba_transition_is_drift(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            repo, _ = init_repo(Path(directory))

            def observe_change_then_restore(
                candidate: Path,
                scope: object,
                head: str,
            ) -> dict[str, object]:
                write(candidate / "tracked.txt", "transient\n")
                observed = INVENTORY.collect_layers(candidate, scope, head)
                write(candidate / "tracked.txt", "initial\n")
                return observed

            with self.assertRaisesRegex(INVENTORY.DriftError, "change layers changed"):
                INVENTORY.collect_stable_inventory(
                    repo,
                    collector=observe_change_then_restore,
                )

    def test_session_scope_resolution_is_inside_stability_bracket(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            repo, head = init_repo(Path(directory))
            session_id = "S0100-20260830-003-scope-drift-fixture"
            write_session(
                repo,
                session_id,
                base=head,
                status="in_progress",
                final=None,
            )
            session_head = commit_all(repo, "track active session")

            def finish_session_during_resolution(
                candidate: Path,
                base_argument: str | None,
                requested_session: str | None,
            ) -> object:
                write_session(
                    candidate,
                    session_id,
                    base=head,
                    status="blocked",
                    final=session_head,
                )
                return INVENTORY.resolve_scope(
                    candidate,
                    base_argument,
                    requested_session,
                )

            with self.assertRaisesRegex(INVENTORY.DriftError, "changed while"):
                INVENTORY.collect_stable_inventory(
                    repo,
                    session_id=session_id,
                    resolver=finish_session_during_resolution,
                )

    def test_session_disappearance_at_closing_boundary_is_drift(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            repo, head = init_repo(Path(directory))
            session_id = "S0100-20260830-005-scope-disappears-fixture"
            session_dir = write_session(
                repo,
                session_id,
                base=head,
                status="in_progress",
                final=None,
            )

            def remove_session_during_collection(
                candidate: Path,
                scope: object,
                current_head: str,
            ) -> dict[str, object]:
                (session_dir / "session.json").unlink()
                return INVENTORY.collect_layers(candidate, scope, current_head)

            with self.assertRaisesRegex(INVENTORY.DriftError, "became invalid"):
                INVENTORY.collect_stable_inventory(
                    repo,
                    session_id=session_id,
                    collector=remove_session_during_collection,
                )

    def test_drift_error_maps_to_exit_five_without_json(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            repo, _ = init_repo(Path(directory))
            stdout = io.StringIO()
            stderr = io.StringIO()
            with mock.patch.object(
                INVENTORY,
                "collect_stable_inventory",
                side_effect=INVENTORY.DriftError("fixture drift"),
            ):
                with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
                    result = INVENTORY.main(["--repo-root", os.fspath(repo)])

            self.assertEqual(result, 5)
            self.assertEqual(stdout.getvalue(), "")
            self.assertIn("fixture drift", stderr.getvalue())

    def test_session_scope_aba_transition_is_drift(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            repo, head = init_repo(Path(directory))
            session_id = "S0100-20260830-004-scope-aba-fixture"
            write_session(
                repo,
                session_id,
                base=head,
                status="in_progress",
                final=None,
            )
            session_head = commit_all(repo, "track active session")

            def observe_terminal_then_restore(
                candidate: Path,
                base_argument: str | None,
                requested_session: str | None,
            ) -> object:
                write_session(
                    candidate,
                    session_id,
                    base=head,
                    status="blocked",
                    final=session_head,
                )
                observed = INVENTORY.resolve_scope(
                    candidate,
                    base_argument,
                    requested_session,
                )
                write_session(
                    candidate,
                    session_id,
                    base=head,
                    status="in_progress",
                    final=None,
                )
                return observed

            with self.assertRaisesRegex(INVENTORY.DriftError, "scope changed"):
                INVENTORY.collect_stable_inventory(
                    repo,
                    session_id=session_id,
                    resolver=observe_terminal_then_restore,
                )

    def test_opaque_untracked_repository_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            repo, _ = init_repo(Path(directory))
            nested = repo / "nested"
            nested.mkdir()
            git(nested, "init", "-q")
            write(nested / "payload.txt", "opaque\n")

            result = invoke(repo)

            self.assertEqual(result.returncode, 4)
            self.assertEqual(result.stdout, b"")
            self.assertIn(b"opaque untracked directory", result.stderr)

    def test_untracked_regular_file_replacement_is_not_followed(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            repo, _ = init_repo(root)
            untracked = repo / "untracked.txt"
            outside = root / "outside.txt"
            write(untracked, "inside\n")
            write(outside, "outside\n")
            original_open = os.open

            def replace_with_symlink(path: object, flags: int) -> int:
                untracked.unlink()
                untracked.symlink_to(outside)
                return original_open(path, flags)

            with mock.patch.object(INVENTORY.os, "open", side_effect=replace_with_symlink):
                with self.assertRaisesRegex(INVENTORY.DriftError, "changed during"):
                    INVENTORY.hash_untracked(repo)

    def test_replacement_refs_do_not_change_revision_meaning(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            repo, head = init_repo(Path(directory))
            blob = run(
                ["git", "hash-object", "-w", "--stdin"],
                cwd=repo,
                input_data=b"replacement\n",
            ).stdout.decode("ascii").strip()
            tree = run(
                ["git", "mktree"],
                cwd=repo,
                input_data=f"100644 blob {blob}\ttracked.txt\n".encode("ascii"),
            ).stdout.decode("ascii").strip()
            replacement = git(repo, "commit-tree", tree, "-m", "replacement").decode(
                "ascii"
            ).strip()
            git(repo, "replace", head, replacement)

            result = invoke(repo)

            self.assertEqual(result.returncode, 0, result.stderr.decode())
            document = json.loads(result.stdout)
            self.assertEqual(document["scope"]["end_revision"], head)
            self.assertEqual(
                document["changes"],
                {"committed": [], "staged": [], "unstaged": [], "untracked": []},
            )
            self.assertEqual(git(repo, "replace", "-l").decode("ascii").strip(), head)

    def test_configured_fsmonitor_hook_is_not_invoked(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            repo, _ = init_repo(root)
            marker = root / "fsmonitor-called"
            hook = root / "fsmonitor-hook.sh"
            write(
                hook,
                "#!/bin/sh\n"
                f': > "{marker}"\n'
                "printf '\\0'\n",
            )
            hook.chmod(0o755)
            git(repo, "config", "core.fsmonitor", os.fspath(hook))

            result = invoke(repo)

            self.assertEqual(result.returncode, 0, result.stderr.decode())
            self.assertFalse(marker.exists())


if __name__ == "__main__":
    unittest.main(verbosity=2)
