---
id: SC0004
status: Applied
created: 2026-08-30
updated: 2026-08-30
decision: D0028
session: S0100-20260830-009-automatic-harness-identity
scope: automatic-harness-identity
history_sync: automatic
effective_revision: 0101a4a549436e6dd8f4c94175685811d37c90d0
superseded_by: null
---

# SC0004: agent-provided harness identity

## Semantic replacement

- Old meaning: `start-work` owned a static Codex and Claude Code identity table;
  commit `f862852` later extended that table with ZCode, so every previously
  unknown harness required a repository code change or a fixed CLI selection.
- New meaning: D0028 makes the active agent's self-declared harness subject
  authoritative. The agent reads and surfaces that subject from its harness
  context, then supplies it command-locally through the generic
  `METAFLUX_AGENT_HARNESS` protocol. The helper performs no `/proc`, process,
  or product-environment inference and generates both Git roles without a
  product mapping table or per-product selector.
- Authority: D0028 in
  `docs/architecture/agent-harness-commit-identity.md`.
- Compatibility consequence: future agent commits use the generated
  `Agent Harness (<subject>) <<subject>@localhost>` identity; human Git
  configuration and every existing commit object remain unchanged.

## Migration inventory

| Surface | Class | Disposition | Evidence |
| --- | --- | --- | --- |
| `docs/architecture/agent-harness-commit-identity.md` | Current | Migrated | D0028 requires agent self-declaration and explicitly forbids process/environment inference |
| `docs/architecture/README.md` | Current | Migrated | Current index resolves the Verified D0028 owner and SC0004 migration |
| `agent/memory/decisions-index.md` | Current | Migrated | D0028 resolves to the Verified repository workflow |
| `agent/semantic-changes/README.md` | Current | Migrated | Index resolves SC0004 as Applied at corrected content revision `0101a4a549436e6dd8f4c94175685811d37c90d0` |
| `agent/skills/start-work/SKILL.md` | Current | Migrated | Canonical workflow makes the agent surface its harness subject before supplying a command-local declaration |
| `agent/skills/start-work/scripts/commit_as_harness.py` | Tooling | Migrated | Static table and process/environment inference removed; the helper consumes only the generic agent declaration |
| `agent/skills/start-work/scripts/test_commit_as_harness.py` | Tooling | Migrated | Seven isolated cases cover unseen declarations, no-inference failure, validation, Git identity, and isolation paths |
| `agent/sessions/README.md` | Current | Migrated | S0100-20260830-008-agent-harness-commit-identity is explicitly the initial static implementation superseded by D0028/SC0004 |
| `agent/progress/current.md` | Current | Retained evidence | HEAD blob `7dc3e420aca64caa8b2adb6f7a73b02e21970818` states the generic invariant and factual `ded1dad`; concurrent P20260830-010 remains outside this migration |
| `agent/progress/checkpoints/2026/P20260830-009-agent-harness-commit-identity.md` | Historical | Migrated | Resume wording now requires agent self-declaration; revision, identity, configuration, and gate evidence remain unchanged |
| `agent/sessions/2026/08/S0100-20260830-008-agent-harness-commit-identity/summary.md` | Historical | Migrated | Handoff now requires agent self-declaration; recorded gates and revisions remain unchanged |
| `agent/sessions/2026/08/S0100-20260830-008-agent-harness-commit-identity/notes.md` | Historical | Migrated | Capture-time identities contextualized; command-local and protected-option observations retained |
| `agent/sessions/2026/08/S0100-20260830-008-agent-harness-commit-identity/events.jsonl` | Historical | Retained evidence | Preserve byte-for-byte event evidence at Git blob `5b16d08e62765e483aa715bded55e954ba63794d` |
| `agent/sessions/2026/08/S0100-20260830-009-automatic-harness-identity/session.json` | Active session | Migrated | Terminal lifecycle names corrected content revision `0101a4a549436e6dd8f4c94175685811d37c90d0` |
| `agent/sessions/2026/08/S0100-20260830-009-automatic-harness-identity/events.jsonl` | Active session | Migrated | Records the rejected process-inference route, user correction, corrected commit, and application |
| `agent/sessions/2026/08/S0100-20260830-009-automatic-harness-identity/summary.md` | Active session | Migrated | Maps the final self-declaration authority to D0028/SC0004 with verification, cleanup, roast, and handoff |
| `agent/sessions/2026/08/S0100-20260830-009-automatic-harness-identity/notes.md` | Active session | Migrated | Records locked evidence, the rejected inference route, self-declaration boundary, and concurrent-work exclusion |

