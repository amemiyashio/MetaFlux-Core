#!/usr/bin/env python3
"""Claude Code PreToolUse guard for MetaFlux-Core (repository-local).

Blocks the edit tools before a rule-skipping change lands:

- agent/progress/checkpoints/** is immutable history (append a correction
  instead of rewriting);
- any edit outside agent/ requires an in-progress session record.

Fails open on any parse or environment error: this guard is a convenience
bridge, and the pre-commit hook plus repository validators and owner-specific
tests remain the hard gates for every contributor regardless of tooling.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

DENIED_MARKER = "MetaFlux guard:"


def deny(message: str) -> int:
    print(f"{DENIED_MARKER} {message}", file=sys.stderr)
    return 2


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
    try:
        relative = target.resolve().relative_to(repo_root)
    except ValueError:
        return 0  # outside this repository; not our jurisdiction

    parts = relative.parts
    if parts[:3] == ("agent", "progress", "checkpoints"):
        return deny(
            "checkpoints are immutable history. Record a correction in the "
            "current session notes and, if material, a new checkpoint; never "
            "rewrite a recorded one."
        )

    if parts and parts[0] != "agent":
        sessions_root = repo_root / "agent" / "sessions"
        for session_file in sessions_root.glob("*/*/*/session.json"):
            try:
                document = json.loads(session_file.read_text(encoding="utf-8"))
            except Exception:
                continue
            if isinstance(document, dict) and document.get("status") == "in_progress":
                return 0
        return deny(
            "changes outside agent/ require an in-progress session. Scaffold "
            "one first: python3 tools/new-session.py "
            "<MAJOR.MINOR.PATCH.WORK> <slug>  (see AGENTS.md)"
        )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
