#!/usr/bin/env python3
"""Scaffold a MetaFlux project work session.

Allocates the next semantic-scope session id for today, creates the session
directory with a validator-clean skeleton (session.json, events.jsonl,
summary.md, notes.md), appends the index row to agent/sessions/README.md, and
prints the next steps. Run from the repository root:

    python3 tools/new-session.py 0.1.0.1 my-session-slug

The skeleton passes tools/check-agent-records.py immediately; replace the TODO
fields as the session progresses and set status to complete when finished.
Uses only the standard library.
"""

from __future__ import annotations

import argparse
import datetime as dt
import json
import re
import subprocess
import sys
from pathlib import Path

SESSION_ID_TEMPLATE = "S{delivery}-{date}-{sequence:03d}-{slug}"
SESSION_ID_RE = re.compile(
    r"^S(?P<delivery>\d{4,})-(?P<date>\d{8})-"
    r"(?P<sequence>\d{3})-[a-z0-9][a-z0-9-]*$"
)
DELIVERY_COORDINATE_RE = re.compile(
    r"^(0|[1-9]\d*)\.(0|[1-9]\d*)\."
    r"(0|[1-9]\d*)\.(0|[1-9]\d*)$"
)
SLUG_RE = re.compile(r"^[a-z0-9][a-z0-9-]*$")


def compact_delivery(coordinate: str) -> str:
    return "".join(coordinate.split("."))


def frontmatter_delivery(path: Path) -> str | None:
    """Read one canonical delivery value from simple Agent frontmatter."""
    try:
        lines = path.read_text(encoding="utf-8").splitlines()
    except (OSError, UnicodeDecodeError):
        return None
    if not lines or lines[0].strip() != "---":
        return None
    try:
        closing = next(
            index
            for index, line in enumerate(lines[1:], start=1)
            if line.strip() == "---"
        )
    except StopIteration:
        return None
    values: list[str] = []
    for line in lines[1:closing]:
        match = re.fullmatch(r"delivery:\s*(.*?)\s*", line)
        if match is None:
            continue
        value = match.group(1)
        if len(value) >= 2 and value[0] == value[-1] and value[0] in {"'", '"'}:
            value = value[1:-1]
        values.append(value)
    if len(values) != 1 or DELIVERY_COORDINATE_RE.fullmatch(values[0]) is None:
        return None
    return values[0]


def repository_deliveries(repo_root: Path) -> list[tuple[Path, str]]:
    """Collect canonical M/W/S coordinates before scaffolding a new session."""
    records: list[tuple[Path, str]] = []
    plan_root = repo_root / "agent" / "plan"
    plan_files = sorted(plan_root.glob("M*/plan.md")) + sorted(
        plan_root.glob("M*/work/W*-*.md")
    )
    for path in plan_files:
        delivery = frontmatter_delivery(path)
        if delivery is not None:
            records.append((path, delivery))

    sessions_root = repo_root / "agent" / "sessions"
    for path in sorted(sessions_root.glob("*/*/S*/session.json")):
        try:
            document = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, UnicodeDecodeError, json.JSONDecodeError):
            continue
        delivery = document.get("delivery") if isinstance(document, dict) else None
        if isinstance(delivery, str) and DELIVERY_COORDINATE_RE.fullmatch(delivery):
            records.append((path, delivery))
    return records