## Active-session handoff

| Session | Guidance | Status | Outcome |
| --- | --- | --- | --- |
| `S0100-20260828-013-m0100-foundation` | `G005` | Adopted | G005 superseded by G006; foundation session terminal at 694272a |
| `S0100-20260830-001-m0100-completion-sprint` | `G008` | Adopted | G008 adopted; completion session terminal at be9a421 |

## Evidence preservation

Existing Git objects remain immutable. In particular, preserve the Author and
Committer recorded by `ded1dad4172b515a3b17cb66c3f7aa18df9cb20e` and
`f862852b28e5471b6533ac6754b08c3ca86cbbf4`, their subjects and timestamps,
the S0100-20260830-008-agent-harness-commit-identity event log, all recorded
test counts and outcomes, the human Git configuration observation, and every
cited revision. Historical prose may only distinguish that initial static
implementation from the current D0028 rule.

Revisions `e5d996bacb0527a0007749cf5409cbc42b1158ab` and
`0aac137a229300ce0f5317f78a244e610caeaaac` remain immutable evidence of the
intermediate process-inference route rejected by the user before application.
They are not current acceptance evidence. Revision
`4c23b3f596badcb1c341780a93050917bd994c88` introduced the corrected behavior,
and `0101a4a549436e6dd8f4c94175685811d37c90d0` is the final effective revision.

## Future-agent reminder

Never add a Codex, Claude Code, ZCode, or other product row to the commit helper,
and never infer the harness from `/proc`, process names, or product-specific
environment namespaces. The agent reads and surfaces its active harness subject
first, supplies the generic command-local declaration, and the helper stops on
missing or malformed input without falling back to human Git configuration.

## Verification

| Gate | Result |
| --- | --- |
| User decision authority | Passed: the user classified this as governance, required harness-neutral identity, and clarified that the agent must provide its subject without `/proc` inference |
| Active-session handoff | G005 and G008 supersede the earlier G004/G007 discovery route and are published to both affected in-progress sessions |
| Active authorization revisions | Passed at initial revision `0a2c0c4242b754254a6806b0d9c8584b17765d37` and correction revision `69fce57347b08dadba48218812ddf3f1cda56542` before their respective protected edits |
| Agent self-declaration behavior | Seven of seven isolated cases pass; namespace-only signals fail closed, and the helper source contains no process-ancestry reader |
| Historical evidence lock and residual search | Passed pre-commit audit: retained event blob is `5b16d08e62765e483aa715bded55e954ba63794d`; no live static `HARNESSES` table remains |
| Agent records and self-tests | Passed at closing-record commit: 30 sessions / 231 events / 219 Markdown files; validator self-test 163/163 |
| Semantic-change edit gate | Passed staged-tree gate and 21/21 self-tests |
| Skill validation and routing | Passed start-work package validation, 11 domain / 2 workflow / 82 routing corpus cases, and routing self-test 34/34 |
| Guidance protocol | Passed 20/20; G005/G008 superseding packets are published to their active target sessions |
| Real self-declared commits | Passed at behavior revision `4c23b3f596badcb1c341780a93050917bd994c88` and effective revision `0101a4a549436e6dd8f4c94175685811d37c90d0`: the agent first emitted `Agent harness subject: codex`, then supplied command-local `METAFLUX_AGENT_HARNESS=codex`; Author and Committer are `Agent Harness (codex) <codex@localhost>` and local human Git configuration remains unchanged |
