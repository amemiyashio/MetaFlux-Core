#!/usr/bin/env python3
"""Self-test the committed-SC gate for protected Agent history edits."""

from __future__ import annotations

import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parent.parent
GATE = ROOT / "tools" / "check-semantic-change-edits.py"
PRE_EDIT = ROOT / ".claude" / "hooks" / "pre_edit.py"
MIGRATION_SESSION = "S0100-20260830-997-semantic-migration"
TERMINAL_SESSION = "S0100-20260830-998-terminal-fixture"
CHECKPOINT = "agent/progress/checkpoints/2026/P20260830-001-fixture.md"
RENAMED_CHECKPOINT = "agent/progress/checkpoints/2026/P20260830-002-renamed.md"
TERMINAL_SUMMARY = f"agent/sessions/2026/08/{TERMINAL_SESSION}/summary.md"
TERMINAL_NOTES = f"agent/sessions/2026/08/{TERMINAL_SESSION}/notes.md"
LIQUIDATION_MANIFEST = "agent/sessions/liquidated-v1.json"


def run(root: Path, *arguments: str, input_text: str | None = None) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        list(arguments),
        cwd=root,
        check=False,
        capture_output=True,
        text=True,
        input=input_text,
    )


def write(root: Path, relative: str, content: str) -> None:
    path = root / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")


def session_document(session_id: str, status: str) -> str:
    return json.dumps(
        {
            "id": session_id,
            "status": status,
            "ended_at": None if status == "in_progress" else "2026-08-30",
        },
        indent=2,
    ) + "\n"


def sc_document(
    sc_id: str,
    *,
    status: str,
    session: str,
    surfaces: list[str],
    disposition: str = "Pending",
) -> str:
    rows = "\n".join(
        f"| `{surface}` | Historical | {disposition} | fixture authorization |"
        for surface in surfaces
    )
    effective_revision = "null" if status == "Active" else "a" * 40
    return (
        "---\n"
        f"id: {sc_id}\n"
        f"status: {status}\n"
        "created: 2026-08-30\n"
        "updated: 2026-08-30\n"
        "decision: D0001\n"
        f"session: {session}\n"
        "scope: fixture-migration\n"
        "history_sync: automatic\n"
        f"effective_revision: {effective_revision}\n"
        "superseded_by: null\n"
        "---\n\n"
        f"# {sc_id}\n\n"
        "## Semantic replacement\n\n"
        "Replace one fixture meaning under D0001.\n\n"
        "## Migration inventory\n\n"
        "| Surface | Class | Disposition | Evidence |\n"
        "| --- | --- | --- | --- |\n"
        f"{rows}\n\n"
        "## Active-session handoff\n\n"
        "| Session | Guidance | Status | Outcome |\n"
        "| --- | --- | --- | --- |\n"
        "| none | none | Not required | fixture has no other active session |\n\n"
        "## Evidence preservation\n\n"
        "Fixture facts remain unchanged.\n\n"
        "## Future-agent reminder\n\n"
        "Use the current fixture meaning.\n\n"
        "## Verification\n\n"
        "| Gate | Result |\n"
        "| --- | --- |\n"
        "| fixture | Pending |\n"
    )


def commit_all(root: Path, message: str) -> None:
    added = run(root, "git", "add", "-A")
    if added.returncode != 0:
        raise AssertionError(added.stderr)
    committed = run(root, "git", "commit", "-m", message)
    if committed.returncode != 0:
        raise AssertionError(committed.stderr)


