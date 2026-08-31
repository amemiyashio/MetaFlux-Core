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
- Require schema_version 2 and governance_epoch D0029 in agent/progress/focus.json and its exact owner before selecting durable work.
- Scaffold a new schema-2/D0029 session before durable work when needed: python3 tools/new-session.py <MAJOR.MINOR.PATCH.WORK> <slug>; scaffolding does not claim execution focus.
- Never resume, upgrade, grandfather, or fall back to a schema-1/pre-epoch session; continue an objective only through a new current-epoch successor.
- Content commits declare METAFLUX_SESSION_ID equal to the focus owner; focus handoff is an atomic record-only close/install.
- Existing checkpoints and terminal sessions are protected; use the committed D0025/SC route for an authorized semantic synchronization.
- Follow the matching procedure in agent/skills/ when one exists.
- Verify with: python3 tools/check-agent-records.py .
"""

if __name__ == "__main__":
    sys.stdout.write(BANNER)
    raise SystemExit(0)
