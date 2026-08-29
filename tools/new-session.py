#!/usr/bin/env python3
"""Scaffold a MetaFlux project work session.

Allocates the next SYYYYMMDD-NNN id for today, creates the session directory
with a validator-clean skeleton (session.json, events.jsonl, summary.md,
notes.md), appends the index row to agent/sessions/README.md, and prints the
next steps. Run from the repository root:

    python3 tools/new-session.py my-session-slug

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

SESSION_ID_TEMPLATE = "{date}-{sequence:03d}"
SESSION_ID_RE = re.compile(r"^S(\d{8})-(\d{3})-[a-z0-9][a-z0-9-]*$")
SLUG_RE = re.compile(r"^[a-z0-9][a-z0-9-]*$")


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

    if not SLUG_RE.fullmatch(arguments.slug):
        print(f"error: slug must match {SLUG_RE.pattern}: {arguments.slug!r}", file=sys.stderr)
        return 1

    repo_root = arguments.repo_root.resolve()
    sessions_root = repo_root / "agent" / "sessions"
    if not sessions_root.is_dir():
        print(f"error: {sessions_root} does not exist", file=sys.stderr)
        return 1

    today = dt.date.today()
    month_dir = sessions_root / f"{today:%Y}" / f"{today:%m}"
    month_dir.mkdir(parents=True, exist_ok=True)

    used_sequences = set()
    for existing in month_dir.iterdir():
        match = SESSION_ID_RE.fullmatch(existing.name)
        if match and match.group(1) == f"{today:%Y%m%d}" and existing.is_dir():
            used_sequences.add(int(match.group(2)))
    sequence = 1
    while sequence in used_sequences:
        sequence += 1

    session_id = f"S{SESSION_ID_TEMPLATE.format(date=f'{today:%Y%m%d}', sequence=sequence)}-{arguments.slug}"
    session_dir = month_dir / session_id
    if session_dir.exists():
        print(f"error: {session_dir} already exists", file=sys.stderr)
        return 1
    session_dir.mkdir()

    session_document = {
        "schema_version": 1,
        "id": session_id,
        "repository": arguments.repository,
        "started_at": f"{today:%Y-%m-%d}",
        "ended_at": f"{today:%Y-%m-%d}",
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
        f"# Summary\n\n"
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
