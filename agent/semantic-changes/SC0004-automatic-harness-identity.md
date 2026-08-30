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
  helper accepts a generic harness-owned provenance declaration without
  inspecting `/proc`; otherwise it correlates generic environment signals with
  Linux process ancestry. It then generates Author and Committer from the
  normalized subject without a product mapping table or per-product selector.
- Authority: D0028 in
  `docs/architecture/agent-harness-commit-identity.md`.
- Compatibility consequence: future agent commits use the generated
  `Agent Harness (<subject>) <<subject>@localhost>` identity; human Git
  configuration and every existing commit object remain unchanged.

## Migration inventory

| Surface | Class | Disposition | Evidence |
| --- | --- | --- | --- |
| `docs/architecture/agent-harness-commit-identity.md` | Current | Migrated | D0028 is Verified by generic runtime derivation, isolation tests, and the automatic content-commit gate |
| `docs/architecture/README.md` | Current | Migrated | Current index resolves the Verified D0028 owner and SC0004 migration |
| `agent/memory/decisions-index.md` | Current | Migrated | D0028 resolves to the Verified repository workflow |
| `agent/semantic-changes/README.md` | Current | Migrated | Index resolves SC0004 and retains Active status until the record commit |
| `agent/skills/start-work/SKILL.md` | Current | Migrated | Canonical workflow requires automatic runtime subject derivation and rejects product selection |
| `agent/skills/start-work/scripts/commit_as_harness.py` | Tooling | Migrated | Static table removed; generic declaration or corroborated environment/ancestry derives identity |
| `agent/skills/start-work/scripts/test_commit_as_harness.py` | Tooling | Migrated | Seven isolated cases pass unseen-harness, selection, rejection, Git identity, and isolation paths |
| `agent/sessions/README.md` | Current | Migrated | S0100-20260830-008-agent-harness-commit-identity is explicitly the initial static implementation superseded by D0028/SC0004 |
| `agent/progress/current.md` | Current | Retained evidence | HEAD blob `7dc3e420aca64caa8b2adb6f7a73b02e21970818` states the generic invariant and factual `ded1dad`; concurrent P20260830-010 remains outside this migration |
| `agent/progress/checkpoints/2026/P20260830-009-agent-harness-commit-identity.md` | Historical | Migrated | Static authority and resume route contextualized; revision, identity, configuration, and gate evidence unchanged |
| `agent/sessions/2026/08/S0100-20260830-008-agent-harness-commit-identity/summary.md` | Historical | Migrated | Initial static implementation contextualized and handoff replaced; recorded gates and revisions unchanged |
| `agent/sessions/2026/08/S0100-20260830-008-agent-harness-commit-identity/notes.md` | Historical | Migrated | Capture-time identities contextualized; command-local and protected-option observations retained |
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
the S0100-20260830-008-agent-harness-commit-identity event log, all recorded
test counts and outcomes, the human Git configuration observation, and every
cited revision. Historical prose may only distinguish that initial static
implementation from the current D0028 rule.

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
| Active authorization revision | Passed at `0a2c0c4242b754254a6806b0d9c8584b17765d37` before protected edits |
| Automatic runtime behavior | Seven of seven isolated cases passed; current runtime resolved `Agent Harness (codex) <codex@localhost>` without a selector |
| Historical evidence lock and residual search | Passed pre-commit audit: retained event blob is `5b16d08e62765e483aa715bded55e954ba63794d`; no live static `HARNESSES` table remains |
| Agent records, semantic edit gate, skill validation, and routing | Pending final gates |
| Real automatic commit | Pending content commit without a fixed harness argument |