def build_repo(
    root: Path,
    *,
    sc_status: str | None = None,
    sc_surfaces: list[str] | None = None,
    sc_disposition: str = "Pending",
    migration_status: str = "in_progress",
    duplicate: bool = False,
    liquidation_manifest: bool = False,
) -> None:
    initialized = run(root, "git", "init", "-q")
    if initialized.returncode != 0:
        raise AssertionError(initialized.stderr)
    run(root, "git", "config", "user.name", "MetaFlux Test")
    run(root, "git", "config", "user.email", "test@metaflux.invalid")
    write(root, CHECKPOINT, "recorded checkpoint\n")
    write(root, TERMINAL_SUMMARY, "terminal summary\n")
    write(root, TERMINAL_NOTES, "terminal notes\n")
    write(
        root,
        f"agent/sessions/2026/08/{TERMINAL_SESSION}/session.json",
        session_document(TERMINAL_SESSION, "complete"),
    )
    write(
        root,
        f"agent/sessions/2026/08/{MIGRATION_SESSION}/session.json",
        session_document(MIGRATION_SESSION, migration_status),
    )
    write(
        root,
        "agent/memory/decisions-index.md",
        "# Decisions\n\n"
        "| ID | Topic | Canonical source | Source status |\n"
        "| --- | --- | --- | --- |\n"
        "| D0001 | Fixture | fixture | Verified |\n",
    )
    write(
        root,
        "agent/semantic-changes/README.md",
        "# Semantic Changes\n\n"
        "## Index\n\n"
        "| ID | Status | Decision | Scope | Updated |\n"
        "| --- | --- | --- | --- | --- |\n",
    )
    if liquidation_manifest:
        write(root, LIQUIDATION_MANIFEST, '{"status": "liquidated"}\n')
    (root / "tools").mkdir(parents=True, exist_ok=True)
    shutil.copy2(GATE, root / "tools" / GATE.name)
    (root / ".claude" / "hooks").mkdir(parents=True, exist_ok=True)
    shutil.copy2(PRE_EDIT, root / ".claude" / "hooks" / PRE_EDIT.name)
    commit_all(root, "baseline")

    if sc_status is not None:
        surfaces = sc_surfaces or []
        index_rows = [
            f"| [SC0001](SC0001-fixture.md) | {sc_status} | D0001 | fixture-migration | 2026-08-30 |"
        ]
        write(
            root,
            "agent/semantic-changes/SC0001-fixture.md",
            sc_document(
                "SC0001",
                status=sc_status,
                session=MIGRATION_SESSION,
                surfaces=surfaces,
                disposition=sc_disposition,
            ),
        )
        if duplicate:
            write(
                root,
                "agent/semantic-changes/SC0002-duplicate.md",
                sc_document(
                    "SC0002",
                    status="Active",
                    session=MIGRATION_SESSION,
                    surfaces=surfaces,
                ),
            )
            index_rows.append(
                "| [SC0002](SC0002-duplicate.md) | Active | D0001 | fixture-migration | 2026-08-30 |"
            )
        write(
            root,
            "agent/semantic-changes/README.md",
            "# Semantic Changes\n\n"
            "## Index\n\n"
            "| ID | Status | Decision | Scope | Updated |\n"
            "| --- | --- | --- | --- | --- |\n"
            + "\n".join(index_rows)
            + "\n",
        )
        commit_all(root, "semantic authorization")


def cached_gate(root: Path) -> subprocess.CompletedProcess[str]:
    return run(root, sys.executable, str(GATE), str(root), "--cached")


def pre_edit(root: Path, relative: str) -> subprocess.CompletedProcess[str]:
    payload = json.dumps({"cwd": str(root), "tool_input": {"file_path": relative}})
    return run(
        root,
        sys.executable,
        str(root / ".claude" / "hooks" / "pre_edit.py"),
        input_text=payload,
    )


def stage_modified(root: Path, relative: str, content: str) -> None:
    write(root, relative, content)
    result = run(root, "git", "add", relative)
    if result.returncode != 0:
        raise AssertionError(result.stderr)