def git_revision(repo_root: Path) -> str | None:
    try:
        result = subprocess.run(
            ["git", "rev-parse", "HEAD"],
            cwd=repo_root,
            capture_output=True,
            text=True,
            check=True,
        )
    except (OSError, subprocess.CalledProcessError):
        return None
    return result.stdout.strip() or None


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "delivery",
        help="four-part delivery coordinate, for example 0.1.0.1",
    )
    parser.add_argument("slug", help="lowercase hyphenated session slug")
    parser.add_argument(
        "--repository",
        default="MetaFlux-Core",
        help="repository name recorded in session.json",
    )
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=Path.cwd(),
        help="repository root (default: current directory)",
    )
    arguments = parser.parse_args()

    if not DELIVERY_COORDINATE_RE.fullmatch(arguments.delivery):
        print(
            "error: delivery must contain four non-negative decimal components: "
            f"{arguments.delivery!r}",
            file=sys.stderr,
        )
        return 1
    if not SLUG_RE.fullmatch(arguments.slug):
        print(f"error: slug must match {SLUG_RE.pattern}: {arguments.slug!r}", file=sys.stderr)
        return 1

    repo_root = arguments.repo_root.resolve()
    sessions_root = repo_root / "agent" / "sessions"
    if not sessions_root.is_dir():
        print(f"error: {sessions_root} does not exist", file=sys.stderr)
        return 1

    requested_body = compact_delivery(arguments.delivery)
    for existing_path, existing_delivery in repository_deliveries(repo_root):
        if (
            compact_delivery(existing_delivery) == requested_body
            and existing_delivery != arguments.delivery
        ):
            print(
                f"error: delivery compact body {requested_body!r} is ambiguous: "
                f"{arguments.delivery!r} collides with {existing_delivery!r} at "
                f"{existing_path.relative_to(repo_root)}",
                file=sys.stderr,
            )
            return 1

    today = dt.date.today()
    month_dir = sessions_root / f"{today:%Y}" / f"{today:%m}"
    month_dir.mkdir(parents=True, exist_ok=True)

    used_sequences = set()
    for existing in month_dir.iterdir():
        match = SESSION_ID_RE.fullmatch(existing.name)
        if match and match.group("date") == f"{today:%Y%m%d}" and existing.is_dir():
            used_sequences.add(int(match.group("sequence")))
    sequence = 1
    while sequence in used_sequences:
        sequence += 1

    session_id = SESSION_ID_TEMPLATE.format(
        delivery=compact_delivery(arguments.delivery),
        date=f"{today:%Y%m%d}",
        sequence=sequence,
        slug=arguments.slug,
    )
    session_dir = month_dir / session_id
    if session_dir.exists():
        print(f"error: {session_dir} already exists", file=sys.stderr)
        return 1
    session_dir.mkdir()

    session_document = {
        "schema_version": 1,
        "id": session_id,
        "repository": arguments.repository,
        "delivery": arguments.delivery,
        "started_at": f"{today:%Y-%m-%d}",
        "ended_at": None,
        "time_precision": "date",
        "status": "in_progress",
        "fidelity": "exact",
        "agents": [{"id": "A001", "role": "TODO: agent role"}],
        "milestones": [],
        "work_items": [],
        "base_revision": git_revision(repo_root),
        "final_revision": None,
        "event_log": "events.jsonl",
        "summary": "summary.md",
        "notes": "notes.md",
    }
    (session_dir / "session.json").write_text(
        json.dumps(session_document, indent=2) + "\n", encoding="utf-8"
    )

    first_event = {
        "schema_version": 1,
        "seq": 1,
        "timestamp": f"{today:%Y-%m-%d}",
        "type": "objective",
        "actor": "A001",
        "content": "TODO: state the repository objective of this session.",
    }
    (session_dir / "events.jsonl").write_text(
        json.dumps(first_event, ensure_ascii=False) + "\n", encoding="utf-8"
    )

    (session_dir / "summary.md").write_text(
        f"# Session Summary\n\n"
        f"## Objective and outcome\n\n"
        f"TODO: objective and resulting repository state for {session_id}.\n\n"
        f"## Durable changes\n\n"
        f"- `path`: TODO.\n\n"
        f"## Verification\n\n"
        f"| Command/gate | Result |\n"
        f"| --- | --- |\n"
        f"| TODO | TODO |\n\n"
        f"## Cleanup\n\n"
        f"- Removed: TODO or none.\n"
        f"- Retained: TODO or none.\n\n"
        f"## Decisions and experience\n\n"
        f"- TODO: canonical decision links and experience IDs.\n\n"
        f"## Distillation\n\n"
        f"- Distilled: TODO at session end (or none).\n\n"
        f"## Unresolved items\n\n"
        f"- TODO: work ID, blocker, next action.\n\n"
        f"## Handoff\n\n"
        f"TODO: first command and minimum reading for the next agent.\n",
        encoding="utf-8",
    )

    (session_dir / "notes.md").write_text(
        f"# Notes\n\nTODO: session-specific detail, findings, and open questions.\n",
        encoding="utf-8",
    )

    index_path = sessions_root / "README.md"
    index_row = (
        f"| [{session_id}]({today:%Y/%m}/{session_id}/summary.md) "
        f"| {today:%Y-%m-%d} | Exact | In progress | TODO: one-line summary |"
    )
    index_text = index_path.read_text(encoding="utf-8") if index_path.is_file() else ""
    lines = index_text.splitlines()
    insert_at = 0
    for position, line in enumerate(lines):
        if line.startswith("| ["):
            insert_at = position + 1
    if insert_at == 0:
        print(
            "error: sessions README index table not found; append the row manually",
            file=sys.stderr,
        )
        return 1
    lines.insert(insert_at, index_row)
    index_path.write_text("\n".join(lines) + "\n", encoding="utf-8")

    print(f"created {session_dir.relative_to(repo_root)}")
    print(f"index row appended to {index_path.relative_to(repo_root)}")
    print("next: fill the objective event, update session.json agents/milestones,")
    print("      record decisions and results as events, then run:")
    print("      python3 tools/check-agent-records.py .")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
