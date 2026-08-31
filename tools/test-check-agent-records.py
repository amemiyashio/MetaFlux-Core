#!/usr/bin/env python3
"""Self-test for tools/check-agent-records.py using synthetic golden trees.

Builds a minimal valid agent/ tree in a temporary directory and asserts the
validator's behavior on it, then mutates one aspect per case to pin every rule:
required session fields, delivery-scoped session ids and references, lifecycle timestamps,
terminal-note cleanup, contiguous event sequence numbers, guidance dispositions and milestone
mapping, transient-inbox isolation and cleanup, roast/session-only classification, index
completeness (both directions), milestone release/index consistency, M/W ownership,
session-to-plan resolution, Codex
skill-package compatibility and
discovery, open-decision identity, decision-index references, staleness warnings,
markdown link existence, checkpoint id/path agreement, current-progress freshness,
latest-session status consistency, execution-focus projection, the staged-guidance gate,
and exact focus-owner authorization at pre-commit.

Run from anywhere:

    python3 tools/test-check-agent-records.py

Uses only the standard library; the validator is loaded by path so the
hyphenated filename is not a problem.
"""

from __future__ import annotations

import importlib.util
import json
import os
import shlex
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

sys.dont_write_bytecode = True

TOOLS_DIR = Path(__file__).resolve().parent
VALIDATOR_PATH = TOOLS_DIR / "check-agent-records.py"
NEW_SESSION_PATH = TOOLS_DIR / "new-session.py"
PRE_COMMIT_PATH = TOOLS_DIR.parent / ".githooks/pre-commit"
CLAUDE_PRE_EDIT_PATH = TOOLS_DIR.parent / ".claude/hooks/pre_edit.py"

spec = importlib.util.spec_from_file_location("check_agent_records", VALIDATOR_PATH)
assert spec is not None and spec.loader is not None
check_agent_records = importlib.util.module_from_spec(spec)
spec.loader.exec_module(check_agent_records)
Validator = check_agent_records.Validator

SESSION_ID = "S0100-20260828-001-selftest"
SESSION_DIR = f"agent/sessions/2026/08/{SESSION_ID}"
SUMMARY_PATH = f"{SESSION_DIR}/summary.md"
SYMLINK_PREFIX = "SYMLINK->"
ROAST_BLOCK = (
    "## roast\n\n"
    "### light roasts\n\n"
    "- none.\n\n"
    "### medium roasts\n\n"
    "- none.\n\n"
    "### dark roasts\n\n"
    "- none.\n\n"
    "## session-only\n\n"
    "- none.\n"
)
POPULATED_ROAST_BLOCK = (
    "## roast\n\n"
    "### light roasts\n\n"
    "- normalized fixture -> agent/memory/project.md (fixture revision)\n\n"
    "### medium roasts\n\n"
    "- bounded fixture synthesis -> E0001 (Candidate; fixture evidence gap)\n\n"
    "### dark roasts\n\n"
    "- governed fixture replacement -> docs/architecture/fixture.md "
    "(fixture decision; authority: D0001, SC not required)\n\n"
    "## session-only\n\n"
    "- fixture resume detail - reason: needed only to resume this session\n"
)
ACTIVE_ROAST_BLOCK = (
    "## roast\n\n"
    "### light roasts\n\n"
    "- TODO.\n\n"
    "### medium roasts\n\n"
    "- TODO.\n\n"
    "### dark roasts\n\n"
    "- TODO.\n\n"
    "## session-only\n\n"
    "- TODO.\n"
)

BASE_FILES: dict[str, str] = {
    "agent/plan/M0100-fixture/plan.md": (
        "---\n"
        "id: M0100\n"
        "release: v0.1.0\n"
        "delivery: 0.1.0.0\n"
        "status: Active\n"
        "updated: 2026-08-28\n"
        "---\n"
        "# M0100\n\n"
        "## Decisions to Close\n\n"
        "1. One open decision.\n"
    ),
    "agent/plan/README.md": (
        "# Plans\n\n"
        "| Milestone | Release | Status |\n"
        "| --- | --- | --- |\n"
        "| [M0100](M0100-fixture/plan.md) | v0.1.0 | Active |\n"
    ),
    "agent/plan/M0100-fixture/work/W0101-fixture.md": (
        "---\n"
        "id: W0101\n"
        "milestone: M0100\n"
        "delivery: 0.1.0.1\n"
        "status: Active\n"
        "updated: 2026-08-28\n"
        "---\n"
        "# W0101\n"
    ),
    "agent/memory/open-decisions.md": (
        "# Open Decisions\n\n"
        "| Milestone | Decision | Blocks | Closure |\n"
        "| --- | --- | --- | --- |\n"
        "| M0100 | One open decision | fixture | fixture |\n"
    ),
    "agent/memory/decisions-index.md": (
        "# Decision Index\n\n"
        "| ID | Topic | Canonical source | Source status |\n"
        "| --- | --- | --- | --- |\n"
        "| D0001 | Fixture decision | "
        "[M0100](../plan/M0100-fixture/plan.md) | Active plan |\n"
    ),
    "agent/experience/README.md": "# Experience\n\nNo records yet.\n",
    "agent/memory/project.md": "# Project Memory\n\nFixture owner.\n",
    "docs/architecture/fixture.md": "# Fixture Architecture\n",
    "agent/semantic-changes/README.md": (
        "# Semantic Changes\n\n"
        "## Index\n\n"
        "| ID | Status | Decision | Scope | Updated |\n"
        "| --- | --- | --- | --- | --- |\n"
    ),
    "agent/sessions/README.md": (
        "# Sessions\n\n"
        "| Session | Date | Fidelity | Status | Summary |\n"
        "| --- | --- | --- | --- | --- |\n"
        f"| [{SESSION_ID}](2026/08/{SESSION_ID}/summary.md) | 2026-08-28 | Exact | Complete | fixture |\n"
    ),
    f"{SESSION_DIR}/session.json": (
        "{\n"
        '  "schema_version": 1,\n'
        f'  "id": "{SESSION_ID}",\n'
        '  "delivery": "0.1.0.0",\n'
        '  "repository": "MetaFlux-Core",\n'
        '  "started_at": "2026-08-28",\n'
        '  "ended_at": "2026-08-28",\n'
        '  "time_precision": "date",\n'
        '  "status": "complete",\n'
        '  "fidelity": "exact",\n'
        '  "agents": [{"id": "A001", "role": "fixture"}],\n'
        '  "milestones": [{"id": "M0100", "title": "Fixture", "status": "active", "event_seqs": [1]}],\n'
        '  "work_items": [{"id": "W0101", "milestone_id": "M0100", '
        '"title": "Fixture work", "status": "active"}],\n'
        '  "base_revision": null,\n'
        '  "final_revision": null,\n'
        '  "event_log": "events.jsonl",\n'
        '  "summary": "summary.md",\n'
        '  "notes": "notes.md"\n'
        "}\n"
    ),
    f"{SESSION_DIR}/events.jsonl": (
        '{"schema_version": 1, "seq": 1, "timestamp": "2026-08-28", '
        '"type": "objective", "actor": "A001", "content": "fixture objective"}\n'
    ),
    SUMMARY_PATH: "# Summary\n\nFixture.\n\n" + ROAST_BLOCK,
    f"{SESSION_DIR}/notes.md": "# Notes\n\nFixture decision D0001.\n",
    "agent/progress/current.md": (
        "---\n"
        "status: Active\n"
        "updated: 2026-08-28\n"
        "checkpoint: P20260828-001\n"
        "---\n\n# Current Progress\n\nFixture.\n"
    ),
    "agent/progress/checkpoints/2026/P20260828-001-fixture.md": (
        "---\n"
        "id: P20260828-001\n"
        "status: Recorded\n"
        "captured: 2026-08-28\n"
        "---\n\n# Fixture Checkpoint\n"
    ),
}


def write_fixture_entry(root: Path, relative_path: str, content: str) -> None:
    target = root / relative_path
    target.parent.mkdir(parents=True, exist_ok=True)
    if content.startswith(SYMLINK_PREFIX):
        target.symlink_to(content.removeprefix(SYMLINK_PREFIX), target_is_directory=True)
    else:
        target.write_text(content, encoding="utf-8")


def build_tree(root: Path) -> None:
    for relative_path, content in BASE_FILES.items():
        write_fixture_entry(root, relative_path, content)


def mutate(paths: dict[str, str]) -> dict[str, str]:
    files = dict(BASE_FILES)
    for relative_path, content in paths.items():
        if content is None:
            files.pop(relative_path, None)
        else:
            files[relative_path] = content
    return files


def write_tree(root: Path, files: dict[str, str]) -> None:
    for relative_path, content in files.items():
        write_fixture_entry(root, relative_path, content)


def run_validator(root: Path) -> tuple[int, list[str], list[str]]:
    validator = Validator(root)
    code = validator.run()
    return code, list(validator.errors), list(validator.warnings)


def replace(files: dict[str, str], path: str, old: str, new: str) -> dict[str, str]:
    mutated = dict(files)
    mutated[path] = mutated[path].replace(old, new)
    return mutated


def with_guidance_event(
    files: dict[str, str],
    *,
    event_type: str = "work_note",
    guidance_id: str | None = "G001",
    disposition: str | None = "adopted",
    deferred_to: str | None = None,
) -> dict[str, str]:
    mutated = dict(files)
    event_path = f"{SESSION_DIR}/events.jsonl"
    event = json.loads(mutated[event_path])
    event["type"] = event_type
    if guidance_id is not None:
        event["guidance_id"] = guidance_id
    if disposition is not None:
        event["disposition"] = disposition
    if deferred_to is not None:
        event["deferred_to"] = deferred_to
    mutated[event_path] = json.dumps(event, sort_keys=True) + "\n"
    return mutated


def with_duplicate_guidance_disposition(files: dict[str, str]) -> dict[str, str]:
    mutated = with_guidance_event(files)
    event_path = f"{SESSION_DIR}/events.jsonl"
    first = json.loads(mutated[event_path])
    second = dict(first)
    second["seq"] = 2
    second["content"] = "duplicate disposition"
    mutated[event_path] = (
        json.dumps(first, sort_keys=True) + "\n" + json.dumps(second, sort_keys=True) + "\n"
    )
    return mutated


def with_terminal_guidance_event(
    files: dict[str, str], *, mapped: bool
) -> dict[str, str]:
    mutated = dict(files)
    event_path = f"{SESSION_DIR}/events.jsonl"
    guidance_event = {
        "schema_version": 1,
        "seq": 2,
        "timestamp": "2026-08-28",
        "type": "work_note",
        "actor": "A001",
        "content": "fixture guidance disposition",
        "guidance_id": "G001",
        "disposition": "adopted",
    }
    mutated[event_path] += json.dumps(guidance_event, sort_keys=True) + "\n"
    if mapped:
        session_path = f"{SESSION_DIR}/session.json"
        session = json.loads(mutated[session_path])
        session["milestones"][0]["event_seqs"].append(2)
        mutated[session_path] = json.dumps(session, indent=2) + "\n"
    return mutated


def with_in_progress_session(files: dict[str, str]) -> dict[str, str]:
    mutated = dict(files)
    session_path = f"{SESSION_DIR}/session.json"
    session = json.loads(mutated[session_path])
    session["status"] = "in_progress"
    session["ended_at"] = None
    mutated[session_path] = json.dumps(session, indent=2) + "\n"
    return mutated


def with_product_execution_focus(files: dict[str, str]) -> dict[str, str]:
    mutated = with_in_progress_session(files)
    session_path = f"{SESSION_DIR}/session.json"
    session = json.loads(mutated[session_path])
    session["schema_version"] = 2
    session["governance_epoch"] = "D0029"
    mutated[session_path] = json.dumps(session, indent=2) + "\n"
    event_path = f"{SESSION_DIR}/events.jsonl"
    events = []
    for line in mutated[event_path].splitlines():
        event = json.loads(line)
        event["schema_version"] = 2
        events.append(json.dumps(event, sort_keys=True))
    mutated[event_path] = "\n".join(events) + "\n"
    milestone_path = "agent/plan/M0100-fixture/plan.md"
    work_path = "agent/plan/M0100-fixture/work/W0101-fixture.md"
    mutated[milestone_path] = mutated[milestone_path].replace(
        "status: Active\n", "status: Active\ndepends_on: []\n", 1
    )
    mutated[work_path] += "\n## Exit Gate\n\nPass the focused fixture.\n"
    mutated["docs/architecture/execution-focus-governance.md"] = (
        "# Execution Focus Governance\n\nD0029 establishes one exact owner.\n"
    )
    mutated["agent/memory/decisions-index.md"] += (
        "| D0029 | Execution focus governance | "
        "[focus](../../docs/architecture/execution-focus-governance.md) | Verified |\n"
    )
    mutated["agent/progress/focus.json"] = json.dumps(
        {
            "schema_version": 2,
            "governance_epoch": "D0029",
            "updated": "2026-08-28",
            "mode": "product",
            "owner_session": SESSION_ID,
            "authority": None,
            "target": {
                "milestone": "M0100",
                "work_item": "W0101",
                "exit_gate": f"{work_path}#exit-gate",
            },
            "resume_target": None,
        },
        indent=2,
    ) + "\n"
    mutated["agent/progress/current.md"] = (
        "---\n"
        "status: Active\n"
        "updated: 2026-08-28\n"
        "checkpoint: P20260828-001\n"
        "governance_epoch: D0029\n"
        "focus_mode: product\n"
        f"focus_owner: {SESSION_ID}\n"
        "milestone: M0100\n"
        "workstream: W0101\n"
        "---\n\n"
        "# Current Progress\n\n"
        "## Next Actions\n\n"
        "1. Pass the focused fixture Exit Gate.\n"
    )
    return mutated