def commit_applied_sc(root: Path) -> str:
    revision = run(root, "git", "rev-parse", "HEAD").stdout.strip()
    sc_path = root / "agent/semantic-changes/SC0001-fixture.md"
    text = sc_path.read_text(encoding="utf-8")
    text = text.replace("status: Active", "status: Applied", 1)
    text = text.replace("effective_revision: null", f"effective_revision: {revision}", 1)
    text = text.replace("| Historical | Pending |", "| Historical | Migrated |")
    text = text.replace("| fixture | Pending |", "| fixture | Passed |")
    sc_path.write_text(text, encoding="utf-8")
    index_path = root / "agent/semantic-changes/README.md"
    index_path.write_text(
        index_path.read_text(encoding="utf-8").replace(
            "| Active | D0001 |", "| Applied | D0001 |", 1
        ),
        encoding="utf-8",
    )
    commit_all(root, "apply semantic change")
    return revision


def expect(name: str, result: subprocess.CompletedProcess[str], success: bool, needle: str = "") -> None:
    if (result.returncode == 0) != success:
        raise AssertionError(
            f"{name}: exit={result.returncode} stdout={result.stdout!r} stderr={result.stderr!r}"
        )
    if needle and needle not in result.stderr:
        raise AssertionError(f"{name}: expected {needle!r}, got {result.stderr!r}")


