# Summary

Added the Claude Code bridge, strictly repository-local, so Claude sees the
same rules and hits the same edit-time enforcement as every other agent —
without touching global configuration or the CLI, and without affecting
contributors who use other tools.

- **`CLAUDE.md`** is a one-line `@AGENTS.md` import (Claude Code expands
  @-imports natively), so the entry files share one rulebook and cannot drift.
  The validator requires that import line when `CLAUDE.md` exists and rejects
  a malformed `.claude/settings.json`; both rules are inert when the bridge is
  absent.
- **`.claude/settings.json`** registers two project-scoped hooks: SessionStart
  prints the onboarding banner; PreToolUse
  (`.claude/hooks/pre_edit.py`) blocks rewrites of
  `agent/progress/checkpoints/` (immutable history) and any edit outside
  `agent/` while no session is in progress — each denial carrying its
  remediation command.
- **Scope guarantees** are documented in `.claude/README.md` and AGENTS.md:
  nothing writes to `~/.claude`; the bridge is inert for non-Claude tools;
  Claude Code asks each user to approve project hooks; the guard fails open
  on schema drift, leaving the pre-commit gate and Nix checks as the hard,
  tool-agnostic gates.

All six guard paths were verified live with stdin JSON payloads before
committing, including the deny-then-scaffold-then-allow sequence.

## Changed paths

- `CLAUDE.md`: new Claude entry point (import form).
- `.claude/settings.json`, `.claude/hooks/{pre_edit,session_start}.py`,
  `.claude/README.md`: bridge, guards, and scope documentation.
- `AGENTS.md`: bridge note with scope guarantees.
- `tools/check-agent-records.py`: entry-point bridge rules.
- `tools/test-check-agent-records.py`: three new cases (23 total).

## Verification

| Gate | Result |
| --- | --- |
| Guard: checkpoint edit | Denied, exit 2 |
| Guard: non-agent edit, no session | Denied with scaffold command |
| Guard: same edit after scaffolding | Allowed |
| Guard: agent/ edit / malformed stdin | Allowed (fail-open) |
| Banner | Printed |
| Self-test / records / dev preset | 23/23, ok (8 sessions), 16/16 |

## Decisions and experience

- No new DNNNN decisions: the bridge is workflow tooling documented in its own
  README and enforced by the existing validator.

## Distillation

- Distilled: the @-import single-source pattern and the fail-open guard
  design into `.claude/README.md`. No experience records.

## Unresolved items

- None blocking.

## Handoff

First command: `python3 tools/check-agent-records.py .`. Claude users read
[CLAUDE.md](../../../../../CLAUDE.md); everyone reads
[AGENTS.md](../../../../../AGENTS.md).
