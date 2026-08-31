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
- Read agent/progress/focus.json and resume its exact owner/Exit Gate before selecting durable work.
- Scaffold a session before durable work when needed: python3 tools/new-session.py <MAJOR.MINOR.PATCH.WORK> <slug>; scaffolding does not claim execution focus.
- Content commits declare METAFLUX_SESSION_ID equal to the focus owner; focus handoff is an atomic record-only close/install.
- Existing checkpoints and terminal sessions are protected; use the committed D0025/SC route for an authorized semantic synchronization.
- Follow the matching procedure in agent/skills/ when one exists.
- Verify with: python3 tools/check-agent-records.py .
"""

if __name__ == "__main__":
    sys.stdout.write(BANNER)
    raise SystemExit(0)
