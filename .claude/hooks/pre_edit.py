#!/usr/bin/env python3
"""Claude Code PreToolUse guard for MetaFlux-Core (repository-local).

Blocks the edit tools before a rule-skipping change lands:

- existing checkpoints and terminal-session files require an exact Historical
  row in a committed Active SC; new checkpoints remain writable;
- any edit outside agent/ requires one resolvable in-progress execution-focus
  owner; an explicit METAFLUX_SESSION_ID must match that owner.

Malformed hook input and paths outside the repository fail open. Missing or
malformed repository focus state fails closed for edits in scope. This guard is
a convenience bridge; the pre-commit hook plus repository validators and
owner-specific tests remain the hard gates for every contributor.
"""

from __future__ import annotations

import json
import os
import re
import subprocess
import sys
from pathlib import Path

DENIED_MARKER = "MetaFlux guard:"
CURRENT_SESSION_SCHEMA_VERSION = 2
CURRENT_FOCUS_SCHEMA_VERSION = 2
EXECUTION_GOVERNANCE_EPOCH = "D0029"
SESSION_ID_RE = re.compile(
    r"^S\d{4,}-(?P<year>\d{4})(?P<month>\d{2})\d{2}-"
    r"\d{3}-[a-z0-9][a-z0-9-]*$"
)


def deny(message: str) -> int:
    print(f"{DENIED_MARKER} {message}", file=sys.stderr)
    return 2


def execution_focus_owner(repo_root: Path) -> str | None:
    focus_path = repo_root / "agent" / "progress" / "focus.json"
    try:
        focus = json.loads(focus_path.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError):
        return None
    if not isinstance(focus, dict):
        return None
    if (
        focus.get("schema_version") != CURRENT_FOCUS_SCHEMA_VERSION
        or focus.get("governance_epoch") != EXECUTION_GOVERNANCE_EPOCH
    ):
        return None
    owner = focus.get("owner_session")
    if not isinstance(owner, str):
        return None
    match = SESSION_ID_RE.fullmatch(owner)
    if match is None:
        return None
    session_path = (
        repo_root
        / "agent"
        / "sessions"
        / match.group("year")
        / match.group("month")
        / owner
        / "session.json"
    )
    try:
        session = json.loads(session_path.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError):
        return None
    if (
        not isinstance(session, dict)
        or session.get("schema_version") != CURRENT_SESSION_SCHEMA_VERSION
        or session.get("governance_epoch") != EXECUTION_GOVERNANCE_EPOCH
        or session.get("id") != owner
        or session.get("status") != "in_progress"
        or session.get("ended_at") is not None
    ):
        return None
    return owner


def main() -> int:
    try:
        payload = json.load(sys.stdin)
    except Exception:
        return 0

    tool_input = payload.get("tool_input")
    if not isinstance(tool_input, dict):
        return 0

    raw_path = tool_input.get("file_path") or tool_input.get("notebook_path")
    if not isinstance(raw_path, str) or not raw_path:
        return 0

    repo_root = Path(payload.get("cwd") or Path.cwd()).resolve()
    target = Path(raw_path)
    if not target.is_absolute():
        target = repo_root / target
    target = Path(os.path.abspath(target))
    try:
        relative = target.relative_to(repo_root)
    except ValueError:
        return 0  # outside this repository; not our jurisdiction

    parts = relative.parts
    history_gate = repo_root / "tools" / "check-semantic-change-edits.py"
    if history_gate.is_file():
        result = subprocess.run(
            [sys.executable, str(history_gate), str(repo_root), "--path", relative.as_posix()],
            cwd=repo_root,
            check=False,
            capture_output=True,
            text=True,
        )
        if result.returncode != 0:
            detail = result.stderr.strip().removeprefix("error: ")
            return deny(detail or "protected history requires a committed Active SC")

    if parts and parts[0] != "agent":
        owner = execution_focus_owner(repo_root)
        if owner is None:
            return deny(
                "changes outside agent/ require one valid in-progress owner in "
                "agent/progress/focus.json (see AGENTS.md)"
            )
        declared_session = os.environ.get("METAFLUX_SESSION_ID")
        if declared_session and declared_session != owner:
            return deny(
                f"METAFLUX_SESSION_ID {declared_session!r} does not match "
                f"execution-focus owner {owner!r}"
            )
        return 0

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
