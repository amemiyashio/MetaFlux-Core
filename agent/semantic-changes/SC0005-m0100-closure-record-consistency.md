---
id: SC0005
status: Active
created: 2026-08-30
updated: 2026-08-30
decision: D0025
session: S0100-20260830-010-m0100-closure-consistency
scope: m0100-closure-record-consistency
history_sync: automatic
effective_revision: null
superseded_by: null
---

# SC0005: M0100 closure-record consistency

## Semantic replacement

- Old meaning: post-close commits `9d770d4` and `1ac597b` directly changed a
  recorded checkpoint and terminal-session summaries without an Active SC,
  replaced a capture-time validation count with a later count, left G010 event
  26 outside the milestone mapping, retained obsolete lifecycle prose, and
  described five D0028-era human-identity commits as predating D0028.
- New meaning: M0100 remains Complete at its recorded product revisions while
  its closure records distinguish capture-time facts from later corrections.
  P010-P012 remain historical evidence, P013 is the additive current
  correction, event 26 is mapped, every live session reference resolves, and
  the five immutable commits record actual `amamiya` Git roles despite their
  user-confirmed intended `zcode` harness subject.
- Authority: D0025 in `docs/architecture/semantic-change-governance.md` owns
  exact protected-history synchronization; D0028 in
  `docs/architecture/agent-harness-commit-identity.md` owns the identity
  interpretation, and the user confirmed `zcode` as the intended subject.
- Compatibility consequence: product evidence and existing Git objects do not
  change. Current readers receive one truthful closure state, and future
  record-only commits require an index-visible in-progress session or an exact
  terminal-session closing transition.

## Migration inventory

| Surface | Class | Disposition | Evidence |
| --- | --- | --- | --- |
| `AGENTS.md` | Current | Pending | Session gate wording matches the all-durable-change rule |
| `agent/memory/decisions-index.md` | Current | Pending | Completed M0100 sources no longer say Active |
| `agent/progress/README.md` | Current | Pending | Latest checkpoint resolves to additive P013 |
| `agent/progress/current.md` | Current | Pending | No terminal session is described as awaiting closure |
| `agent/progress/checkpoints/2026/P20260830-010-m0100-completion.md` | Historical | Retained evidence | Preserve release, identity, count, and revision facts byte-for-byte |
| `agent/progress/checkpoints/2026/P20260830-011-m0100-final-closure.md` | Historical | Retained evidence | Preserve the premature closure claim as historical evidence |
| `agent/progress/checkpoints/2026/P20260830-012-m0100-correction.md` | Historical | Pending | Restore its committed capture form; P013 owns the later correction |
| `agent/progress/checkpoints/2026/P20260830-013-m0100-closure-consistency.md` | Current | Pending | Current-template additive correction names the migration revision |
| `agent/semantic-changes/README.md` | Current | Pending | SC0005 lifecycle is indexed |
| `agent/sessions/README.md` | Current | Pending | Migration session lifecycle and terminal outcomes stay synchronized |
| `agent/sessions/2026/08/S0100-20260828-013-m0100-foundation/session.json` | Historical | Retained evidence | Preserve terminal lifecycle and final content revision byte-for-byte |
| `agent/sessions/2026/08/S0100-20260828-013-m0100-foundation/events.jsonl` | Historical | Retained evidence | Preserve all 17 implementation and guidance events byte-for-byte |
| `agent/sessions/2026/08/S0100-20260828-013-m0100-foundation/summary.md` | Historical | Pending | Correct the duplicated session ID without changing implementation evidence |
| `agent/sessions/2026/08/S0100-20260830-001-m0100-completion-sprint/session.json` | Historical | Pending | Map existing G010 event 26; lock every other field |
| `agent/sessions/2026/08/S0100-20260830-001-m0100-completion-sprint/events.jsonl` | Historical | Retained evidence | Preserve seq 24/26 and every event byte; P013 corrects the forward identity assertion |
| `agent/sessions/2026/08/S0100-20260830-001-m0100-completion-sprint/summary.md` | Historical | Pending | Preserve capture-time 26/201 count and distinguish later correction evidence |
| `agent/sessions/2026/08/S0100-20260830-001-m0100-completion-sprint/notes.md` | Historical | Pending | Replace the terminal TODO with a bounded pointer to canonical closure evidence |
| `.githooks/pre-commit` | Tooling | Pending | Candidate-index session coverage blocks agent-only bypasses |
| `tools/check-agent-records.py` | Tooling | Pending | Full session references, terminal notes, and terminal guidance-event mapping are checked |
| `tools/test-check-agent-records.py` | Tooling | Pending | Focused regressions cover every new record and hook invariant |
| `tools/README.md` | Tooling | Pending | Validator and session-gate behavior is documented |
| `agent/sessions/2026/08/S0100-20260830-010-m0100-closure-consistency/session.json` | Active session | Pending | Final lifecycle binds the migration revision |
| `agent/sessions/2026/08/S0100-20260830-010-m0100-closure-consistency/events.jsonl` | Active session | Pending | Objective, authorization, verification, and application remain compact |
| `agent/sessions/2026/08/S0100-20260830-010-m0100-closure-consistency/summary.md` | Active session | Pending | Terminal summary follows record-session and roast contracts |
| `agent/sessions/2026/08/S0100-20260830-010-m0100-closure-consistency/notes.md` | Active session | Pending | Locked facts and failed-route lesson remain bounded |

## Active-session handoff

| Session | Guidance | Status | Outcome |
| --- | --- | --- | --- |
| none | none | Not required | No affected recipient other than the migration owner |

## Evidence preservation

Lock P010-P012 capture-time text, the completion session's event log, the
original `26 sessions / 201 events` validation row, all product test results,
release revisions `0feac1d` and `9dc7636`, tree `e8362666`, timestamps, hashes,
commands, outputs, cleanup observations, and every existing Git Author and
Committer. The five commits named above remain byte-for-byte Git objects;
P013 records intended `zcode` versus actual `amamiya` without rewriting them.
Commits `9d770d4` and `1ac597b` also remain evidence of the unauthorized route.

## Future-agent reminder

Ordinary closure corrections append a current-template checkpoint. Before any
terminal-session or recorded-checkpoint edit, commit an Active SC containing
that exact Historical path. A forward identity assertion is accepted only when
Git confirms it; otherwise preserve the commit and append intended versus
actual identity facts.

## Verification

| Gate | Result |
| --- | --- |
| Active authorization | Pending |
| Protected-history diff | Pending |
| Exact residual searches | Pending |
| Agent records and self-tests | Pending |
| Semantic-change edit gate | Pending |
| Guidance and harness identity | Pending |
| Git diff and identity audit | Pending |
