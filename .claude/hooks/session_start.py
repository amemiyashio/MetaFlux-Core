#!/usr/bin/env python3
"""Claude Code SessionStart banner for MetaFlux-Core (repository-local).

stdout from a SessionStart hook is added to the session context. Keep it
short: the point is to route the agent into AGENTS.md before it touches
anything.
"""

import sys

BANNER = """\
MetaFlux-Core rules apply in this repository:
- Read AGENTS.md at the repository root before making any change.
- Scaffold a session before editing anything outside agent/: python3 tools/new-session.py <MAJOR.MINOR.PATCH.WORK> <slug>
- agent/progress/checkpoints/ is immutable history; never rewrite it.
- Follow the matching procedure in agent/skills/ when one exists.
- Verify with: python3 tools/check-agent-records.py .
"""

if __name__ == "__main__":
    sys.stdout.write(BANNER)
    raise SystemExit(0)
