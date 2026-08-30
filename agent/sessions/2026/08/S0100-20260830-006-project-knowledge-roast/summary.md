# Session Summary

## Objective and outcome

Replace the pre-D0026 project-knowledge promotion model with the explicit-only
`$roast` skill, three semantic-transformation depths, and an independent
`session-only` disposition through D0026 and an SC0002-governed semantic
migration. D0026 is proposed; implementation and migration remain in progress.

## Durable changes

- `docs/architecture/project-knowledge-roast.md`: D0026 canonical replacement design.
- `agent/memory/decisions-index.md`: resolvable D0026 authority.
- `agent/skills/session-guidance/`: durable SC handoff ID reservation with 20/20 tests.
- `agent/semantic-changes/SC0002-project-knowledge-roast.md`: 68-surface Active migration authorization.

## Verification

| Command/gate | Result |
| --- | --- |
| `python3 tools/check-agent-records.py .` | Passed: 27 sessions, 206 events, 202 Markdown files |
| `git diff --check` | Passed |

## Cleanup

- Removed: TODO or none.
- Retained: TODO or none.

## Decisions and experience

- D0026 defines roast depth, the independent session-only disposition, and explicit invocation.

## roast

### light roasts

- none.

### medium roasts

- SC handoff references now reserve cleaned guidance IDs -> agent/skills/session-guidance (a2df763; 20/20 guidance tests)

### dark roasts

- D0026 project-knowledge roast contract -> docs/architecture/project-knowledge-roast.md (21f1d47 and 5b9e0d3; authority: D0026, SC0002)

## session-only

- none.

## Unresolved items

- SC0002 authorization, implementation, active-session handoff, and historical migration remain open.

## Handoff

Read D0026 and create SC0002 Active with the exact protected-history inventory before editing any terminal session or checkpoint.
