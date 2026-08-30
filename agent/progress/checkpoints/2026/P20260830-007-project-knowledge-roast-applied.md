---
id: P20260830-007
status: Recorded
captured: 2026-08-30
milestone: M0100
branch: main
git_revision: 991e5327c8a3b1f5d05112f895011f8d83f0bff0
workspace: SC0002 applied and S0100-20260830-006-project-knowledge-roast closed; G002/G004 adopted and removed; M0100 product sessions remain independently active
---

# Project knowledge roast applied

## Engineering state

Revision `991e5327c8a3b1f5d05112f895011f8d83f0bff0` is the final D0026
migration content and SC0002 effective revision; initial migration revision
`29a4e38f3a9097ea3680ed7aa98f67ed41105b2e` established the replacement, and
the final content revision tightened every promotion to one resolvable owner.
This separate record closure marks the SC Applied. The repository exposes one
explicit-only `$roast` skill, enforces
the ordered light/medium/dark promotion buckets, and keeps `## session-only` as
a separate top-level disposition. All 68 inventory surfaces are resolved: 58
are Migrated, three obsolete callable-package paths are Removed, and seven
files remain Retained evidence. SC0001 remains Applied for D0025 governance
while its pre-D0026 promotion-model consequence is replaced. Neither closed SC
authorizes later history edits.

## Verification evidence

| Gate | Result | Artifact/log |
| --- | --- | --- |
| Architecture CTest | 6/6 passed through the Nix dev shell | External `.metaflux-build` tree; not retained as evidence |
| Agent repository gate | 27 sessions, 210 events, 205 Markdown files passed | `tools/check-agent-records.py` |
| Agent validator self-test | 163/163 passed | `tools/test-check-agent-records.py` |
| Protected-history edit self-test | 21/21 passed | `tools/test-semantic-change-edits.py` |
| Session-guidance self-test | 20/20 passed | `agent/skills/session-guidance/scripts/test_guidance.py` |
| Skill routing | 82-case corpus and 34/34 self-test passed | `agent/skills/trigger-evals.json` |
| Roast package and A-E forward review | Package valid; disposition, depth, omission, and authority routes passed | `agent/sessions/2026/08/S0100-20260830-006-project-knowledge-roast/notes.md` |
| Locked evidence | Group A 17-file, Group B 14-file, and seven retained-file audits passed | Migration-session `notes.md` and SC0002 inventory |

## Decisions and durable outcomes

- [D0026](../../../memory/decisions-index.md) is a Verified repository contract;
  roast depth describes semantic transformation rather than evidence strength,
  importance, or retention time.
- `session-only` is independent of roast. A retained local artifact is also
  accounted for under session cleanup; the disposition alone does not preserve
  a file.
- An unresolved project-governance replacement never enters terminal dark roast.
  It first needs its decision and, when established meaning changes, an Active
  SC already committed to `HEAD`.

## Open work and risks

- The M0100 foundation and completion sessions remain under their own owners;
  both now have D0026 summaries and empty guidance inboxes.
- Intel host, physical NVIDIA binding performance, and native NixOS
  qualification remain the `v0.2.0` support-expansion boundary.

## Resume notes

1. Read `agent/progress/current.md`; SC0002 is evidence rather than an active permit.
2. Close the remaining M0100 sessions through their own cleanup and lifecycle records.
3. Use a new decision-bound SC for any later breaking repository-semantic replacement.
