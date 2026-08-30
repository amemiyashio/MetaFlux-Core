---
id: SC0004
status: Active
created: 2026-08-30
updated: 2026-08-30
decision: D0028
session: S0100-20260830-009-automatic-harness-identity
scope: automatic-harness-identity
history_sync: automatic
effective_revision: null
superseded_by: null
---

# SC0004: automatic harness identity

## Semantic replacement

- Old meaning: `start-work` owned a static Codex and Claude Code identity table;
  commit `f862852` later extended that table with ZCode, so every previously
  unknown harness required a repository code change or a fixed CLI selection.
- New meaning: D0028 makes the runtime harness subject authoritative. The
  helper reads generic harness environment signals and Linux process ancestry,
  then generates Author and Committer from the normalized subject without any
  product mapping table or per-product CLI selector.
- Authority: D0028 in
  `docs/architecture/agent-harness-commit-identity.md`.
- Compatibility consequence: future agent commits use the generated
  `Agent Harness (<subject>) <<subject>@localhost>` identity; human Git
  configuration and every existing commit object remain unchanged.

## Migration inventory

| Surface | Class | Disposition | Evidence |
| --- | --- | --- | --- |
| `docs/architecture/agent-harness-commit-identity.md` | Current | Pending | Verify D0028 becomes Verified only with generic runtime and real commit evidence |
| `docs/architecture/README.md` | Current | Pending | Replace the Proposed/Active pointer after SC0004 application |
| `agent/memory/decisions-index.md` | Current | Pending | D0028 source status must match verified implementation state |
| `agent/semantic-changes/README.md` | Current | Pending | SC0004 index status must match this record |
| `agent/skills/start-work/SKILL.md` | Current | Pending | Remove fixed identities and manual harness selection from the canonical workflow |
| `agent/skills/start-work/scripts/commit_as_harness.py` | Tooling | Pending | Remove `HARNESSES`; read and normalize runtime subject without product branches |
| `agent/skills/start-work/scripts/test_commit_as_harness.py` | Tooling | Pending | Prove unseen harnesses, ancestry selection, rejection paths, Git identity, and configuration isolation |
| `agent/sessions/README.md` | Current | Pending | Mark S0100-20260830-008-agent-harness-commit-identity as the initial static implementation later superseded by D0028/SC0004 |
| `agent/progress/current.md` | Current | Retained evidence | Existing text states the generic active-harness invariant and factual `ded1dad` result; the concurrent P20260830-010 hunk remains outside this migration |
| `agent/progress/checkpoints/2026/P20260830-009-agent-harness-commit-identity.md` | Historical | Pending | Replace only static-mapping authority and resume instructions; lock revision, identity, and verification facts |
| `agent/sessions/2026/08/S0100-20260830-008-agent-harness-commit-identity/summary.md` | Historical | Pending | Contextualize the initial static implementation and replace its current handoff; lock recorded gates and revisions |
| `agent/sessions/2026/08/S0100-20260830-008-agent-harness-commit-identity/notes.md` | Historical | Pending | Replace fixed identities and manual selection as current advice; retain first-implementation observations |
| `agent/sessions/2026/08/S0100-20260830-008-agent-harness-commit-identity/events.jsonl` | Historical | Retained evidence | Preserve byte-for-byte event evidence at Git blob `5b16d08e62765e483aa715bded55e954ba63794d` |
| `agent/sessions/2026/08/S0100-20260830-009-automatic-harness-identity/session.json` | Active session | Pending | Close only after migration content and effective revision exist |
| `agent/sessions/2026/08/S0100-20260830-009-automatic-harness-identity/events.jsonl` | Active session | Pending | Record authorization, guidance publication, migration evidence, and application |
| `agent/sessions/2026/08/S0100-20260830-009-automatic-harness-identity/summary.md` | Active session | Pending | Record final D0028/SC0004 outcome, roast, cleanup, and handoff |
| `agent/sessions/2026/08/S0100-20260830-009-automatic-harness-identity/notes.md` | Active session | Pending | Record locked-evidence and automatic detection audit |

## Active-session handoff

| Session | Guidance | Status | Outcome |
| --- | --- | --- | --- |
| `S0100-20260828-013-m0100-foundation` | `G004` | Published | Re-read migrated start-work at the next commit boundary; do not add another fixed harness mapping |
| `S0100-20260830-001-m0100-completion-sprint` | `G007` | Published | Re-read migrated start-work at the next commit boundary; do not add another fixed harness mapping |

## Evidence preservation

Existing Git objects remain immutable. In particular, preserve the Author and
Committer recorded by `ded1dad4172b515a3b17cb66c3f7aa18df9cb20e` and
`f862852b28e5471b6533ac6754b08c3ca86cbbf4`, their subjects and timestamps,
the S0100-20260830-008-agent-harness-commit-identity event log, all recorded test counts and outcomes,
the human Git configuration observation, and every cited revision. Historical
prose may only distinguish that initial static implementation from the current
D0028 rule.

## Future-agent reminder

Never add a Codex, Claude Code, ZCode, or other product row to the commit helper.
Read the active runtime harness subject, validate and normalize it, derive both
Git roles from that subject, and stop on missing or ambiguous evidence without
falling back to human Git configuration.

## Verification

| Gate | Result |
| --- | --- |
| User decision authority | Passed: the user explicitly classified the replacement as a governance action and required automatic harness-subject identity |
| Active-session handoff | G004 and G007 published to both affected in-progress sessions |
| Active authorization revision | Pending activation commit |
| Automatic runtime behavior | Pending implementation and isolated forward tests |
| Historical evidence lock and residual search | Pending migration audit |
| Agent records, semantic edit gate, skill validation, and routing | Pending final gates |
| Real automatic commit | Pending content commit without a fixed harness argument |
