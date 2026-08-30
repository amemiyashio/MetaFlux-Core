---
id: P20260830-006
status: Recorded
captured: 2026-08-30
milestone: M0100
branch: main
git_revision: 21f1d47cd0522682e4047542fc53627bff2967fe
workspace: D0026 proposed and indexed; S0100-20260830-006-project-knowledge-roast remains active; SC0002 not yet activated
---

# Project knowledge roast decision

## Engineering state

Revision `21f1d47cd0522682e4047542fc53627bff2967fe` establishes D0026 as the
canonical replacement design. Only promoted durable claims enter the ordered
`light roasts`, `medium roasts`, or `dark roasts` depths; `session-only` is an
independent disposition. The `$roast` package, summary schema, tooling, active
handoffs, and historical migration have not yet changed.

## Verification evidence

| Gate | Result | Artifact/log |
| --- | --- | --- |
| Agent validator | Passed: 27 sessions, 206 events, 202 Markdown files | `tools/check-agent-records.py` |
| Working-tree hygiene | Passed | `git diff --check` |

## Decisions and durable outcomes

- [D0026](../../../memory/decisions-index.md) uses roast depth only for semantic
  transformation, independently of evidence maturity and importance.
- Every dark roast resolves to a decision; a breaking dark roast additionally
  requires the D0025/SC workflow.
- `$roast` is explicit-only. `start-work` and `record-session` compose it at
  material boundaries, while ordinary code critique does not trigger it.

## Open work and risks

- SC0002 must enumerate and authorize every protected historical surface before
  the first history edit.
- Both existing in-progress sessions need transient guidance and owner-side
  migration before the strict summary gate lands.

## Resume notes

1. Create and commit SC0002 Active with the exact current/tooling/historical inventory.
2. Fix guidance ID reservation before publishing the active-session handoffs.
3. Migrate current interfaces, tooling, active owners, and protected history; then verify and apply SC0002.
