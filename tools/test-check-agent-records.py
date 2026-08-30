#!/usr/bin/env python3
"""Self-test for tools/check-agent-records.py using synthetic golden trees.

Builds a minimal valid agent/ tree in a temporary directory and asserts the
validator's behavior on it, then mutates one aspect per case to pin every rule:
required session fields, delivery-scoped session ids, lifecycle timestamps,
contiguous event sequence numbers, guidance dispositions, transient-inbox isolation
and cleanup, distillation, index completeness (both directions), milestone
release/index consistency, M/W ownership, session-to-plan resolution, Codex
skill-package compatibility and
discovery, open-decision identity, decision-index references, staleness warnings,
markdown link existence, checkpoint id/path agreement, current-progress freshness,
latest-session status consistency, and the staged-guidance gate.

Run from anywhere:

    python3 tools/test-check-agent-records.py

Uses only the standard library; the validator is loaded by path so the
hyphenated filename is not a problem.
"""

from __future__ import annotations

import importlib.util
import json
import os
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

spec = importlib.util.spec_from_file_location("check_agent_records", VALIDATOR_PATH)
assert spec is not None and spec.loader is not None
check_agent_records = importlib.util.module_from_spec(spec)
spec.loader.exec_module(check_agent_records)
Validator = check_agent_records.Validator

SESSION_ID = "S0100-20260828-001-selftest"
SESSION_DIR = f"agent/sessions/2026/08/{SESSION_ID}"
SYMLINK_PREFIX = "SYMLINK->"

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
    f"{SESSION_DIR}/summary.md": (
        "# Summary\n\nFixture.\n\n## Distillation\n\n- Distilled: none\n"
    ),
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


def with_in_progress_session(files: dict[str, str]) -> dict[str, str]:
    mutated = dict(files)
    session_path = f"{SESSION_DIR}/session.json"
    session = json.loads(mutated[session_path])
    session["status"] = "in_progress"
    session["ended_at"] = None
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
    if not summary.startswith("# Session Summary\n\n## Objective and outcome\n"):
        problems.append("generated summary does not use the current section shape")

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
        fake_python.write_text("#!/bin/sh\nexit 0\n", encoding="utf-8")
        fake_python.chmod(0o755)
        environment = os.environ.copy()
        environment["PATH"] = f"{fake_bin}{os.pathsep}{environment.get('PATH', '')}"
        return environment

    guidance_path = Path(
        "agent/sessions/2026/08/S0100-20260828-001-selftest/guidance/G001-fixture.ready.md"
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
        "distillation section missing",
        replace(BASE_FILES, f"{SESSION_DIR}/summary.md", "\n## Distillation\n\n- Distilled: none\n", ""),
        True,
        False,
    ),
    (
        "pre-cutoff session without distillation is grandfathered",
        replace(
            BASE_FILES,
            f"{SESSION_DIR}/session.json",
            '"started_at": "2026-08-28"',
            '"started_at": "2026-08-27"',
        ),
        False,
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
            f"{SESSION_DIR}/summary.md",
            "## Distillation",
            "## Cleanup\n\n- Removed: none.\n- Retained: none.\n\n## Distillation",
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
    if failures:
        print(f"{failures} case(s) failed", file=sys.stderr)
        return 1
    print(f"agent-records self-test: {len(CASES) + 3} case(s) passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
