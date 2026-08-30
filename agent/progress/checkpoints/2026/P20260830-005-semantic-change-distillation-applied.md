---
id: P20260830-005
status: Recorded
captured: 2026-08-30
milestone: M0100
branch: main
git_revision: 90c45eda2815c59617fffea581522b7ed6bff1c0
workspace: SC0001 applied and S0100-20260830-005-semantic-change-distillation closed; G001/G002/G003 remain transient under their active target-session owners
---

# Semantic change and distillation applied

## Engineering state

Revision `90c45eda2815c59617fffea581522b7ed6bff1c0` completes SC0001's
evidence-preserving migration. All 75 inventoried surfaces are resolved: 71 are
Migrated and four whole-file historical surfaces remain explicit Retained
evidence. The 31 authorized protected files use current D0025 interpretation,
terminal post-cutoff summaries require `Promoted` and `Session-only` rows, and
the Applied SC no longer authorizes history edits.

## Verification evidence

| Gate | Result | Artifact/log |
| --- | --- | --- |
| Dev configure and architecture CTest | 6/6 passed | External `.metaflux-build` tree; not retained as evidence |
| Agent validator | Working and staged trees passed | `tools/check-agent-records.py` |
| Agent validator self-test | 136 cases passed | `tools/test-check-agent-records.py` |
| Protected-history edit self-test | 21/21 passed | `tools/test-semantic-change-edits.py` |
| Session-guidance self-test | 17/17 passed | `session-guidance/scripts/test_guidance.py` |
| Skill routing | 81-case corpus and 34/34 self-test passed | `agent/skills/trigger-evals.json` |
| Locked evidence | Four Retained evidence blob hashes unchanged | `SC0001` inventory |

## Decisions and durable outcomes

- [D0025](../../../memory/decisions-index.md) remains the authority for future
  breaking replacements; each replacement needs a new independent SC.
- SC0001 binds its result to `90c45ed`; Git retains all prior wording without a
  repository snapshot archive.
- Terminal summaries now separate promoted claims from intentionally
  session-only material, while reusable unverified methods remain Candidate.

## Open work and risks

- G001 and G003 remain published to the two active M0100 session owners; the
  pre-existing G002 remains untouched. Those owners validate, disposition, and
  remove their transient packets before session closure.
- Intel host, physical NVIDIA binding performance, and native NixOS
  qualification remain the `v0.2.0` support-expansion boundary.

## Resume notes

1. Read `agent/progress/current.md`; SC0001 is evidence, not an active permit.
2. Process active-session guidance only as the owning session and in numeric order.
3. Use a new decision-bound SC for any later breaking semantic replacement.
