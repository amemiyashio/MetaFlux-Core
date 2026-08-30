---
id: P20260830-004
status: Recorded
captured: 2026-08-30
milestone: M0100
branch: main
git_revision: 1ecdfb01497610c5042e12bda4a16839d2b9c734
workspace: governance content committed; S0100-20260830-005-semantic-change-distillation remains active; SC0001 not yet activated; existing guidance remains untracked
---

# Semantic change and distillation governance

## Engineering state

Revision `1ecdfb01497610c5042e12bda4a16839d2b9c734` establishes D0025, independent
`SCNNNN` records, and the `govern-semantic-change` and
`distill-project-knowledge` workflows. The protected-history gate reads only
complete Active SC authorization already in `HEAD`; the record gate separately
validates the exact staged tree. No historical record has been edited under the
new policy yet, and SC0001 remains the next phase.

## Verification evidence

| Gate | Result | Artifact/log |
| --- | --- | --- |
| Dev configure and architecture CTest | 6/6 passed | External `.metaflux-build` tree; not retained as evidence |
| Agent validator | Passed on working and staged trees | `tools/check-agent-records.py` |
| Agent validator self-test | 132 cases passed | `tools/test-check-agent-records.py` |
| Protected-history edit self-test | 21/21 passed | `tools/test-semantic-change-edits.py` |
| Session-guidance self-test | 17/17 passed | `session-guidance/scripts/test_guidance.py` |
| Skill routing | 81-case corpus and 34/34 self-test passed | `agent/skills/trigger-evals.json` |
| New skill packages | 2/2 quick validation and two independent forward tests passed | `agent/skills/` |

## Decisions and durable outcomes

- [D0025](../../../memory/decisions-index.md) owns decision-authorized semantic
  synchronization; D0024 retains product SemVer and delivery-coordinate meaning.
- Git preserves prior checkouts. SC records authorize migration and retain a
  compact reminder; they are not worktree snapshots or duplicate specifications.
- Raw timestamps, commands, output, counts, revisions, hashes, provenance, and
  observations stay locked when current interpretation changes.

## Open work and risks

- SC0001 must enumerate every exact terminal-session/checkpoint path before the
  first protected-history edit and must be committed separately as Active.
- Other active session owners receive transient guidance; their records and
  guidance disposition remain under their ownership.

## Resume notes

1. Read D0025 and `agent/skills/govern-semantic-change/references/protocol.md`.
2. Create SC0001 with the audited historical inventory and publish active-session handoffs.
3. Commit authorization before migrating any protected file; rerun the Agent and architecture gates.