def update_focus_document(
    files: dict[str, str], **updates: object
) -> dict[str, str]:
    mutated = dict(files)
    focus_path = "agent/progress/focus.json"
    focus = json.loads(mutated[focus_path])
    focus.update(updates)
    mutated[focus_path] = json.dumps(focus, indent=2) + "\n"
    return mutated


def update_session_document(
    files: dict[str, str], **updates: object
) -> dict[str, str]:
    mutated = dict(files)
    session_path = f"{SESSION_DIR}/session.json"
    session = json.loads(mutated[session_path])
    session.update(updates)
    mutated[session_path] = json.dumps(session, indent=2) + "\n"
    return mutated


def with_guidance_artifacts(files: dict[str, str], *filenames: str) -> dict[str, str]:
    mutated = dict(files)
    for filename in filenames:
        mutated[f"{SESSION_DIR}/guidance/{filename}"] = "fixture guidance\n"
    return mutated


def with_guidance_isolation_probe(files: dict[str, str]) -> dict[str, str]:
    mutated = dict(files)
    guidance_dir = f"{SESSION_DIR}/guidance"
    mutated[f"{guidance_dir}/G001-isolation.ready.md"] = (
        "---\n"
        "id: P99999999-999\n"
        "---\n\n"
        "# Transient guidance\n\n"
        "Unknown decision D9999 with [broken](./missing.md).\n\n"
        "password: transient-fixture-secret\n"
    )
    mutated[f"{guidance_dir}/events.jsonl"] = (
        '{"type":"decision","content":"unknown D9998"}\n'
    )
    mutated[f"{guidance_dir}/session.json"] = "{}\n"
    return mutated


def check_new_session_skeleton(root: Path) -> list[str]:
    write_tree(root, BASE_FILES)
    sessions_root = root / "agent/sessions"

    result = subprocess.run(
        [
            sys.executable,
            "-B",
            str(NEW_SESSION_PATH),
            "0.1.0.0",
            "lifecycle-fixture",
            "--repo-root",
            str(root),
        ],
        capture_output=True,
        text=True,
        check=False,
    )
    problems: list[str] = []
    if result.returncode != 0:
        problems.append(f"scaffolder exited {result.returncode}: {result.stderr.strip()}")
        return problems
    if "this scaffold does not claim execution focus" not in result.stdout:
        problems.append("scaffolder did not distinguish session creation from focus ownership")

    generated = list(sessions_root.glob("*/*/S*-lifecycle-fixture"))
    if len(generated) != 1:
        problems.append(f"expected one generated session, found {len(generated)}")
        return problems

    session_dir = generated[0]
    try:
        session = json.loads((session_dir / "session.json").read_text(encoding="utf-8"))
        summary = (session_dir / "summary.md").read_text(encoding="utf-8")
    except (OSError, json.JSONDecodeError) as exc:
        problems.append(f"generated skeleton is unreadable: {exc}")
        return problems

    if session.get("status") != "in_progress":
        problems.append("generated session status is not in_progress")
    if session.get("ended_at") is not None:
        problems.append("generated in-progress session has a non-null ended_at")
    if session.get("delivery") != "0.1.0.0":
        problems.append("generated session does not retain its delivery coordinate")
    if session.get("schema_version") != 2:
        problems.append("generated session does not use current schema_version 2")
    if session.get("governance_epoch") != "D0029":
        problems.append("generated session does not declare governance_epoch D0029")
    try:
        first_event = json.loads(
            (session_dir / "events.jsonl").read_text(encoding="utf-8").splitlines()[0]
        )
    except (OSError, json.JSONDecodeError, IndexError) as exc:
        problems.append(f"generated event is unreadable: {exc}")
    else:
        if first_event.get("schema_version") != 2:
            problems.append("generated event does not match session schema_version 2")
    if not summary.startswith("# Session Summary\n\n## Objective and outcome\n"):
        problems.append("generated summary does not use the current section shape")
    roast_skeleton = (
        "## roast\n\n"
        "### light roasts\n\n"
        "- TODO.\n\n"
        "### medium roasts\n\n"
        "- TODO.\n\n"
        "### dark roasts\n\n"
        "- TODO.\n\n"
        "## session-only\n\n"
        "- TODO.\n"
    )
    if roast_skeleton not in summary:
        problems.append("generated summary is missing the exact active roast contract")

    code, errors, warnings = run_validator(root)
    if code != 0 or errors or warnings:
        problems.append(
            f"generated skeleton failed validation: code={code} "
            f"errors={errors} warnings={warnings}"
        )

    shutil.rmtree(root)
    root.mkdir(parents=True)
    write_tree(root, with_session_scope(BASE_FILES, "12.2.1.0"))
    collision = subprocess.run(
        [
            sys.executable,
            "-B",
            str(NEW_SESSION_PATH),
            "1.2.2.10",
            "collision-fixture",
            "--repo-root",
            str(root),
        ],
        capture_output=True,
        text=True,
        check=False,
    )
    if collision.returncode == 0 or "ambiguous" not in collision.stderr:
        problems.append(
            "scaffolder accepted a compact delivery collision: "
            f"exit={collision.returncode} stderr={collision.stderr.strip()!r}"
        )
    return problems


def check_guidance_inbox_isolation(root: Path) -> list[str]:
    problems: list[str] = []

    def validate(files: dict[str, str]) -> Validator:
        if root.exists():
            shutil.rmtree(root)
        root.mkdir(parents=True)
        write_tree(root, files)
        validator = Validator(root)
        validator.run()
        return validator

    baseline = validate(BASE_FILES)
    if baseline.errors or baseline.warnings:
        problems.append(
            f"baseline fixture is invalid: errors={baseline.errors} warnings={baseline.warnings}"
        )
        return problems

    active = validate(
        with_guidance_isolation_probe(with_in_progress_session(BASE_FILES))
    )
    if active.errors or active.warnings:
        problems.append(
            "active guidance affected durable records: "
            f"errors={active.errors} warnings={active.warnings}"
        )
    if active.markdown_count != baseline.markdown_count:
        problems.append(
            "active guidance changed the durable Markdown count: "
            f"baseline={baseline.markdown_count} active={active.markdown_count}"
        )

    terminal = validate(with_guidance_isolation_probe(BASE_FILES))
    if not terminal.errors:
        problems.append("terminal guidance inbox did not fail validation")
    unexpected = [
        error
        for error in terminal.errors
        if "terminal sessions must have an empty guidance inbox" not in error
    ]
    if unexpected:
        problems.append(f"terminal guidance reached durable scanners: {unexpected}")

    return problems


