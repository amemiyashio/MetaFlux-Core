---
id: P20260830-002
date: 2026-08-30
status: Recorded
revision: 6ddd907
trigger: verified session-local specialist guidance and transient cleanup loop
---

# Session-local specialist guidance

## Outcome

Revision `6ddd907` adds a repository-local skill for bounded specialist
direction. Guidance is temporary input to one active session, not a second
source or evidence archive. The session owner retains implementation and
verification responsibility and preserves only the compact disposition.

## Enforced boundaries

- Packets move atomically through `draft`, `ready`, and `processing`, then are
  removed after a verified material disposition or explicit no-material result.
- Interrupted creation has exact, session-contained recovery; malformed event
  logs, symlink escapes, duplicate dispositions, and ambiguous cleanup retain
  the packet instead of deleting it.
- Raw packet paths cannot enter Git through add, modify, copy, rename, or type
  change. Active input is isolated from durable-record scans, and terminal
  inboxes must be empty.
- The matching domain skill, current source, tests, user direction, and
  canonical constraints remain authoritative.

## Verification

| Gate | Result |
| --- | --- |
| Guidance CLI self-test | 16/16 passed |
| Agent-record validator self-test | 81/81 passed |
| Full dev CTest | 63/63 passed |
| Skill validation and routing | Passed |
| `nix flake check path:. -L` | Passed |
| Independent forward use and targeted review | Passed; no residual finding |

## Cleanup

All isolated test repositories, transient packet/patch files, and temporary
validator output were removed. No session guidance inbox or session-owned
generated artifact remains. The shared external dev build tree was preserved.

## Handoff

At the next guidance boundary, list ready packets with
`python3 agent/skills/session-guidance/scripts/guidance.py list --session SESSION`.
Load `session-guidance` only for a ready packet or an explicit publish/process
request.