def main() -> int:
    passed = 0
    with tempfile.TemporaryDirectory(prefix="metaflux-semantic-edit-") as temporary:
        base = Path(temporary)

        root = base / "new-checkpoint"
        root.mkdir()
        build_repo(root)
        stage_modified(root, "agent/progress/checkpoints/2026/P20260830-009-new.md", "new\n")
        expect("new checkpoint", cached_gate(root), True)
        passed += 1

        root = base / "checkpoint-denied"
        root.mkdir()
        build_repo(root)
        stage_modified(root, CHECKPOINT, "changed\n")
        expect("checkpoint denied", cached_gate(root), False, "lacks a committed Active SC")
        passed += 1

        root = base / "terminal-denied"
        root.mkdir()
        build_repo(root)
        stage_modified(root, TERMINAL_SUMMARY, "changed\n")
        expect("terminal denied", cached_gate(root), False, "lacks a committed Active SC")
        passed += 1

        root = base / "liquidation-manifest-denied"
        root.mkdir()
        build_repo(root, liquidation_manifest=True)
        stage_modified(root, LIQUIDATION_MANIFEST, '{"status": "rewritten"}\n')
        expect(
            "liquidation manifest denied",
            cached_gate(root),
            False,
            "lacks a committed Active SC",
        )
        passed += 1

        root = base / "liquidation-manifest-authorized"
        root.mkdir()
        build_repo(
            root,
            sc_status="Active",
            sc_surfaces=[LIQUIDATION_MANIFEST],
            liquidation_manifest=True,
        )
        stage_modified(root, LIQUIDATION_MANIFEST, '{"status": "migrated"}\n')
        expect("liquidation manifest authorized", cached_gate(root), True)
        passed += 1

        root = base / "same-commit"
        root.mkdir()
        build_repo(root)
        stage_modified(root, CHECKPOINT, "changed\n")
        write(
            root,
            "agent/semantic-changes/SC0001-fixture.md",
            sc_document("SC0001", status="Active", session=MIGRATION_SESSION, surfaces=[CHECKPOINT]),
        )
        run(root, "git", "add", "agent/semantic-changes/SC0001-fixture.md")
        expect("same commit authorization", cached_gate(root), False, "lacks a committed Active SC")
        passed += 1

        root = base / "authorized"
        root.mkdir()
        build_repo(root, sc_status="Active", sc_surfaces=[CHECKPOINT, TERMINAL_SUMMARY])
        stage_modified(root, CHECKPOINT, "changed\n")
        stage_modified(root, TERMINAL_SUMMARY, "changed summary\n")
        expect("authorized exact paths", cached_gate(root), True)
        passed += 1

        root = base / "unlisted"
        root.mkdir()
        build_repo(root, sc_status="Active", sc_surfaces=[CHECKPOINT])
        stage_modified(root, TERMINAL_NOTES, "changed notes\n")
        expect("unlisted terminal path", cached_gate(root), False, TERMINAL_NOTES)
        passed += 1

        root = base / "applied"
        root.mkdir()
        build_repo(root, sc_status="Applied", sc_surfaces=[CHECKPOINT])
        stage_modified(root, CHECKPOINT, "changed\n")
        expect("Applied does not authorize", cached_gate(root), False, "lacks a committed Active SC")
        passed += 1

        root = base / "migrated-row"
        root.mkdir()
        build_repo(
            root,
            sc_status="Active",
            sc_surfaces=[CHECKPOINT],
            sc_disposition="Migrated",
        )
        stage_modified(root, CHECKPOINT, "changed\n")
        expect(
            "Migrated row closes authorization",
            cached_gate(root),
            False,
            "lacks a committed Active SC",
        )
        passed += 1

        root = base / "invalid-index"
        root.mkdir()
        build_repo(root, sc_status="Active", sc_surfaces=[CHECKPOINT])
        write(
            root,
            "agent/semantic-changes/README.md",
            "# Semantic Changes\n\n## Index\n\n"
            "| ID | Status | Decision | Scope | Updated |\n"
            "| --- | --- | --- | --- | --- |\n",
        )
        commit_all(root, "break semantic index")
        stage_modified(root, CHECKPOINT, "changed\n")
        expect(
            "invalid HEAD index",
            cached_gate(root),
            False,
            "index row does not match in HEAD",
        )
        passed += 1

        root = base / "new-applied"
        root.mkdir()
        build_repo(root)
        write(
            root,
            "agent/semantic-changes/SC0001-fixture.md",
            sc_document(
                "SC0001",
                status="Applied",
                session=MIGRATION_SESSION,
                surfaces=[CHECKPOINT],
                disposition="Migrated",
            ),
        )
        run(root, "git", "add", "agent/semantic-changes/SC0001-fixture.md")
        expect(
            "new SC must start Active",
            cached_gate(root),
            False,
            "must start as Active",
        )
        passed += 1

        root = base / "unresolved-effective-revision"
        root.mkdir()
        build_repo(root, sc_status="Active", sc_surfaces=[CHECKPOINT])
        sc_path = root / "agent/semantic-changes/SC0001-fixture.md"
        applied = sc_path.read_text(encoding="utf-8").replace(
            "status: Active", "status: Applied", 1
        ).replace("effective_revision: null", "effective_revision: " + "a" * 40, 1)
        sc_path.write_text(applied, encoding="utf-8")
        run(root, "git", "add", "agent/semantic-changes/SC0001-fixture.md")
        expect(
            "Applied revision resolves",
            cached_gate(root),
            False,
            "must be a full commit in the HEAD ancestry",
        )
        passed += 1

        root = base / "terminal-rewrite"
        root.mkdir()
        build_repo(root, sc_status="Active", sc_surfaces=[CHECKPOINT])
        commit_applied_sc(root)
        sc_path = root / "agent/semantic-changes/SC0001-fixture.md"
        sc_path.write_text(
            sc_path.read_text(encoding="utf-8").replace(
                "Use the current fixture meaning.",
                "Rewrite the terminal fixture meaning.",
            ),
            encoding="utf-8",
        )
        run(root, "git", "add", "agent/semantic-changes/SC0001-fixture.md")
        expect(
            "terminal SC rewrite",
            cached_gate(root),
            False,
            "terminal semantic-change records are immutable",
        )
        passed += 1

        root = base / "lifecycle-reversion"
        root.mkdir()
        build_repo(root, sc_status="Active", sc_surfaces=[CHECKPOINT])
        applied_revision = commit_applied_sc(root)
        sc_path = root / "agent/semantic-changes/SC0001-fixture.md"
        reverted = sc_path.read_text(encoding="utf-8").replace(
            "status: Applied", "status: Active", 1
        ).replace(
            f"effective_revision: {applied_revision}",
            "effective_revision: null",
            1,
        )
        sc_path.write_text(reverted, encoding="utf-8")
        run(root, "git", "add", "agent/semantic-changes/SC0001-fixture.md")
        expect(
            "semantic lifecycle reversion",
            cached_gate(root),
            False,
            "lifecycle cannot move Applied -> Active",
        )
        passed += 1

        root = base / "closed-owner"
        root.mkdir()
        build_repo(
            root,
            sc_status="Active",
            sc_surfaces=[CHECKPOINT],
            migration_status="complete",
        )
        stage_modified(root, CHECKPOINT, "changed\n")
        expect("closed owner", cached_gate(root), False, "not in_progress in HEAD")
        passed += 1

        root = base / "overlap"
        root.mkdir()
        build_repo(root, sc_status="Active", sc_surfaces=[CHECKPOINT], duplicate=True)
        stage_modified(root, CHECKPOINT, "changed\n")
        expect("overlap", cached_gate(root), False, "overlapping Active SC permits")
        passed += 1

        root = base / "type-change"
        root.mkdir()
        build_repo(root, sc_status="Active", sc_surfaces=[CHECKPOINT])
        checkpoint = root / CHECKPOINT
        checkpoint.unlink()
        checkpoint.symlink_to("replacement")
        run(root, "git", "add", CHECKPOINT)
        expect("type change", cached_gate(root), False, "cannot change file type")
        passed += 1

        root = base / "rename"
        root.mkdir()
        build_repo(root, sc_status="Active", sc_surfaces=[CHECKPOINT, RENAMED_CHECKPOINT])
        renamed = run(root, "git", "mv", CHECKPOINT, RENAMED_CHECKPOINT)
        if renamed.returncode != 0:
            raise AssertionError(renamed.stderr)
        expect("authorized rename", cached_gate(root), True)
        passed += 1

        root = base / "cross-owner-rename"
        root.mkdir()
        build_repo(root)
        write(
            root,
            "agent/semantic-changes/SC0001-source.md",
            sc_document(
                "SC0001",
                status="Active",
                session=MIGRATION_SESSION,
                surfaces=[CHECKPOINT],
            ),
        )
        write(
            root,
            "agent/semantic-changes/SC0002-target.md",
            sc_document(
                "SC0002",
                status="Active",
                session=MIGRATION_SESSION,
                surfaces=[RENAMED_CHECKPOINT],
            ),
        )
        write(
            root,
            "agent/semantic-changes/README.md",
            "# Semantic Changes\n\n## Index\n\n"
            "| ID | Status | Decision | Scope | Updated |\n"
            "| --- | --- | --- | --- | --- |\n"
            "| [SC0001](SC0001-source.md) | Active | D0001 | fixture-migration | 2026-08-30 |\n"
            "| [SC0002](SC0002-target.md) | Active | D0001 | fixture-migration | 2026-08-30 |\n",
        )
        commit_all(root, "split semantic authorization")
        renamed = run(root, "git", "mv", CHECKPOINT, RENAMED_CHECKPOINT)
        if renamed.returncode != 0:
            raise AssertionError(renamed.stderr)
        expect(
            "cross-owner rename",
            cached_gate(root),
            False,
            "must belong to the same Active SC",
        )
        passed += 1

        root = base / "pre-edit"
        root.mkdir()
        build_repo(root)
        expect(
            "pre-edit new checkpoint",
            pre_edit(root, "agent/progress/checkpoints/2026/P20260830-009-new.md"),
            True,
        )
        expect("pre-edit old checkpoint", pre_edit(root, CHECKPOINT), False, "committed Active SC")
        passed += 2

        root = base / "pre-edit-authorized"
        root.mkdir()
        build_repo(root, sc_status="Active", sc_surfaces=[CHECKPOINT])
        expect("pre-edit authorized", pre_edit(root, CHECKPOINT), True)
        passed += 1

    print(f"semantic-change edit self-test: {passed}/{passed} cases passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