def check_pre_commit_guidance_gate(root: Path) -> list[str]:
    problems: list[str] = []
    active_session_id = "S0100-20260828-001-selftest"

    def prepare_repository() -> dict[str, str]:
        if root.exists():
            shutil.rmtree(root)
        root.mkdir(parents=True)
        subprocess.run(
            ["git", "init", "-q"],
            cwd=root,
            check=True,
            capture_output=True,
            text=True,
        )
        fake_bin = root / "fake-bin"
        fake_bin.mkdir()
        fake_python = fake_bin / "python3"
        fake_python.write_text(
            "#!/bin/sh\n"
            f"if [ \"$1\" = \"-c\" ]; then exec {shlex.quote(sys.executable)} \"$@\"; fi\n"
            "exit 0\n",
            encoding="utf-8",
        )
        fake_python.chmod(0o755)
        environment = os.environ.copy()
        environment["PATH"] = f"{fake_bin}{os.pathsep}{environment.get('PATH', '')}"
        environment["METAFLUX_SESSION_ID"] = active_session_id
        return environment

    guidance_path = Path(
        "agent/sessions/2026/08/S0100-20260828-001-selftest/guidance/G001-fixture.ready.md"
    )
    active_session_path = Path(
        f"agent/sessions/2026/08/{active_session_id}/session.json"
    )
    focus_path = Path("agent/progress/focus.json")

    def stage_active_session() -> None:
        write_fixture_entry(
            root,
            active_session_path.as_posix(),
            '{"status":"in_progress"}\n',
        )
        subprocess.run(
            ["git", "add", "--", active_session_path.as_posix()],
            cwd=root,
            check=True,
            capture_output=True,
            text=True,
        )
        write_fixture_entry(
            root,
            focus_path.as_posix(),
            json.dumps({"owner_session": active_session_id}) + "\n",
        )
        subprocess.run(
            ["git", "add", "--", focus_path.as_posix()],
            cwd=root,
            check=True,
            capture_output=True,
            text=True,
        )

    def prepare_tracked_path(tracked_path: Path) -> dict[str, str]:
        environment = prepare_repository()
        write_fixture_entry(root, tracked_path.as_posix(), "tracked fixture\n")
        subprocess.run(
            ["git", "add", "--", tracked_path.as_posix()],
            cwd=root,
            check=True,
            capture_output=True,
            text=True,
        )
        subprocess.run(
            [
                "git",
                "-c",
                "user.name=MetaFlux Self-Test",
                "-c",
                "user.email=selftest@invalid",
                "commit",
                "--no-verify",
                "-qm",
                "fixture",
            ],
            cwd=root,
            check=True,
            capture_output=True,
            text=True,
        )
        return environment

    def prepare_tracked_guidance() -> dict[str, str]:
        return prepare_tracked_path(guidance_path)

    environment = prepare_repository()
    write_fixture_entry(root, guidance_path.as_posix(), "fixture guidance\n")
    subprocess.run(
        ["git", "add", "--", guidance_path.as_posix()],
        cwd=root,
        check=True,
        capture_output=True,
        text=True,
    )
    result = subprocess.run(
        [str(PRE_COMMIT_PATH)],
        cwd=root,
        env=environment,
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode == 0 or "guidance" not in result.stderr:
        problems.append(
            "staged guidance was not rejected: "
            f"exit={result.returncode} stderr={result.stderr.strip()!r}"
        )

    environment = prepare_repository()
    write_fixture_entry(root, "agent/README.md", "# Agent fixture\n")
    write_fixture_entry(root, guidance_path.as_posix(), "untracked guidance\n")
    stage_active_session()
    subprocess.run(
        ["git", "add", "--", "agent/README.md"],
        cwd=root,
        check=True,
        capture_output=True,
        text=True,
    )
    result = subprocess.run(
        [str(PRE_COMMIT_PATH)],
        cwd=root,
        env=environment,
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode != 0:
        problems.append(
            "untracked guidance blocked an unrelated Agent commit: "
            f"exit={result.returncode} stderr={result.stderr.strip()!r}"
        )

    renamed_out_path = Path("agent/renamed-guidance.md")
    environment = prepare_tracked_guidance()
    subprocess.run(
        ["git", "mv", "--", guidance_path.as_posix(), renamed_out_path.as_posix()],
        cwd=root,
        check=True,
        capture_output=True,
        text=True,
    )
    rename_out = subprocess.run(
        ["git", "diff", "--cached", "--name-status", "--find-renames", "--diff-filter=R"],
        cwd=root,
        check=True,
        capture_output=True,
        text=True,
    )
    if not rename_out.stdout.startswith("R"):
        problems.append(f"guidance rename-out fixture was not status R: {rename_out.stdout!r}")
    result = subprocess.run(
        [str(PRE_COMMIT_PATH)],
        cwd=root,
        env=environment,
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode == 0 or guidance_path.as_posix() not in result.stderr:
        problems.append(
            "staged guidance rename-out was not rejected by its source path: "
            f"exit={result.returncode} stderr={result.stderr.strip()!r}"
        )

    renamed_in_path = Path("agent/incoming-guidance.md")
    environment = prepare_tracked_path(renamed_in_path)
    (root / guidance_path).parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(
        ["git", "mv", "--", renamed_in_path.as_posix(), guidance_path.as_posix()],
        cwd=root,
        check=True,
        capture_output=True,
        text=True,
    )
    rename_in = subprocess.run(
        ["git", "diff", "--cached", "--name-status", "--find-renames", "--diff-filter=R"],
        cwd=root,
        check=True,
        capture_output=True,
        text=True,
    )
    if not rename_in.stdout.startswith("R"):
        problems.append(f"guidance rename-in fixture was not status R: {rename_in.stdout!r}")
    result = subprocess.run(
        [str(PRE_COMMIT_PATH)],
        cwd=root,
        env=environment,
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode == 0 or guidance_path.as_posix() not in result.stderr:
        problems.append(
            "staged guidance rename-in was not rejected by its destination path: "
            f"exit={result.returncode} stderr={result.stderr.strip()!r}"
        )

    environment = prepare_tracked_guidance()
    tracked_guidance = root / guidance_path
    tracked_guidance.unlink()
    tracked_guidance.symlink_to("missing-guidance-target")
    subprocess.run(
        ["git", "add", "--", guidance_path.as_posix()],
        cwd=root,
        check=True,
        capture_output=True,
        text=True,
    )
    type_change = subprocess.run(
        ["git", "diff", "--cached", "--name-status", "--diff-filter=T"],
        cwd=root,
        check=True,
        capture_output=True,
        text=True,
    )
    if not type_change.stdout.startswith("T\t"):
        problems.append(f"guidance type-change fixture was not status T: {type_change.stdout!r}")
    result = subprocess.run(
        [str(PRE_COMMIT_PATH)],
        cwd=root,
        env=environment,
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode == 0 or "guidance" not in result.stderr:
        problems.append(
            "staged guidance type change was not rejected: "
            f"exit={result.returncode} stderr={result.stderr.strip()!r}"
        )

    environment = prepare_tracked_guidance()
    (root / guidance_path).unlink()
    stage_active_session()
    subprocess.run(
        ["git", "add", "-u", "--", guidance_path.as_posix()],
        cwd=root,
        check=True,
        capture_output=True,
        text=True,
    )
    result = subprocess.run(
        [str(PRE_COMMIT_PATH)],
        cwd=root,
        env=environment,
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode != 0:
        problems.append(
            "guidance deletion was not stageable for cleanup: "
            f"exit={result.returncode} stderr={result.stderr.strip()!r}"
        )

    return problems


def check_pre_commit_session_gate(root: Path) -> list[str]:
    problems: list[str] = []
    session_id = "S0100-20260828-001-selftest"
    successor_id = "S0100-20260828-002-successor"
    other_id = "S0100-20260828-003-other"
    session_path = Path(
        f"agent/sessions/2026/08/{session_id}/session.json"
    )
    successor_path = Path(f"agent/sessions/2026/08/{successor_id}/session.json")
    other_path = Path(f"agent/sessions/2026/08/{other_id}/session.json")
    focus_path = Path("agent/progress/focus.json")
    agent_path = Path("agent/README.md")
    product_path = Path("src/fixture.txt")
    active_session = (
        '{"schema_version":2,"governance_epoch":"D0029",'
        '"status":"in_progress"}\n'
    )
    complete_session = (
        '{"schema_version":2,"governance_epoch":"D0029",'
        '"status":"complete"}\n'
    )
    legacy_active_session = (
        '{"schema_version":1,"governance_epoch":"D0029",'
        '"status":"in_progress"}\n'
    )

    def focus_document(owner: str) -> str:
        return json.dumps(
            {
                "schema_version": 2,
                "governance_epoch": "D0029",
                "owner_session": owner,
            }
        ) + "\n"

    def cutover_focus_document(schema_version: int, *, updated: str = "2026-08-28") -> str:
        document: dict[str, object] = {
            "schema_version": schema_version,
            "updated": updated,
            "mode": "governance",
            "owner_session": session_id,
            "authority": {"decision": "D0029", "semantic_change": "SC0007"},
            "target": None,
            "resume_target": {"milestone": "M0110", "work_item": "W0112"},
        }
        if schema_version == 2:
            document["governance_epoch"] = "D0029"
        return json.dumps(document) + "\n"

    def prepare_repository(*, bypass_python_gates: bool = True) -> dict[str, str]:
        if root.exists():
            shutil.rmtree(root)
        root.mkdir(parents=True)
        subprocess.run(
            ["git", "init", "-q"],
            cwd=root,
            check=True,
            capture_output=True,
            text=True,
        )
        environment = os.environ.copy()
        environment["METAFLUX_SESSION_ID"] = session_id
        if bypass_python_gates:
            fake_bin = root / "fake-bin"
            fake_bin.mkdir()
            fake_python = fake_bin / "python3"
            fake_python.write_text(
                "#!/bin/sh\n"
                f"if [ \"$1\" = \"-c\" ]; then exec {shlex.quote(sys.executable)} \"$@\"; fi\n"
                "exit 0\n",
                encoding="utf-8",
            )
            fake_python.chmod(0o755)
            environment["PATH"] = (
                f"{fake_bin}{os.pathsep}{environment.get('PATH', '')}"
            )
        return environment

    def stage(path: Path, content: str) -> None:
        write_fixture_entry(root, path.as_posix(), content)
        subprocess.run(
            ["git", "add", "--", path.as_posix()],
            cwd=root,
            check=True,
            capture_output=True,
            text=True,
        )

    def run_hook(environment: dict[str, str]) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [str(PRE_COMMIT_PATH)],
            cwd=root,
            env=environment,
            capture_output=True,
            text=True,
            check=False,
        )

    def commit_staged(message: str) -> None:
        subprocess.run(
            [
                "git",
                "-c",
                "user.name=MetaFlux Self-Test",
                "-c",
                "user.email=selftest@invalid",
                "commit",
                "--no-verify",
                "-qm",
                message,
            ],
            cwd=root,
            check=True,
            capture_output=True,
            text=True,
        )

    def prepare_active_head() -> dict[str, str]:
        environment = prepare_repository()
        stage(session_path, active_session)
        stage(focus_path, focus_document(session_id))
        commit_staged("active session fixture")
        return environment

    def stage_focus_handoff() -> None:
        stage(session_path, complete_session)
        stage(successor_path, active_session)
        stage(focus_path, focus_document(successor_id))

    environment = prepare_repository()
    stage(agent_path, "# Agent fixture\n")
    result = run_hook(environment)
    if result.returncode == 0 or "focus.json" not in result.stderr:
        problems.append(
            "durable change without an execution focus was not rejected: "
            f"exit={result.returncode} stderr={result.stderr.strip()!r}"
        )

    environment = prepare_repository()
    stage(product_path, "product fixture\n")
    stage(focus_path, focus_document(session_id))
    write_fixture_entry(root, session_path.as_posix(), active_session)
    result = run_hook(environment)
    if result.returncode == 0 or "execution-focus owner" not in result.stderr:
        problems.append(
            "unstaged focus owner incorrectly covered a product commit: "
            f"exit={result.returncode} stderr={result.stderr.strip()!r}"
        )

    environment = prepare_repository()
    stage(session_path, active_session)
    stage(focus_path, focus_document(session_id))
    result = run_hook(environment)
    if result.returncode != 0:
        problems.append(
            "first staged active-session scaffold was rejected: "
            f"exit={result.returncode} stderr={result.stderr.strip()!r}"
        )

    environment = prepare_active_head()
    environment.pop("METAFLUX_SESSION_ID", None)
    stage(product_path, "product fixture\n")
    result = run_hook(environment)
    if result.returncode == 0 or "METAFLUX_SESSION_ID" not in result.stderr:
        problems.append(
            "product commit without an explicit session declaration was not rejected: "
            f"exit={result.returncode} stderr={result.stderr.strip()!r}"
        )

    environment = prepare_active_head()
    stage(product_path, "product fixture\n")
    result = run_hook(environment)
    if result.returncode != 0:
        problems.append(
            "product commit by the exact focus owner was rejected: "
            f"exit={result.returncode} stderr={result.stderr.strip()!r}"
        )

    environment = prepare_repository()
    stage(session_path, legacy_active_session)
    stage(focus_path, cutover_focus_document(1))
    commit_staged("legacy SC0007 governance owner fixture")
    stage(session_path, active_session)
    stage(focus_path, cutover_focus_document(2))
    stage(product_path, "atomic D0029 cutover fixture\n")
    result = run_hook(environment)
    if result.returncode != 0:
        problems.append(
            "exact atomic D0029 cutover was rejected: "
            f"exit={result.returncode} stderr={result.stderr.strip()!r}"
        )

    environment = prepare_repository()
    stage(session_path, legacy_active_session)
    stage(focus_path, cutover_focus_document(1))
    commit_staged("legacy SC0007 drift fixture")
    stage(session_path, active_session)
    stage(focus_path, cutover_focus_document(2, updated="2026-08-29"))
    stage(product_path, "drifting D0029 cutover fixture\n")
    result = run_hook(environment)
    if result.returncode == 0 or "record-only" not in result.stderr:
        problems.append(
            "D0029 cutover accepted unrelated focus drift: "
            f"exit={result.returncode} stderr={result.stderr.strip()!r}"
        )

    environment = prepare_active_head()
    stage(other_path, active_session)
    commit_staged("unrelated active session fixture")
    stage(product_path, "product fixture\n")
    environment["METAFLUX_SESSION_ID"] = other_id
    result = run_hook(environment)
    if result.returncode == 0 or "does not own" not in result.stderr:
        problems.append(
            "unrelated active session incorrectly covered a product commit: "
            f"exit={result.returncode} stderr={result.stderr.strip()!r}"
        )

    environment = prepare_active_head()
    stage(other_path, active_session)
    commit_staged("non-owner active session fixture")
    stage(other_path, complete_session)
    stage(Path("agent/sessions/README.md"), "# Non-owner close fixture\n")
    environment["METAFLUX_SESSION_ID"] = other_id
    result = run_hook(environment)
    if result.returncode != 0:
        problems.append(
            "exact record-only non-owner close was rejected: "
            f"exit={result.returncode} stderr={result.stderr.strip()!r}"
        )

    environment = prepare_active_head()
    stage_focus_handoff()
    stage(Path("agent/sessions/README.md"), "# Closing record fixture\n")
    stage(
        Path(
            "agent/sessions/2026/08/S0100-20260828-001-selftest/outputs/0001.txt"
        ),
        "bounded output fixture\n",
    )
    result = run_hook(environment)
    if result.returncode != 0:
        problems.append(
            "record-only final session close was rejected: "
            f"exit={result.returncode} stderr={result.stderr.strip()!r}"
        )

    environment = prepare_active_head()
    stage(successor_path, active_session)
    stage(focus_path, focus_document(successor_id))
    stage(Path("agent/sessions/README.md"), "# Incomplete handoff fixture\n")
    result = run_hook(environment)
    if result.returncode == 0 or "terminally close" not in result.stderr:
        problems.append(
            "focus handoff without terminally closing the old owner was not rejected: "
            f"exit={result.returncode} stderr={result.stderr.strip()!r}"
        )

    environment = prepare_active_head()
    stage_focus_handoff()
    stage(product_path, "product fixture\n")
    result = run_hook(environment)
    if result.returncode == 0 or "record-only" not in result.stderr:
        problems.append(
            "product content piggybacked on a final session close: "
            f"exit={result.returncode} stderr={result.stderr.strip()!r}"
        )

    environment = prepare_active_head()
    stage_focus_handoff()
    stage(Path("agent/skills/fixture/SKILL.md"), "# Skill fixture\n")
    result = run_hook(environment)
    if result.returncode == 0 or "record-only" not in result.stderr:
        problems.append(
            "agent skill content piggybacked on a final session close: "
            f"exit={result.returncode} stderr={result.stderr.strip()!r}"
        )

    environment = prepare_active_head()
    stage_focus_handoff()
    stage(
        Path("agent/sessions/2026/08/S0100-20260828-001-selftest/extra.py"),
        "raise SystemExit(0)\n",
    )
    result = run_hook(environment)
    if result.returncode == 0 or "record-only" not in result.stderr:
        problems.append(
            "unknown session descendant piggybacked on a final session close: "
            f"exit={result.returncode} stderr={result.stderr.strip()!r}"
        )

    environment = prepare_active_head()
    stage_focus_handoff()
    stage(Path("agent/progress/checkpoints/2026/extra.bin"), "fixture\n")
    result = run_hook(environment)
    if result.returncode == 0 or "record-only" not in result.stderr:
        problems.append(
            "noncanonical checkpoint descendant piggybacked on a final close: "
            f"exit={result.returncode} stderr={result.stderr.strip()!r}"
        )

    environment = prepare_repository()
    stage(
        session_path,
        '{"schema_version":2,"governance_epoch":"D0029",'
        '"status":"complete","milestones":[{"status":"in_progress"}]}\n',
    )
    stage(focus_path, focus_document(session_id))
    result = run_hook(environment)
    if result.returncode == 0 or "not one in-progress session" not in result.stderr:
        problems.append(
            "nested in-progress status incorrectly covered a newly added terminal session: "
            f"exit={result.returncode} stderr={result.stderr.strip()!r}"
        )

    environment = prepare_repository(bypass_python_gates=False)
    strong_validator = "# STRONG\nraise SystemExit(0)\n"
    weak_validator = "# WEAK\nraise SystemExit(0)\n"
    candidate_probe = (
        "from pathlib import Path\n"
        "validator = Path(__file__).with_name('check-agent-records.py')\n"
        "raise SystemExit(0 if '# STRONG' in validator.read_text() else 1)\n"
    )
    stage(Path("tools/check-semantic-change-edits.py"), "raise SystemExit(0)\n")
    stage(Path("tools/check-agent-records.py"), strong_validator)
    stage(Path("tools/test-check-agent-records.py"), candidate_probe)
    stage(session_path, active_session)
    stage(focus_path, focus_document(session_id))
    subprocess.run(
        [
            "git",
            "-c",
            "user.name=MetaFlux Self-Test",
            "-c",
            "user.email=selftest@invalid",
            "commit",
            "--no-verify",
            "-qm",
            "candidate gate fixture",
        ],
        cwd=root,
        check=True,
        capture_output=True,
        text=True,
    )
    stage(Path("tools/check-agent-records.py"), weak_validator)
    write_fixture_entry(root, "tools/check-agent-records.py", strong_validator)
    result = run_hook(environment)
    if result.returncode == 0:
        problems.append(
            "partially staged validator executed the stronger working-tree code"
        )

    return problems


def check_claude_focus_guard(root: Path) -> list[str]:
    problems: list[str] = []
    owner = "S0100-20260828-001-selftest"
    session_path = f"agent/sessions/2026/08/{owner}/session.json"

    def reset() -> None:
        if root.exists():
            shutil.rmtree(root)
        root.mkdir(parents=True)

    def write_owner(*, status: str = "in_progress", ended_at: str | None = None) -> None:
        write_fixture_entry(
            root,
            session_path,
            json.dumps(
                {
                    "schema_version": 2,
                    "governance_epoch": "D0029",
                    "id": owner,
                    "status": status,
                    "ended_at": ended_at,
                },
                indent=2,
            )
            + "\n",
        )

    def write_focus() -> None:
        write_fixture_entry(
            root,
            "agent/progress/focus.json",
            json.dumps(
                {
                    "schema_version": 2,
                    "governance_epoch": "D0029",
                    "owner_session": owner,
                },
                indent=2,
            )
            + "\n",
        )

    def run_guard(
        relative_path: str, *, declared_session: str | None = None
    ) -> subprocess.CompletedProcess[str]:
        environment = os.environ.copy()
        environment.pop("METAFLUX_SESSION_ID", None)
        if declared_session is not None:
            environment["METAFLUX_SESSION_ID"] = declared_session
        payload = {
            "cwd": str(root),
            "tool_input": {"file_path": str(root / relative_path)},
        }
        return subprocess.run(
            [sys.executable, "-B", str(CLAUDE_PRE_EDIT_PATH)],
            input=json.dumps(payload),
            cwd=root,
            env=environment,
            capture_output=True,
            text=True,
            check=False,
        )

    reset()
    write_owner()
    result = run_guard("src/fixture.cpp")
    if result.returncode != 2 or "focus.json" not in result.stderr:
        problems.append(
            "an unrelated active session covered a Claude product edit without focus: "
            f"exit={result.returncode} stderr={result.stderr.strip()!r}"
        )

    reset()
    write_owner()
    write_focus()
    result = run_guard("src/fixture.cpp")
    if result.returncode != 0:
        problems.append(
            "the valid focus owner did not cover a Claude product edit: "
            f"exit={result.returncode} stderr={result.stderr.strip()!r}"
        )

    result = run_guard(
        "src/fixture.cpp",
        declared_session="S0100-20260828-002-unrelated",
    )
    if result.returncode != 2 or "does not match" not in result.stderr:
        problems.append(
            "an explicit mismatched Claude session was not rejected: "
            f"exit={result.returncode} stderr={result.stderr.strip()!r}"
        )

    reset()
    write_owner(status="complete", ended_at="2026-08-28")
    write_focus()
    result = run_guard("src/fixture.cpp")
    if result.returncode != 2 or "focus.json" not in result.stderr:
        problems.append(
            "a terminal focus owner covered a Claude product edit: "
            f"exit={result.returncode} stderr={result.stderr.strip()!r}"
        )

    reset()
    result = run_guard("agent/sessions/fixture/session.json")
    if result.returncode != 0:
        problems.append(
            "the Claude guard blocked agent scaffolding before focus existed: "
            f"exit={result.returncode} stderr={result.stderr.strip()!r}"
        )

    return problems


def check_cached_tree_validation(root: Path) -> list[str]:
    """Prove --cached validates the index rather than a cleaner worktree."""

    problems: list[str] = []
    if root.exists():
        shutil.rmtree(root)
    root.mkdir(parents=True)
    write_tree(root, BASE_FILES)
    subprocess.run(
        ["git", "init", "-q"], cwd=root, check=True, capture_output=True, text=True
    )
    subprocess.run(
        ["git", "add", "-A"], cwd=root, check=True, capture_output=True, text=True
    )
    subprocess.run(
        [
            "git",
            "-c",
            "user.name=MetaFlux Self-Test",
            "-c",
            "user.email=selftest@invalid",
            "commit",
            "--no-verify",
            "-qm",
            "baseline",
        ],
        cwd=root,
        check=True,
        capture_output=True,
        text=True,
    )

    index_path = root / "agent/semantic-changes/README.md"
    valid_index = index_path.read_text(encoding="utf-8")
    index_path.write_text(
        valid_index
        + "| [SC0001](SC0001-missing.md) | Active | D0001 | fixture | 2026-08-28 |\n",
        encoding="utf-8",
    )
    subprocess.run(
        ["git", "add", "--", "agent/semantic-changes/README.md"],
        cwd=root,
        check=True,
        capture_output=True,
        text=True,
    )
    index_path.write_text(valid_index, encoding="utf-8")

    ordinary = subprocess.run(
        [sys.executable, "-B", str(VALIDATOR_PATH), str(root)],
        check=False,
        capture_output=True,
        text=True,
    )
    if ordinary.returncode != 0:
        problems.append(
            "working-tree control unexpectedly failed: " + ordinary.stderr.strip()
        )
    cached = subprocess.run(
        [sys.executable, "-B", str(VALIDATOR_PATH), str(root), "--cached"],
        check=False,
        capture_output=True,
        text=True,
    )
    if cached.returncode == 0 or "SC0001" not in cached.stderr:
        problems.append(
            "staged invalid semantic-change index was not rejected: "
            f"exit={cached.returncode} stderr={cached.stderr.strip()!r}"
        )
    return problems


SKILLS_README = (
    "# Skills\n\n## Index\n\n"
    "| Skill | Status | Use when |\n"
    "| --- | --- | --- |\n"
    "| [fixture-skill](fixture-skill/SKILL.md) | Active | Testing fixture skills |\n"
)
SKILL_FILE = (
    "---\n"
    "name: fixture-skill\n"
    "description: fixture skill\n"
    "---\n\n"
    "# Fixture Skill\n\nSteps.\n"
)
EXTENDED_SKILL_FILE = (
    "---\n"
    "name: fixture-skill\n"
    "description: >-\n"
    "  Fixture skill using standard optional frontmatter.\n"
    "license: Apache-2.0\n"
    "allowed-tools:\n"
    "  - shell\n"
    "metadata:\n"
    "  owner: fixture\n"
    "---\n\n"
    "# Fixture Skill\n\nSteps.\n"
)
OPENAI_YAML = (
    "interface:\n"
    '  display_name: "Fixture Skill"\n'
    '  short_description: "Review fixture skill package behavior"\n'
    '  default_prompt: "Use $fixture-skill to review this fixture skill package."\n'
)
DOMAIN_SKILL_SLUG = "runtime-contracts-registry"
DOMAIN_SKILLS_README = (
    "# Skills\n\n## Index\n\n"
    "| Skill | Status | Use when |\n"
    "| --- | --- | --- |\n"
    "| [runtime-contracts-registry](runtime-contracts-registry/SKILL.md) | "
    "Active | Reviewing runtime contract ownership |\n"
)
DOMAIN_SKILL_FILE = (
    "---\n"
    "name: runtime-contracts-registry\n"
    "description: Review runtime contract and registry behavior.\n"
    "---\n\n"
    "# Runtime Contracts Registry\n\n"
    "## Inputs\n\nFixture inputs.\n\n"
    "## Routing\n\nFixture routing.\n\n"
    "## Workflow\n\nFixture workflow.\n\n"
    "## Output\n\nFixture output.\n\n"
    "## Verification\n\nFixture verification.\n"
)
DOMAIN_OPENAI_YAML = (
    "interface:\n"
    '  display_name: "Runtime Contracts Registry"\n'
    '  short_description: "Review runtime contract registry boundaries"\n'
    '  default_prompt: "Use $runtime-contracts-registry to review this runtime contract fixture."\n'
)
WORKFLOW_SKILL_SLUG = "roast"
WORKFLOW_SKILLS_README = (
    "# Skills\n\n## Index\n\n"
    "| Skill | Status | Use when |\n"
    "| --- | --- | --- |\n"
    "| [roast](roast/SKILL.md) | Active | Classifying durable project knowledge |\n"
)
WORKFLOW_SKILL_FILE = (
    "---\n"
    "name: roast\n"
    "description: Classify promoted project knowledge by semantic transformation depth.\n"
    "---\n\n"
    "# Roast\n\nClassify and route material claims.\n"
)
WORKFLOW_OPENAI_YAML = (
    "interface:\n"
    '  display_name: "Roast Project Knowledge"\n'
    '  short_description: "Classify durable knowledge by transformation depth"\n'
    '  default_prompt: "Use $roast to classify and route these durable project claims."\n'
    "policy:\n"
    "  allow_implicit_invocation: false\n"
)
CONVERGENCE_SKILL_SLUG = "converge-project-changes"
CONVERGENCE_SKILLS_README = (
    "# Skills\n\n## Index\n\n"
    "| Skill | Status | Use when |\n"
    "| --- | --- | --- |\n"
    "| [converge-project-changes](converge-project-changes/SKILL.md) | "
    "Active | Converging collaborator changes |\n"
)
CONVERGENCE_SKILL_FILE = (
    "---\n"
    "name: converge-project-changes\n"
    "description: Audit and converge an explicit collaborator-delivered change set.\n"
    "---\n\n"
    "# Converge Project Changes\n\nReview and route the delivered batch.\n"
)
CONVERGENCE_OPENAI_YAML = (
    "interface:\n"
    '  display_name: "Converge Project Changes"\n'
    '  short_description: "Converge collaborator changes to project rules"\n'
    '  default_prompt: "Use $converge-project-changes to review this delivered change set."\n'
    "policy:\n"
    "  allow_implicit_invocation: true\n"
)


def with_skills(
    files: dict[str, str], skill_file: str | None = SKILL_FILE
) -> dict[str, str]:
    mutated = dict(files)
    mutated["agent/skills/README.md"] = SKILLS_README
    if skill_file is not None:
        mutated["agent/skills/fixture-skill/SKILL.md"] = skill_file
    else:
        # keep the directory physically present so the missing-SKILL.md rule,
        # not the nonexistent-skill rule, is what fires
        mutated["agent/skills/fixture-skill/.keep"] = ""
    mutated[".agents/skills"] = f"{SYMLINK_PREFIX}../agent/skills"
    return mutated


def without_path(files: dict[str, str], path: str) -> dict[str, str]:
    mutated = dict(files)
    mutated.pop(path, None)
    return mutated


def with_codex_resources(files: dict[str, str]) -> dict[str, str]:
    mutated = with_skills(files)
    mutated["agent/skills/fixture-skill/agents/openai.yaml"] = OPENAI_YAML
    mutated["agent/skills/fixture-skill/references/guide.md"] = "# Guide\n"
    mutated["agent/skills/fixture-skill/scripts/check.sh"] = "#!/bin/sh\nexit 0\n"
    mutated["agent/skills/fixture-skill/assets/template.txt"] = "fixture\n"
    return mutated


def with_domain_skill(
    files: dict[str, str],
    skill_file: str = DOMAIN_SKILL_FILE,
    openai_yaml: str | None = DOMAIN_OPENAI_YAML,
) -> dict[str, str]:
    mutated = dict(files)
    mutated["agent/skills/README.md"] = DOMAIN_SKILLS_README
    mutated[f"agent/skills/{DOMAIN_SKILL_SLUG}/SKILL.md"] = skill_file
    if openai_yaml is not None:
        mutated[f"agent/skills/{DOMAIN_SKILL_SLUG}/agents/openai.yaml"] = openai_yaml
    mutated[".agents/skills"] = f"{SYMLINK_PREFIX}../agent/skills"
    return mutated


def with_workflow_skill(
    files: dict[str, str],
    *,
    skill_file: str = WORKFLOW_SKILL_FILE,
    openai_yaml: str | None = WORKFLOW_OPENAI_YAML,
) -> dict[str, str]:
    mutated = dict(files)
    mutated["agent/skills/README.md"] = WORKFLOW_SKILLS_README
    mutated[f"agent/skills/{WORKFLOW_SKILL_SLUG}/SKILL.md"] = skill_file
    if openai_yaml is not None:
        mutated[f"agent/skills/{WORKFLOW_SKILL_SLUG}/agents/openai.yaml"] = openai_yaml
    mutated[".agents/skills"] = f"{SYMLINK_PREFIX}../agent/skills"
    return mutated


def with_convergence_skill(
    files: dict[str, str],
    *,
    openai_yaml: str | None = CONVERGENCE_OPENAI_YAML,
) -> dict[str, str]:
    mutated = dict(files)
    mutated["agent/skills/README.md"] = CONVERGENCE_SKILLS_README
    mutated[f"agent/skills/{CONVERGENCE_SKILL_SLUG}/SKILL.md"] = (
        CONVERGENCE_SKILL_FILE
    )
    if openai_yaml is not None:
        mutated[f"agent/skills/{CONVERGENCE_SKILL_SLUG}/agents/openai.yaml"] = (
            openai_yaml
        )
    mutated[".agents/skills"] = f"{SYMLINK_PREFIX}../agent/skills"
    return mutated


def with_multidigit_delivery(files: dict[str, str]) -> dict[str, str]:
    """Remap the base graph to the 12.2.1.0/12.2.1.1 compression example."""
    replacements = (
        ("S0100", "S12210"),
        ("M0100", "M12210"),
        ("W0101", "W12211"),
        ("v0.1.0", "v12.2.1"),
        ("0.1.0.0", "12.2.1.0"),
        ("0.1.0.1", "12.2.1.1"),
    )
    mutated: dict[str, str] = {}
    for path, content in files.items():
        for old, new in replacements:
            path = path.replace(old, new)
            content = content.replace(old, new)
        mutated[path] = content
    return mutated


def with_session_parent_mismatch(files: dict[str, str]) -> dict[str, str]:
    """Keep W0101 resolvable while assigning it to the wrong session M."""
    mutated = dict(files)
    session_path = f"{SESSION_DIR}/session.json"
    session = json.loads(mutated[session_path])
    session["milestones"].append(
        {
            "id": "M0110",
            "title": "Wrong parent",
            "status": "active",
            "event_seqs": [1],
        }
    )
    session["work_items"][0]["milestone_id"] = "M0110"
    mutated[session_path] = json.dumps(session, indent=2) + "\n"
    return mutated


def with_same_day_delivery_order_trap(files: dict[str, str]) -> dict[str, str]:
    """Add an older higher-delivery session before a newer lower-delivery one."""
    newer_id = "S0100-20260828-002-newer-scope"
    mutated: dict[str, str] = {}
    for path, content in files.items():
        mutated[path.replace(SESSION_ID, newer_id)] = content.replace(
            SESSION_ID, newer_id
        )

    newer_dir = f"agent/sessions/2026/08/{newer_id}"
    older_id = "S0101-20260828-001-older-scope"
    older_dir = f"agent/sessions/2026/08/{older_id}"
    newer_session = json.loads(mutated[f"{newer_dir}/session.json"])
    older_session = dict(newer_session)
    older_session["id"] = older_id
    older_session["delivery"] = "0.1.0.1"
    older_session["work_items"] = [dict(newer_session["work_items"][0])]
    older_session["work_items"][0]["status"] = "queued"
    mutated[f"{older_dir}/session.json"] = json.dumps(older_session, indent=2) + "\n"
    for filename in ("events.jsonl", "summary.md", "notes.md"):
        mutated[f"{older_dir}/{filename}"] = mutated[
            f"{newer_dir}/{filename}"
        ].replace(newer_id, older_id)
    mutated["agent/sessions/README.md"] += (
        f"| [{older_id}](2026/08/{older_id}/summary.md) "
        "| 2026-08-28 | Exact | Complete | older scope fixture |\n"
    )
    return mutated


def with_session_scope(
    files: dict[str, str], delivery: str
) -> dict[str, str]:
    """Move the fixture session to another explicit delivery scope."""
    delivery_code = delivery.replace(".", "")
    scoped_id = SESSION_ID.replace("S0100-", f"S{delivery_code}-", 1)
    mutated: dict[str, str] = {}
    for path, content in files.items():
        path = path.replace(SESSION_ID, scoped_id)
        content = content.replace(SESSION_ID, scoped_id)
        mutated[path] = content
    session_path = next(
        path
        for path in mutated
        if path.startswith("agent/sessions/") and path.endswith("/session.json")
    )
    session = json.loads(mutated[session_path])
    session["delivery"] = delivery
    mutated[session_path] = json.dumps(session, indent=2) + "\n"
    return mutated


def with_session_compact_collision(files: dict[str, str]) -> dict[str, str]:
    """Keep scope S12210 while changing its dotted coordinate ambiguously."""
    mutated = with_multidigit_delivery(files)
    session_path = next(
        path
        for path in mutated
        if path.startswith("agent/sessions/") and path.endswith("/session.json")
    )
    session = json.loads(mutated[session_path])
    session["delivery"] = "1.2.2.10"
    mutated[session_path] = json.dumps(session, indent=2) + "\n"
    return mutated


def with_work_compact_collision(files: dict[str, str]) -> dict[str, str]:
    """Add a valid W whose coordinate collides with another M compact body."""
    mutated = with_multidigit_delivery(files)
    mutated["agent/plan/M1220-collision-parent/plan.md"] = (
        "---\n"
        "id: M1220\n"
        "release: v1.2.2\n"
        "delivery: 1.2.2.0\n"
        "status: Queued\n"
        "updated: 2026-08-28\n"
        "---\n"
        "# M1220\n"
    )
    mutated["agent/plan/M1220-collision-parent/work/W12210-collision.md"] = (
        "---\n"
        "id: W12210\n"
        "milestone: M1220\n"
        "delivery: 1.2.2.10\n"
        "status: Queued\n"
        "updated: 2026-08-28\n"
        "---\n"
        "# W12210\n"
    )
    mutated["agent/plan/README.md"] += (
        "| [M1220](M1220-collision-parent/plan.md) | v1.2.2 | Queued |\n"
    )
    return mutated


def semantic_change_record(
    *,
    record_id: str = "SC0001",
    status: str = "Active",
    decision: str = "D0001",
    disposition: str | None = None,
    verification: str | None = None,
    superseded_by: str = "null",
    evidence: str = "fixture search",
    effective_revision: str | None = None,
) -> str:
    if disposition is None:
        disposition = "Pending" if status == "Active" else "Migrated"
    if verification is None:
        verification = "Pending" if status == "Active" else "Passed"
    if effective_revision is None:
        effective_revision = "null" if status == "Active" else "a" * 40
    return (
        "---\n"
        f"id: {record_id}\n"
        f"status: {status}\n"
        "created: 2026-08-28\n"
        "updated: 2026-08-28\n"
        f"decision: {decision}\n"
        f"session: {SESSION_ID}\n"
        "scope: fixture-governance\n"
        "history_sync: automatic\n"
        f"effective_revision: {effective_revision}\n"
        f"superseded_by: {superseded_by}\n"
        "---\n\n"
        f"# {record_id}: Fixture semantic change\n\n"
        "## Semantic replacement\n\n"
        "Replace one fixture meaning under D0001.\n\n"
        "## Migration inventory\n\n"
        "| Surface | Class | Disposition | Evidence |\n"
        "| --- | --- | --- | --- |\n"
        f"| `agent/README.md` | Current | {disposition} | {evidence} |\n\n"
        "## Active-session handoff\n\n"
        "| Session | Guidance | Status | Outcome |\n"
        "| --- | --- | --- | --- |\n"
        "| none | none | Not required | no other active fixture session |\n\n"
        "## Evidence preservation\n\n"
        "Fixture factual evidence remains unchanged.\n\n"
        "## Future-agent reminder\n\n"
        "Use the current fixture meaning.\n\n"
        "## Verification\n\n"
        "| Gate | Result |\n"
        "| --- | --- |\n"
        f"| fixture gate | {verification} |\n"
    )


def with_semantic_change(
    files: dict[str, str],
    *,
    status: str = "Active",
    decision: str = "D0001",
    disposition: str | None = None,
    verification: str | None = None,
    superseded_by: str = "null",
    evidence: str = "fixture search",
    effective_revision: str | None = None,
) -> dict[str, str]:
    mutated = with_in_progress_session(files) if status == "Active" else dict(files)
    mutated["agent/semantic-changes/SC0001-fixture.md"] = semantic_change_record(
        status=status,
        decision=decision,
        disposition=disposition,
        verification=verification,
        superseded_by=superseded_by,
        evidence=evidence,
        effective_revision=effective_revision,
    )
    mutated["agent/semantic-changes/README.md"] = (
        BASE_FILES["agent/semantic-changes/README.md"]
        + f"| [SC0001](SC0001-fixture.md) | {status} | {decision} | fixture-governance | 2026-08-28 |\n"
    )
    return mutated


def with_governance_execution_focus(files: dict[str, str]) -> dict[str, str]:
    mutated = with_semantic_change(
        with_product_execution_focus(files), decision="D0029"
    )
    product_focus = json.loads(mutated["agent/progress/focus.json"])
    mutated = update_focus_document(
        mutated,
        mode="governance",
        authority={"decision": "D0029", "semantic_change": "SC0001"},
        target=None,
        resume_target=product_focus["target"],
    )
    mutated["agent/progress/current.md"] = mutated[
        "agent/progress/current.md"
    ].replace("focus_mode: product", "focus_mode: governance")
    return mutated


PRODUCT_FOCUS_FILES = with_product_execution_focus(BASE_FILES)
GOVERNANCE_FOCUS_FILES = with_governance_execution_focus(BASE_FILES)

FOCUS_CASES: list[tuple[str, dict[str, str | None], bool, str]] = [
    (
        "D0029 requires an execution focus record",
        mutate(
            {
                "docs/architecture/execution-focus-governance.md": (
                    "# Execution Focus Governance\n\nD0029 establishes one exact owner.\n"
                ),
                "agent/memory/decisions-index.md": BASE_FILES[
                    "agent/memory/decisions-index.md"
                ]
                + "| D0029 | Execution focus governance | "
                "[focus](../../docs/architecture/execution-focus-governance.md) | Verified |\n",
            }
        ),
        True,
        "is required by D0029",
    ),
    ("valid product execution focus", PRODUCT_FOCUS_FILES, False, ""),
    ("valid governance execution focus", GOVERNANCE_FOCUS_FILES, False, ""),
    (
        "execution focus requires schema version 2",
        update_focus_document(PRODUCT_FOCUS_FILES, schema_version=1),
        True,
        "schema_version must equal 2",
    ),
    (
        "execution focus requires the D0029 governance epoch",
        update_focus_document(PRODUCT_FOCUS_FILES, governance_epoch="D0028"),
        True,
        "governance_epoch must equal 'D0029'",
    ),
    (
        "legacy session cannot own execution focus",
        update_session_document(
            PRODUCT_FOCUS_FILES,
            schema_version=1,
            governance_epoch=None,
        ),
        True,
        "must use current session schema_version 2; legacy sessions are evidence only",
    ),
    (
        "focus owner requires the D0029 governance epoch",
        update_session_document(PRODUCT_FOCUS_FILES, governance_epoch="D0028"),
        True,
        "must declare governance_epoch 'D0029'",
    ),
    (
        "execution focus owner must be in progress",
        replace(
            PRODUCT_FOCUS_FILES,
            f"{SESSION_DIR}/session.json",
            '"status": "in_progress"',
            '"status": "complete"',
        ),
        True,
        "must be in_progress with ended_at null",
    ),
    (
        "product owner must cover the focused work",
        mutate(
            {
                **PRODUCT_FOCUS_FILES,
                f"{SESSION_DIR}/session.json": json.dumps(
                    {
                        **json.loads(PRODUCT_FOCUS_FILES[f"{SESSION_DIR}/session.json"]),
                        "work_items": [],
                    },
                    indent=2,
                )
                + "\n",
            }
        ),
        True,
        "product owner session must reference the focused milestone and work item",
    ),
    (
        "focused milestone dependencies must be complete",
        replace(
            PRODUCT_FOCUS_FILES,
            "agent/plan/M0100-fixture/plan.md",
            "depends_on: []",
            "depends_on: [M0100]",
        ),
        True,
        "milestone dependency M0100 must be Complete",
    ),
    (
        "focus exit gate path is canonical",
        update_focus_document(
            PRODUCT_FOCUS_FILES,
            target={
                "milestone": "M0100",
                "work_item": "W0101",
                "exit_gate": "agent/plan/M0100-fixture/work/W0101-fixture.md#other",
            },
        ),
        True,
        "target.exit_gate must be",
    ),
    (
        "focused work item requires an Exit Gate heading",
        replace(
            PRODUCT_FOCUS_FILES,
            "agent/plan/M0100-fixture/work/W0101-fixture.md",
            "## Exit Gate",
            "## Completion Boundary",
        ),
        True,
        "focused work item has no canonical Exit Gate",
    ),
    (
        "current progress owner projects the focus",
        replace(
            PRODUCT_FOCUS_FILES,
            "agent/progress/current.md",
            f"focus_owner: {SESSION_ID}",
            "focus_owner: S0100-20260828-999-drift",
        ),
        True,
        "frontmatter focus_owner must match execution focus value",
    ),
    (
        "current progress projects the governance epoch",
        replace(
            PRODUCT_FOCUS_FILES,
            "agent/progress/current.md",
            "governance_epoch: D0029",
            "governance_epoch: D0028",
        ),
        True,
        "frontmatter governance_epoch must match execution focus value",
    ),
    (
        "current progress bounds next actions",
        mutate(
            {
                **PRODUCT_FOCUS_FILES,
                "agent/progress/current.md": PRODUCT_FOCUS_FILES[
                    "agent/progress/current.md"
                ]
                + "2. Second action.\n3. Third action.\n4. Fourth action.\n",
            }
        ),
        True,
        "Next Actions must contain between one and three items",
    ),
    (
        "current progress rejects the cumulative next-work archive",
        mutate(
            {
                **PRODUCT_FOCUS_FILES,
                "agent/progress/current.md": PRODUCT_FOCUS_FILES[
                    "agent/progress/current.md"
                ]
                + "\n## Versioned Next Work\n\n1. Competing work.\n",
            }
        ),
        True,
        "must not retain the cumulative Versioned Next Work archive",
    ),
    (
        "governance focus resolves its active semantic change",
        update_focus_document(
            GOVERNANCE_FOCUS_FILES,
            authority={"decision": "D0029", "semantic_change": "SC9999"},
        ),
        True,
        "governance semantic_change SC9999 does not resolve",
    ),
]


CASES: list[tuple[str, dict[str, str | None], bool, bool]] = [
    # name, files, expect failure, expect warning
    ("valid release and M/W/session graph", BASE_FILES, False, False),
    (
        "multi-digit delivery components concatenate directly",
        with_multidigit_delivery(BASE_FILES),
        False,
        False,
    ),
    (
        "legacy unscoped session id is rejected",
        replace(
            BASE_FILES,
            f"{SESSION_DIR}/session.json",
            SESSION_ID,
            "S20260828-001-selftest",
        ),
        True,
        False,
    ),
    (
        "ordinary prose around short session-like tokens is accepted",
        mutate(
            {
                f"{SESSION_DIR}/notes.md": BASE_FILES[f"{SESSION_DIR}/notes.md"]
                + "\nSession XS013 and S0130 are ordinary prose fixtures.\n"
            }
        ),
        False,
        False,
    ),
    (
        "legacy dated session references are rejected",
        mutate(
            {
                f"{SESSION_DIR}/notes.md": BASE_FILES[f"{SESSION_DIR}/notes.md"]
                + "\nOld references S20260828-001 and "
                "S20260828-001-selftest are invalid.\n"
            }
        ),
        True,
        False,
    ),
    (
        "legacy short session references are rejected",
        mutate(
            {
                f"{SESSION_DIR}/notes.md": BASE_FILES[f"{SESSION_DIR}/notes.md"]
                + "\nOld reference S013 is invalid.\n"
            }
        ),
        True,
        False,
    ),
    (
        "truncated scoped session references are rejected",
        mutate(
            {
                f"{SESSION_DIR}/notes.md": BASE_FILES[f"{SESSION_DIR}/notes.md"]
                + "\nIncomplete reference S0100-20260828-001 is invalid.\n"
            }
        ),
        True,
        False,
    ),
    (
        "unresolved full scoped session references are rejected",
        mutate(
            {
                f"{SESSION_DIR}/notes.md": BASE_FILES[f"{SESSION_DIR}/notes.md"]
                + "\nUnknown reference S0100-20260828-999-missing is invalid.\n"
            }
        ),
        True,
        False,
    ),
    (
        "legacy session references in event logs are rejected",
        replace(
            BASE_FILES,
            f"{SESSION_DIR}/events.jsonl",
            "fixture objective",
            "fixture objective referencing S20260828-001-selftest",
        ),
        True,
        False,
    ),
    (
        "session delivery is required",
        replace(
            BASE_FILES,
            f"{SESSION_DIR}/session.json",
            '  "delivery": "0.1.0.0",\n',
            "",
        ),
        True,
        False,
    ),
    (
        "session delivery components are canonical",
        replace(
            BASE_FILES,
            f"{SESSION_DIR}/session.json",
            '"delivery": "0.1.0.0"',
            '"delivery": "0.01.0.0"',
        ),
        True,
        False,
    ),
    (
        "session id scope derives from delivery",
        replace(
            BASE_FILES,
            f"{SESSION_DIR}/session.json",
            '"delivery": "0.1.0.0"',
            '"delivery": "0.1.0.1"',
        ),
        True,
        False,
    ),
    (
        "session id date uses scoped capture position",
        replace(
            BASE_FILES,
            f"{SESSION_DIR}/session.json",
            SESSION_ID,
            "S0100-20260230-001-selftest",
        ),
        True,
        False,
    ),
    (
        "session delivery rejects compact collision with milestone",
        with_session_compact_collision(BASE_FILES),
        True,
        False,
    ),
    (
        "work delivery rejects compact collision with milestone",
        with_work_compact_collision(BASE_FILES),
        True,
        False,
    ),
    (
        "milestone release is required exactly once",
        replace(
            BASE_FILES,
            "agent/plan/M0100-fixture/plan.md",
            "release: v0.1.0\n",
            "",
        ),
        True,
        False,
    ),
    (
        "milestone release cannot repeat",
        replace(
            BASE_FILES,
            "agent/plan/M0100-fixture/plan.md",
            "release: v0.1.0\n",
            "release: v0.1.0\nrelease: v0.1.0\n",
        ),
        True,
        False,
    ),
    (
        "milestone release requires v prefix",
        replace(
            BASE_FILES,
            "agent/plan/M0100-fixture/plan.md",
            "release: v0.1.0",
            "release: 0.1.0",
        ),
        True,
        False,
    ),
    (
        "milestone release requires three parts",
        replace(
            BASE_FILES,
            "agent/plan/M0100-fixture/plan.md",
            "release: v0.1.0",
            "release: v0.1",
        ),
        True,
        False,
    ),
    (
        "milestone release rejects leading zeros",
        replace(
            BASE_FILES,
            "agent/plan/M0100-fixture/plan.md",
            "release: v0.1.0",
            "release: v00.1.0",
        ),
        True,
        False,
    ),
    (
        "milestone delivery is required exactly once",
        replace(
            BASE_FILES,
            "agent/plan/M0100-fixture/plan.md",
            "delivery: 0.1.0.0\n",
            "",
        ),
        True,
        False,
    ),
    (
        "delivery components reject leading zeros",
        replace(
            BASE_FILES,
            "agent/plan/M0100-fixture/plan.md",
            "delivery: 0.1.0.0",
            "delivery: 0.01.0.0",
        ),
        True,
        False,
    ),
    (
        "milestone delivery uses work zero",
        replace(
            BASE_FILES,
            "agent/plan/M0100-fixture/plan.md",
            "delivery: 0.1.0.0",
            "delivery: 0.1.0.1",
        ),
        True,
        False,
    ),
    (
        "milestone delivery must match release core",
        replace(
            BASE_FILES,
            "agent/plan/M0100-fixture/plan.md",
            "delivery: 0.1.0.0",
            "delivery: 0.1.1.0",
        ),
        True,
        False,
    ),
    (
        "plan index release must match milestone plan",
        replace(
            BASE_FILES,
            "agent/plan/README.md",
            "v0.1.0",
            "v0.1.1",
        ),
        True,
        False,
    ),
    (
        "work item milestone is required exactly once",
        replace(
            BASE_FILES,
            "agent/plan/M0100-fixture/work/W0101-fixture.md",
            "milestone: M0100\n",
            "",
        ),
        True,
        False,
    ),
    (
        "work item milestone must match path and delivery",
        replace(
            BASE_FILES,
            "agent/plan/M0100-fixture/work/W0101-fixture.md",
            "milestone: M0100",
            "milestone: M0110",
        ),
        True,
        False,
    ),
    (
        "work item delivery uses nonzero work ordinal",
        replace(
            BASE_FILES,
            "agent/plan/M0100-fixture/work/W0101-fixture.md",
            "delivery: 0.1.0.1",
            "delivery: 0.1.0.0",
        ),
        True,
        False,
    ),
    (
        "work item id derives from delivery",
        replace(
            BASE_FILES,
            "agent/plan/M0100-fixture/work/W0101-fixture.md",
            "delivery: 0.1.0.1",
            "delivery: 0.1.0.2",
        ),
        True,
        False,
    ),
    (
        "session work item must resolve to durable W record",
        replace(
            BASE_FILES,
            f"{SESSION_DIR}/session.json",
            '"id": "W0101"',
            '"id": "W0102"',
        ),
        True,
        False,
    ),
    (
        "session work item must belong to declared milestone",
        with_session_parent_mismatch(BASE_FILES),
        True,
        False,
    ),
    (
        "session missing required field",
        replace(BASE_FILES, f"{SESSION_DIR}/session.json", '"time_precision": "date",\n', ""),
        True,
        False,
    ),
    (
        "in-progress session without end time",
        replace(
            replace(
                BASE_FILES,
                f"{SESSION_DIR}/session.json",
                '"status": "complete"',
                '"status": "in_progress"',
            ),
            f"{SESSION_DIR}/session.json",
            '"ended_at": "2026-08-28"',
            '"ended_at": null',
        ),
        False,
        False,
    ),
    (
        "in-progress session with end time",
        replace(
            BASE_FILES,
            f"{SESSION_DIR}/session.json",
            '"status": "complete"',
            '"status": "in_progress"',
        ),
        True,
        False,
    ),
    (
        "terminal session without end time",
        replace(
            BASE_FILES,
            f"{SESSION_DIR}/session.json",
            '"ended_at": "2026-08-28"',
            '"ended_at": null',
        ),
        True,
        False,
    ),
    (
        "terminal session notes reject TODO",
        mutate(
            {
                f"{SESSION_DIR}/notes.md": BASE_FILES[f"{SESSION_DIR}/notes.md"]
                + "\nTODO: unresolved closure work.\n"
            }
        ),
        True,
        False,
    ),
    (
        "active session notes permit TODO",
        with_in_progress_session(
            mutate(
                {
                    f"{SESSION_DIR}/notes.md": BASE_FILES[f"{SESSION_DIR}/notes.md"]
                    + "\nTODO: active follow-up.\n"
                }
            )
        ),
        False,
        False,
    ),
    (
        "event sequence gap",
        replace(BASE_FILES, f"{SESSION_DIR}/events.jsonl", '"seq": 1', '"seq": 2'),
        True,
        False,
    ),
    (
        "adopted guidance work note",
        with_guidance_event(BASE_FILES, disposition="adopted"),
        False,
        False,
    ),
    (
        "terminal guidance event must map to a milestone",
        with_terminal_guidance_event(BASE_FILES, mapped=False),
        True,
        False,
    ),
    (
        "terminal guidance event mapped to a milestone",
        with_terminal_guidance_event(BASE_FILES, mapped=True),
        False,
        False,
    ),
    (
        "adapted guidance decision",
        with_guidance_event(
            BASE_FILES,
            event_type="decision",
            disposition="adapted",
        ),
        False,
        False,
    ),
    (
        "rejected guidance work note",
        with_guidance_event(BASE_FILES, disposition="rejected"),
        False,
        False,
    ),
    (
        "deferred guidance decision",
        with_guidance_event(
            BASE_FILES,
            event_type="decision",
            disposition="deferred",
            deferred_to="agent/memory/open-decisions.md",
        ),
        False,
        False,
    ),
    (
        "guidance id without disposition",
        with_guidance_event(BASE_FILES, disposition=None),
        True,
        False,
    ),
    (
        "guidance disposition without id",
        with_guidance_event(BASE_FILES, guidance_id=None),
        True,
        False,
    ),
    (
        "invalid guidance id",
        with_guidance_event(BASE_FILES, guidance_id="G01"),
        True,
        False,
    ),
    (
        "invalid guidance disposition",
        with_guidance_event(BASE_FILES, disposition="accepted"),
        True,
        False,
    ),
    (
        "deferred guidance without target",
        with_guidance_event(BASE_FILES, disposition="deferred"),
        True,
        False,
    ),
    (
        "deferred guidance with empty target",
        with_guidance_event(
            BASE_FILES,
            disposition="deferred",
            deferred_to="",
        ),
        True,
        False,
    ),
    (
        "non-deferred guidance with target",
        with_guidance_event(
            BASE_FILES,
            disposition="adopted",
            deferred_to="agent/memory/open-decisions.md",
        ),
        True,
        False,
    ),
    (
        "guidance target without disposition",
        with_guidance_event(
            BASE_FILES,
            guidance_id=None,
            disposition=None,
            deferred_to="agent/memory/open-decisions.md",
        ),
        True,
        False,
    ),
    (
        "guidance fields on objective event",
        with_guidance_event(BASE_FILES, event_type="objective"),
        True,
        False,
    ),
    (
        "duplicate guidance disposition",
        with_duplicate_guidance_disposition(BASE_FILES),
        True,
        False,
    ),
    (
        "active session permits transient guidance",
        with_guidance_artifacts(
            with_in_progress_session(BASE_FILES),
            "G001-fixture.draft.md",
            "G002-fixture.ready.md",
            "G003-fixture.processing.md",
            "G004-fixture.patch",
        ),
        False,
        False,
    ),
    (
        "terminal session rejects draft guidance",
        with_guidance_artifacts(BASE_FILES, "G001-fixture.draft.md"),
        True,
        False,
    ),
    (
        "terminal session rejects ready guidance",
        with_guidance_artifacts(BASE_FILES, "G001-fixture.ready.md"),
        True,
        False,
    ),
    (
        "terminal session rejects processing guidance",
        with_guidance_artifacts(BASE_FILES, "G001-fixture.processing.md"),
        True,
        False,
    ),
    (
        "terminal session rejects guidance patch",
        with_guidance_artifacts(BASE_FILES, "G001-fixture.patch"),
        True,
        False,
    ),
    (
        "terminal session rejects arbitrary guidance file",
        with_guidance_artifacts(BASE_FILES, "leftover.txt"),
        True,
        False,
    ),
    (
        "terminal session rejects guidance symlink",
        {
            **BASE_FILES,
            f"{SESSION_DIR}/guidance/leftover": f"{SYMLINK_PREFIX}../notes.md",
        },
        True,
        False,
    ),
    (
        "valid populated roast and session-only contract",
        {**BASE_FILES, SUMMARY_PATH: "# Summary\n\nFixture.\n\n" + POPULATED_ROAST_BLOCK},
        False,
        False,
    ),
    (
        "active roast permits standalone TODO containers",
        with_in_progress_session(
            {**BASE_FILES, SUMMARY_PATH: "# Summary\n\nFixture.\n\n" + ACTIVE_ROAST_BLOCK}
        ),
        False,
        False,
    ),
    (
        "post-policy roast section missing",
        {**BASE_FILES, SUMMARY_PATH: "# Summary\n\nFixture.\n"},
        True,
        False,
    ),
    (
        "roast heading is exactly lowercase",
        replace(BASE_FILES, SUMMARY_PATH, "## roast", "## Roast"),
        True,
        False,
    ),
    (
        "duplicate roast section rejected",
        {**BASE_FILES, SUMMARY_PATH: BASE_FILES[SUMMARY_PATH] + "\n" + ROAST_BLOCK},
        True,
        False,
    ),
    (
        "roast bucket missing",
        replace(
            BASE_FILES,
            SUMMARY_PATH,
            "### medium roasts\n\n- none.\n\n",
            "",
        ),
        True,
        False,
    ),
    (
        "roast buckets out of order",
        replace(
            BASE_FILES,
            SUMMARY_PATH,
            "### light roasts\n\n- none.\n\n### medium roasts\n\n- none.",
            "### medium roasts\n\n- none.\n\n### light roasts\n\n- none.",
        ),
        True,
        False,
    ),
    (
        "roast bucket wrong case",
        replace(BASE_FILES, SUMMARY_PATH, "### light roasts", "### Light roasts"),
        True,
        False,
    ),
    (
        "roast prose before buckets rejected",
        replace(BASE_FILES, SUMMARY_PATH, "## roast\n\n", "## roast\n\nProse.\n\n"),
        True,
        False,
    ),
    (
        "session-only section missing",
        replace(BASE_FILES, SUMMARY_PATH, "\n## session-only\n\n- none.\n", "\n"),
        True,
        False,
    ),
    (
        "session-only must immediately follow roast",
        replace(
            BASE_FILES,
            SUMMARY_PATH,
            "## session-only",
            "## intervening\n\nnone.\n\n## session-only",
        ),
        True,
        False,
    ),
    (
        "none must be sole bucket value",
        replace(
            BASE_FILES,
            SUMMARY_PATH,
            "### light roasts\n\n- none.",
            "### light roasts\n\n- none.\n- claim -> owner (evidence)",
        ),
        True,
        False,
    ),
    (
        "terminal roast rejects TODO",
        replace(BASE_FILES, SUMMARY_PATH, "### light roasts\n\n- none.", "### light roasts\n\n- TODO."),
        True,
        False,
    ),
    (
        "terminal roast rejects embedded TODO",
        replace(
            BASE_FILES,
            SUMMARY_PATH,
            "### light roasts\n\n- none.",
            "### light roasts\n\n- fixture claim -> owner (TODO evidence)",
        ),
        True,
        False,
    ),
    (
        "TODO must be sole active bucket value",
        with_in_progress_session(
            replace(
                BASE_FILES,
                SUMMARY_PATH,
                "### light roasts\n\n- none.",
                "### light roasts\n\n- TODO.\n- claim -> owner (evidence)",
            )
        ),
        True,
        False,
    ),
    (
        "roast entry shape required",
        replace(BASE_FILES, SUMMARY_PATH, "### light roasts\n\n- none.", "### light roasts\n\n- unlabeled claim"),
        True,
        False,
    ),
    (
        "roast requires one canonical owner",
        replace(
            BASE_FILES,
            SUMMARY_PATH,
            "### light roasts\n\n- none.",
            "### light roasts\n\n- fixture claim -> agent/memory/project.md and tools/README.md (evidence)",
        ),
        True,
        False,
    ),
    (
        "roast owner path must resolve",
        replace(
            BASE_FILES,
            SUMMARY_PATH,
            "### light roasts\n\n- none.",
            "### light roasts\n\n- fixture claim -> missing/owner.md (evidence)",
        ),
        True,
        False,
    ),
    (
        "session-only reason required",
        replace(BASE_FILES, SUMMARY_PATH, "## session-only\n\n- none.", "## session-only\n\n- local detail"),
        True,
        False,
    ),
    (
        "dark roast authority required",
        replace(
            BASE_FILES,
            SUMMARY_PATH,
            "### dark roasts\n\n- none.",
            "### dark roasts\n\n- governed claim -> docs/architecture/fixture.md (evidence only)",
        ),
        True,
        False,
    ),
    (
        "claim cannot appear in two destinations",
        {
            **BASE_FILES,
            SUMMARY_PATH: "# Summary\n\nFixture.\n\n"
            + POPULATED_ROAST_BLOCK.replace(
                "fixture resume detail - reason: needed only to resume this session",
                "normalized fixture - reason: duplicate destination probe",
            ),
        },
        True,
        False,
    ),
    (
        "terminal legacy knowledge schema rejected",
        {
            **BASE_FILES,
            SUMMARY_PATH: "# Summary\n\nFixture.\n\n"
            "## Distillation\n\n- Promoted: none\n- Session-only: none\n",
        },
        True,
        False,
    ),
    (
        "active legacy knowledge schema rejected without compatibility",
        with_in_progress_session(
            {
                **BASE_FILES,
                SUMMARY_PATH: "# Summary\n\nFixture.\n\n"
                "## Distillation\n\n- Distilled: none\n",
            }
        ),
        True,
        False,
    ),
    (
        "legacy and roast schemas cannot coexist",
        {
            **BASE_FILES,
            SUMMARY_PATH: BASE_FILES[SUMMARY_PATH]
            + "\n## Distillation\n\n- Promoted: none\n",
        },
        True,
        False,
    ),
    (
        "pre-cutoff session without roast is grandfathered",
        replace(
            {**BASE_FILES, SUMMARY_PATH: "# Summary\n\nFixture.\n"},
            f"{SESSION_DIR}/session.json",
            '"started_at": "2026-08-28"',
            '"started_at": "2026-08-27"',
        ),
        False,
        False,
    ),
    (
        "pre-cutoff present roast still validates",
        replace(
            replace(
                BASE_FILES,
                f"{SESSION_DIR}/session.json",
                '"started_at": "2026-08-28"',
                '"started_at": "2026-08-27"',
            ),
            SUMMARY_PATH,
            "### medium roasts",
            "### invalid medium",
        ),
        True,
        False,
    ),
    (
        "cleanup section required from cutoff",
        replace(
            BASE_FILES,
            f"{SESSION_DIR}/session.json",
            '"started_at": "2026-08-28"',
            '"started_at": "2026-08-29"',
        ),
        True,
        False,
    ),
    (
        "cleanup section accepted from cutoff",
        replace(
            replace(
                BASE_FILES,
                f"{SESSION_DIR}/session.json",
                '"started_at": "2026-08-28"',
                '"started_at": "2026-08-29"',
            ),
            SUMMARY_PATH,
            "## roast",
            "## Cleanup\n\n- Removed: none.\n- Retained: none.\n\n## roast",
        ),
        False,
        False,
    ),
    (
        "sessions index missing row",
        mutate(
            {
                "agent/sessions/README.md": BASE_FILES["agent/sessions/README.md"].replace(
                    f"| [{SESSION_ID}](2026/08/{SESSION_ID}/summary.md) | 2026-08-28 | Exact | Complete | fixture |\n",
                    "",
                )
            }
        ),
        True,
        False,
    ),
    (
        "sessions index stale row",
        mutate(
            {
                "agent/sessions/README.md": BASE_FILES["agent/sessions/README.md"]
                + "| [S0100-20260828-002-ghost](2026/08/S0100-20260828-002-ghost/summary.md) | 2026-08-28 | Exact | Complete | ghost |\n"
            }
        ),
        True,
        False,
    ),
    (
        "ledger row missing",
        mutate({"agent/memory/open-decisions.md": BASE_FILES["agent/memory/open-decisions.md"].replace("| M0100 | One open decision | fixture | fixture |\n", "")}),
        True,
        False,
    ),
    (
        "ledger extra row",
        mutate(
            {
                "agent/memory/open-decisions.md": BASE_FILES["agent/memory/open-decisions.md"]
                + "| M0100 | Extra decision | fixture | fixture |\n"
            }
        ),
        True,
        False,
    ),
    (
        "ledger same count but wrong decision",
        replace(
            BASE_FILES,
            "agent/memory/open-decisions.md",
            "One open decision",
            "Different release identity policy",
        ),
        True,
        False,
    ),
    (
        "ledger overly generic subset",
        replace(
            BASE_FILES,
            "agent/memory/open-decisions.md",
            "One open decision",
            "open",
        ),
        True,
        False,
    ),
    (
        "Unicode decision identity mismatch",
        replace(
            replace(
                BASE_FILES,
                "agent/plan/M0100-fixture/plan.md",
                "One open decision.",
                "选择供应商路径。",
            ),
            "agent/memory/open-decisions.md",
            "One open decision",
            "决定完全不同的协议",
        ),
        True,
        False,
    ),
    (
        "decision role reversal",
        replace(
            replace(
                BASE_FILES,
                "agent/plan/M0100-fixture/plan.md",
                "One open decision.",
                "Provider owns registry; daemon consumes policy.",
            ),
            "agent/memory/open-decisions.md",
            "One open decision",
            "Daemon owns registry; provider consumes policy",
        ),
        True,
        False,
    ),
    (
        "duplicate open-decision ledger row",
        mutate(
            {
                "agent/memory/open-decisions.md": BASE_FILES[
                    "agent/memory/open-decisions.md"
                ]
                + "| M0100 | One open decision | fixture | fixture |\n"
            }
        ),
        True,
        False,
    ),
    (
        "duplicate decision index ID",
        mutate(
            {
                "agent/memory/decisions-index.md": BASE_FILES[
                    "agent/memory/decisions-index.md"
                ]
                + "| D0001 | Duplicate | "
                "[M0100](../plan/M0100-fixture/plan.md) | Active plan |\n"
            }
        ),
        True,
        False,
    ),
    (
        "unknown decision ID in agent Markdown",
        replace(
            BASE_FILES,
            f"{SESSION_DIR}/notes.md",
            "D0001",
            "D9999",
        ),
        True,
        False,
    ),
    (
        "unknown decision ID in decision event",
        replace(
            replace(
                BASE_FILES,
                f"{SESSION_DIR}/events.jsonl",
                '"type": "objective"',
                '"type": "decision"',
            ),
            f"{SESSION_DIR}/events.jsonl",
            "fixture objective",
            "fixture decision D9999",
        ),
        True,
        False,
    ),
    (
        "stale active plan warns without failing",
        replace(BASE_FILES, "agent/plan/M0100-fixture/plan.md", "updated: 2026-08-28", "updated: 2026-07-01"),
        False,
        True,
    ),
    (
        "broken markdown link",
        replace(
            BASE_FILES,
            f"{SESSION_DIR}/notes.md",
            "Fixture decision D0001.\n",
            "Fixture decision D0001 with [broken](./missing.md).\n",
        ),
        True,
        False,
    ),
    (
        "checkpoint id does not match path",
        replace(BASE_FILES, "agent/progress/checkpoints/2026/P20260828-001-fixture.md", "id: P20260828-001", "id: P20260828-002"),
        True,
        False,
    ),
    (
        "current progress references stale checkpoint",
        mutate(
            {
                "agent/progress/checkpoints/2026/P20260828-002-newer.md": (
                    "---\nid: P20260828-002\nstatus: Recorded\ncaptured: 2026-08-28\n---\n\n# Newer\n"
                )
            }
        ),
        True,
        False,
    ),
    (
        "current progress references unknown checkpoint",
        replace(BASE_FILES, "agent/progress/current.md", "checkpoint: P20260828-001", "checkpoint: P19990101-001"),
        True,
        False,
    ),
    (
        "latest session status drift warns without failing",
        replace(BASE_FILES, "agent/plan/M0100-fixture/plan.md", "status: Active", "status: Queued"),
        False,
        True,
    ),
    (
        "latest same-day session follows ordinal before delivery scope",
        with_same_day_delivery_order_trap(BASE_FILES),
        False,
        False,
    ),
    (
        "valid minimal Codex skill package",
        with_skills(BASE_FILES),
        False,
        False,
    ),
    (
        "valid Codex skill package with optional resources",
        with_codex_resources(BASE_FILES),
        False,
        False,
    ),
    (
        "valid Codex optional frontmatter fields",
        with_skills(BASE_FILES, skill_file=EXTENDED_SKILL_FILE),
        False,
        False,
    ),
    (
        "valid domain skill package",
        with_domain_skill(BASE_FILES),
        False,
        False,
    ),
    (
        "valid explicit-only workflow skill package",
        with_workflow_skill(BASE_FILES),
        False,
        False,
    ),
    (
        "valid implicitly routed convergence skill package",
        with_convergence_skill(BASE_FILES),
        False,
        False,
    ),
    (
        "convergence skill rejects explicit-only invocation policy",
        with_convergence_skill(
            BASE_FILES,
            openai_yaml=CONVERGENCE_OPENAI_YAML.replace(
                "allow_implicit_invocation: true",
                "allow_implicit_invocation: false",
            ),
        ),
        True,
        False,
    ),
    (
        "routed workflow skill requires openai YAML",
        with_workflow_skill(BASE_FILES, openai_yaml=None),
        True,
        False,
    ),
    (
        "roast requires explicit invocation policy",
        with_workflow_skill(
            BASE_FILES,
            openai_yaml=WORKFLOW_OPENAI_YAML.replace(
                "policy:\n  allow_implicit_invocation: false\n",
                "",
            ),
        ),
        True,
        False,
    ),
    (
        "roast rejects implicit invocation policy",
        with_workflow_skill(
            BASE_FILES,
            openai_yaml=WORKFLOW_OPENAI_YAML.replace(
                "allow_implicit_invocation: false",
                "allow_implicit_invocation: true",
            ),
        ),
        True,
        False,
    ),
    (
        "openai policy value must be an unquoted boolean",
        with_workflow_skill(
            BASE_FILES,
            openai_yaml=WORKFLOW_OPENAI_YAML.replace(
                "allow_implicit_invocation: false",
                'allow_implicit_invocation: "false"',
            ),
        ),
        True,
        False,
    ),
    (
        "roast default prompt rejects an additional skill token",
        with_workflow_skill(
            BASE_FILES,
            openai_yaml=WORKFLOW_OPENAI_YAML.replace(
                "to classify and route",
                "with $distill-project-knowledge to classify and route",
            ),
        ),
        True,
        False,
    ),
    (
        "obsolete callable skill slug is forbidden",
        {
            **with_skills(BASE_FILES),
            "agent/skills/README.md": SKILLS_README.replace(
                "| [fixture-skill](fixture-skill/SKILL.md) | Active | Testing fixture skills |",
                "| [fixture-skill](fixture-skill/SKILL.md) | Active | Testing fixture skills |\n"
                "| [distill-project-knowledge](distill-project-knowledge/SKILL.md) | "
                "Retired | Obsolete fixture |",
            ),
            "agent/skills/distill-project-knowledge/SKILL.md": (
                "---\nname: distill-project-knowledge\n"
                "description: Obsolete fixture skill.\n---\n\n# Obsolete\n"
            ),
        },
        True,
        False,
    ),
    (
        "skills index missing row",
        replace(
            with_skills(BASE_FILES),
            "agent/skills/README.md",
            SKILLS_README,
            "# Skills\n\nNo skills.\n",
        ),
        True,
        False,
    ),
    (
        "stray skill link outside Index is not a catalog row",
        {
            **with_skills(BASE_FILES),
            "agent/skills/README.md": (
                "# Skills\n\n## Index\n\n"
                "| Skill | Status | Use when |\n"
                "| --- | --- | --- |\n\n"
                "## Notes\n\n"
                "[fixture-skill](fixture-skill/SKILL.md)\n"
            ),
        },
        True,
        False,
    ),
    (
        "skills index has noncanonical columns",
        {
            **with_skills(BASE_FILES),
            "agent/skills/README.md": SKILLS_README.replace(
                "| Skill | Status | Use when |\n| --- | --- | --- |",
                "| Skill | Status |\n| --- | --- |",
            ),
        },
        True,
        False,
    ),
    (
        "skills index duplicate row",
        {
            **with_skills(BASE_FILES),
            "agent/skills/README.md": SKILLS_README
            + "| [fixture-skill](fixture-skill/SKILL.md) | Active | Duplicate |\n",
        },
        True,
        False,
    ),
    (
        "skills index invalid lifecycle status",
        {
            **with_skills(BASE_FILES),
            "agent/skills/README.md": SKILLS_README.replace(
                "| Active | Testing fixture skills |",
                "| Experimental | Testing fixture skills |",
            ),
        },
        True,
        False,
    ),
    (
        "skills index wrong canonical target",
        {
            **with_skills(BASE_FILES),
            "agent/skills/README.md": SKILLS_README.replace(
                "fixture-skill/SKILL.md",
                "other/SKILL.md",
            ),
        },
        True,
        False,
    ),
    (
        "malformed openai YAML",
        {
            **with_codex_resources(BASE_FILES),
            "agent/skills/fixture-skill/agents/openai.yaml": OPENAI_YAML.replace(
                'display_name: "Fixture Skill"',
                'display_name: "Fixture Skill',
            ),
        },
        True,
        False,
    ),
    (
        "unsupported openai YAML schema field",
        {
            **with_codex_resources(BASE_FILES),
            "agent/skills/fixture-skill/agents/openai.yaml": OPENAI_YAML
            + '  icon: "fixture.png"\n',
        },
        True,
        False,
    ),
    (
        "openai YAML missing required field",
        {
            **with_codex_resources(BASE_FILES),
            "agent/skills/fixture-skill/agents/openai.yaml": OPENAI_YAML.replace(
                '  display_name: "Fixture Skill"\n',
                "",
            ),
        },
        True,
        False,
    ),
    (
        "openai YAML wrong default skill token",
        {
            **with_codex_resources(BASE_FILES),
            "agent/skills/fixture-skill/agents/openai.yaml": OPENAI_YAML.replace(
                "$fixture-skill",
                "$other-skill",
            ),
        },
        True,
        False,
    ),
    (
        "openai YAML repeats default skill token",
        {
            **with_codex_resources(BASE_FILES),
            "agent/skills/fixture-skill/agents/openai.yaml": OPENAI_YAML.replace(
                "to review this fixture skill package.",
                "and $fixture-skill to review this fixture skill package.",
            ),
        },
        True,
        False,
    ),
    (
        "openai YAML short description out of bounds",
        {
            **with_codex_resources(BASE_FILES),
            "agent/skills/fixture-skill/agents/openai.yaml": OPENAI_YAML.replace(
                "Review fixture skill package behavior",
                "Too short",
            ),
        },
        True,
        False,
    ),
    (
        "openai YAML long description out of bounds",
        {
            **with_codex_resources(BASE_FILES),
            "agent/skills/fixture-skill/agents/openai.yaml": OPENAI_YAML.replace(
                "Review fixture skill package behavior",
                "Review fixture skill package behavior with a deliberately "
                "overlong interface description",
            ),
        },
        True,
        False,
    ),
    (
        "domain skill missing openai YAML",
        with_domain_skill(BASE_FILES, openai_yaml=None),
        True,
        False,
    ),
    (
        "domain skill sections out of order",
        with_domain_skill(
            BASE_FILES,
            skill_file=DOMAIN_SKILL_FILE.replace(
                "## Routing\n\nFixture routing.\n\n"
                "## Workflow\n\nFixture workflow.\n\n",
                "## Workflow\n\nFixture workflow.\n\n"
                "## Routing\n\nFixture routing.\n\n",
            ),
        ),
        True,
        False,
    ),
    (
        "skill directory without SKILL.md",
        with_skills(BASE_FILES, skill_file=None),
        True,
        False,
    ),
    (
        "skill missing description frontmatter",
        with_skills(
            BASE_FILES,
            skill_file=SKILL_FILE.replace("description: fixture skill\n", ""),
        ),
        True,
        False,
    ),
    (
        "legacy repository status is not Codex frontmatter",
        with_skills(
            BASE_FILES,
            skill_file=SKILL_FILE.replace(
                "description: fixture skill\n",
                "description: fixture skill\nstatus: Active\n",
            ),
        ),
        True,
        False,
    ),
    (
        "skill name must match directory slug",
        with_skills(
            BASE_FILES,
            skill_file=SKILL_FILE.replace("name: fixture-skill", "name: other-skill"),
        ),
        True,
        False,
    ),
    (
        "Codex discovery symlink missing",
        without_path(with_skills(BASE_FILES), ".agents/skills"),
        True,
        False,
    ),
    (
        "Codex discovery symlink target drift",
        {
            **with_skills(BASE_FILES),
            ".agents/skills": f"{SYMLINK_PREFIX}../wrong-skills",
        },
        True,
        False,
    ),
    (
        "skill directory name not a slug",
        {
            **with_skills(BASE_FILES),
            "agent/skills/fixture-skill/SKILL.md": SKILL_FILE,
            "agent/skills/Bad_Name/SKILL.md": SKILL_FILE,
        },
        True,
        False,
    ),
    (
        "valid Claude entry-point bridge",
        mutate(
            {
                "CLAUDE.md": "# CLAUDE.md\n\n@AGENTS.md\n",
                ".claude/settings.json": "{\"hooks\": {}}\n",
            }
        ),
        False,
        False,
    ),
    (
        "CLAUDE.md duplicating rules instead of importing",
        mutate({"CLAUDE.md": "# CLAUDE.md\n\nRead the rules: do not rewrite history.\n"}),
        True,
        False,
    ),
    (
        "broken Claude settings JSON",
        mutate(
            {
                "CLAUDE.md": "# CLAUDE.md\n\n@AGENTS.md\n",
                ".claude/settings.json": "{not json",
            }
        ),
        True,
        False,
    ),
]


SC_CASES: list[tuple[str, dict[str, str | None], bool, str]] = [
    (
        "valid Active semantic change",
        with_semantic_change(BASE_FILES),
        False,
        "",
    ),
    (
        "valid Applied semantic change",
        with_semantic_change(BASE_FILES, status="Applied"),
        False,
        "",
    ),
    (
        "semantic-change index missing row",
        {
            **with_semantic_change(BASE_FILES),
            "agent/semantic-changes/README.md": BASE_FILES[
                "agent/semantic-changes/README.md"
            ],
        },
        True,
        "semantic-change index is missing SC0001",
    ),
    (
        "semantic-change index status drift",
        replace(
            with_semantic_change(BASE_FILES),
            "agent/semantic-changes/README.md",
            "| Active | D0001 |",
            "| Applied | D0001 |",
        ),
        True,
        "semantic-change index status drifts for SC0001",
    ),
    (
        "semantic-change path identity mismatch",
        {
            **without_path(
                with_semantic_change(BASE_FILES),
                "agent/semantic-changes/SC0001-fixture.md",
            ),
            "agent/semantic-changes/SC0002-fixture.md": semantic_change_record(),
        },
        True,
        "frontmatter id 'SC0001' does not match path id 'SC0002'",
    ),
    (
        "semantic-change required section missing",
        replace(
            with_semantic_change(BASE_FILES),
            "agent/semantic-changes/SC0001-fixture.md",
            "## Future-agent reminder",
            "## Missing reminder",
        ),
        True,
        "semantic change sections must appear exactly in order",
    ),
    (
        "semantic-change invalid status",
        with_semantic_change(BASE_FILES, status="Broken"),
        True,
        "semantic-change status is invalid",
    ),
    (
        "semantic-change unknown decision",
        with_semantic_change(BASE_FILES, decision="D9999"),
        True,
        "decision reference D9999 is absent from the index",
    ),
    (
        "Active semantic change requires active migration session",
        {
            **with_semantic_change(BASE_FILES),
            f"{SESSION_DIR}/session.json": BASE_FILES[f"{SESSION_DIR}/session.json"],
        },
        True,
        "Active semantic change requires an in-progress migration session",
    ),
    (
        "semantic-change duplicate migration surface",
        replace(
            with_semantic_change(BASE_FILES),
            "agent/semantic-changes/SC0001-fixture.md",
            "| `agent/README.md` | Current | Pending | fixture search |\n",
            "| `agent/README.md` | Current | Pending | fixture search |\n"
            "| `agent/README.md` | Tooling | Pending | duplicate search |\n",
        ),
        True,
        "migration inventory repeats Surface 'agent/README.md'",
    ),
    (
        "semantic-change invalid migration disposition",
        with_semantic_change(BASE_FILES, disposition="Done"),
        True,
        "migration Disposition is invalid",
    ),
    (
        "Applied semantic change rejects Pending surface",
        with_semantic_change(
            BASE_FILES,
            status="Applied",
            disposition="Pending",
        ),
        True,
        "Applied semantic change retains Pending Surface",
    ),
    (
        "Applied semantic change rejects pending verification",
        with_semantic_change(
            BASE_FILES,
            status="Applied",
            verification="Pending",
        ),
        True,
        "Applied semantic change has incomplete verification",
    ),
    (
        "Applied semantic change rejects negated pass text",
        with_semantic_change(
            BASE_FILES,
            status="Applied",
            verification="Not passed",
        ),
        True,
        "Applied semantic change requires an explicit passing result",
    ),
    (
        "Applied semantic change rejects placeholder evidence",
        with_semantic_change(
            BASE_FILES,
            status="Applied",
            evidence="TODO",
        ),
        True,
        "Applied semantic change has placeholder Evidence",
    ),
    (
        "Applied semantic change requires a full revision",
        with_semantic_change(
            BASE_FILES,
            status="Applied",
            effective_revision="abcdef1",
        ),
        True,
        "Applied semantic change requires a full hexadecimal effective_revision",
    ),
    (
        "Applied semantic change rejects empty verification gate",
        replace(
            with_semantic_change(BASE_FILES, status="Applied"),
            "agent/semantic-changes/SC0001-fixture.md",
            "| fixture gate | Passed |",
            "|  | Passed |",
        ),
        True,
        "Applied semantic change has an empty verification Gate",
    ),
    (
        "semantic-change supersession must resolve",
        with_semantic_change(
            BASE_FILES,
            status="Superseded",
            superseded_by="SC9999",
        ),
        True,
        "superseded_by does not resolve: SC9999",
    ),
    (
        "semantic change cannot supersede itself",
        with_semantic_change(
            BASE_FILES,
            status="Superseded",
            superseded_by="SC0001",
        ),
        True,
        "semantic change cannot supersede itself",
    ),
    (
        "semantic-change supersession must move forward",
        {
            **with_semantic_change(BASE_FILES, status="Applied"),
            "agent/semantic-changes/SC0002-later.md": semantic_change_record(
                record_id="SC0002",
                status="Superseded",
                superseded_by="SC0001",
            ),
            "agent/semantic-changes/README.md": (
                BASE_FILES["agent/semantic-changes/README.md"]
                + "| [SC0001](SC0001-fixture.md) | Applied | D0001 | fixture-governance | 2026-08-28 |\n"
                + "| [SC0002](SC0002-later.md) | Superseded | D0001 | fixture-governance | 2026-08-28 |\n"
            ),
        },
        True,
        "superseded_by must name a later SC identity",
    ),
]


def main() -> int:
    failures = 0
    with tempfile.TemporaryDirectory(prefix="metaflux-records-selftest-") as temporary:
        root = Path(temporary) / "repo"
        for name, files, expect_failure, expect_warning in CASES:
            if root.exists():
                shutil.rmtree(root)
            root.mkdir(parents=True)
            write_tree(root, files)  # type: ignore[arg-type]
            code, errors, warnings = run_validator(root)
            failed = (code != 0) != expect_failure
            warned = bool(warnings) != expect_warning
            if failed or warned:
                failures += 1
                print(f"FAIL: {name}", file=sys.stderr)
                print(f"  exit={code} errors={errors} warnings={warnings}", file=sys.stderr)
            else:
                print(f"ok: {name}")
        for name, files, expect_failure, needle in FOCUS_CASES:
            if root.exists():
                shutil.rmtree(root)
            root.mkdir(parents=True)
            write_tree(root, files)  # type: ignore[arg-type]
            code, errors, warnings = run_validator(root)
            failed = (code != 0) != expect_failure
            wrong_error = bool(needle) and not any(needle in error for error in errors)
            if failed or wrong_error or warnings:
                failures += 1
                print(f"FAIL: {name}", file=sys.stderr)
                print(f"  exit={code} errors={errors} warnings={warnings}", file=sys.stderr)
            else:
                print(f"ok: {name}")
        for name, files, expect_failure, needle in SC_CASES:
            if root.exists():
                shutil.rmtree(root)
            root.mkdir(parents=True)
            write_tree(root, files)  # type: ignore[arg-type]
            code, errors, warnings = run_validator(root)
            failed = (code != 0) != expect_failure
            wrong_error = bool(needle) and not any(needle in error for error in errors)
            if failed or wrong_error or warnings:
                failures += 1
                print(f"FAIL: {name}", file=sys.stderr)
                print(f"  exit={code} errors={errors} warnings={warnings}", file=sys.stderr)
            else:
                print(f"ok: {name}")
        if root.exists():
            shutil.rmtree(root)
        root.mkdir(parents=True)
        scaffold_problems = check_new_session_skeleton(root)
        if scaffold_problems:
            failures += 1
            print("FAIL: new-session lifecycle skeleton", file=sys.stderr)
            for problem in scaffold_problems:
                print(f"  {problem}", file=sys.stderr)
        else:
            print("ok: new-session lifecycle skeleton")
        isolation_problems = check_guidance_inbox_isolation(root)
        if isolation_problems:
            failures += 1
            print("FAIL: guidance inbox isolation", file=sys.stderr)
            for problem in isolation_problems:
                print(f"  {problem}", file=sys.stderr)
        else:
            print("ok: guidance inbox isolation")
        pre_commit_problems = check_pre_commit_guidance_gate(root)
        if pre_commit_problems:
            failures += 1
            print("FAIL: pre-commit guidance gate", file=sys.stderr)
            for problem in pre_commit_problems:
                print(f"  {problem}", file=sys.stderr)
        else:
            print("ok: pre-commit guidance gate")
        session_gate_problems = check_pre_commit_session_gate(root)
        if session_gate_problems:
            failures += 1
            print("FAIL: pre-commit execution focus gate", file=sys.stderr)
            for problem in session_gate_problems:
                print(f"  {problem}", file=sys.stderr)
        else:
            print("ok: pre-commit execution focus gate")
        claude_guard_problems = check_claude_focus_guard(root)
        if claude_guard_problems:
            failures += 1
            print("FAIL: Claude execution focus guard", file=sys.stderr)
            for problem in claude_guard_problems:
                print(f"  {problem}", file=sys.stderr)
        else:
            print("ok: Claude execution focus guard")
        cached_problems = check_cached_tree_validation(root)
        if cached_problems:
            failures += 1
            print("FAIL: cached tree validation", file=sys.stderr)
            for problem in cached_problems:
                print(f"  {problem}", file=sys.stderr)
        else:
            print("ok: cached tree validation")
    if failures:
        print(f"{failures} case(s) failed", file=sys.stderr)
        return 1
    print(
        "agent-records self-test: "
        f"{len(CASES) + len(FOCUS_CASES) + len(SC_CASES) + 6} case(s) passed"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
