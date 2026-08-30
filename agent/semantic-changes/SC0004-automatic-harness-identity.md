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
| `agent/semantic-changes/README.md` | Current | Migrated | Index retains SC0004 Active until the corrected content revision is committed |
| `agent/skills/start-work/SKILL.md` | Current | Migrated | Canonical workflow makes the agent surface its harness subject before supplying a command-local declaration |
| `agent/skills/start-work/scripts/commit_as_harness.py` | Tooling | Migrated | Static table and process/environment inference removed; the helper consumes only the generic agent declaration |
| `agent/skills/start-work/scripts/test_commit_as_harness.py` | Tooling | Migrated | Seven isolated cases cover unseen declarations, no-inference failure, validation, Git identity, and isolation paths |
| `agent/sessions/README.md` | Current | Migrated | S0100-20260830-008-agent-harness-commit-identity is explicitly the initial static implementation superseded by D0028/SC0004 |
| `agent/progress/current.md` | Current | Retained evidence | HEAD blob `7dc3e420aca64caa8b2adb6f7a73b02e21970818` states the generic invariant and factual `ded1dad`; concurrent P20260830-010 remains outside this migration |
| `agent/progress/checkpoints/2026/P20260830-009-agent-harness-commit-identity.md` | Historical | Pending | Replace the superseded automatic-inference resume wording with agent self-declaration while preserving revision, identity, configuration, and gate evidence |
| `agent/sessions/2026/08/S0100-20260830-008-agent-harness-commit-identity/summary.md` | Historical | Pending | Replace the superseded automatic-inference handoff wording with agent self-declaration while preserving recorded gates and revisions |
| `agent/sessions/2026/08/S0100-20260830-008-agent-harness-commit-identity/notes.md` | Historical | Migrated | Capture-time identities contextualized; command-local and protected-option observations retained |
| `agent/sessions/2026/08/S0100-20260830-008-agent-harness-commit-identity/events.jsonl` | Historical | Retained evidence | Preserve byte-for-byte event evidence at Git blob `5b16d08e62765e483aa715bded55e954ba63794d` |
| `agent/sessions/2026/08/S0100-20260830-009-automatic-harness-identity/session.json` | Active session | Pending | Close only after the corrected self-declaration content revision exists |
| `agent/sessions/2026/08/S0100-20260830-009-automatic-harness-identity/events.jsonl` | Active session | Pending | Record the rejected process-inference route, user correction, corrected commit, and application |
| `agent/sessions/2026/08/S0100-20260830-009-automatic-harness-identity/summary.md` | Active session | Pending | Record final D0028/SC0004 outcome, roast, cleanup, and handoff |
| `agent/sessions/2026/08/S0100-20260830-009-automatic-harness-identity/notes.md` | Active session | Pending | Record locked evidence, self-declaration boundary, and concurrent-work exclusion |

## Active-session handoff

| Session | Guidance | Status | Outcome |
| --- | --- | --- | --- |
| `S0100-20260828-013-m0100-foundation` | `G005` | Published | Supersedes G004: agent self-declares its harness subject; helper performs no process or product-environment inference |
| `S0100-20260830-001-m0100-completion-sprint` | `G008` | Published | Supersedes G007: agent self-declares its harness subject; helper performs no process or product-environment inference |

## Evidence preservation

Existing Git objects remain immutable. In particular, preserve the Author and
Committer recorded by `ded1dad4172b515a3b17cb66c3f7aa18df9cb20e` and
`f862852b28e5471b6533ac6754b08c3ca86cbbf4`, their subjects and timestamps,
the S0100-20260830-008-agent-harness-commit-identity event log, all recorded
test counts and outcomes, the human Git configuration observation, and every
cited revision. Historical prose may only distinguish that initial static
implementation from the current D0028 rule.

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
| Active authorization revision | Passed at `0a2c0c4242b754254a6806b0d9c8584b17765d37` before protected edits |
| Agent self-declaration behavior | Seven of seven isolated cases pass; namespace-only signals fail closed, and the helper source contains no process-ancestry reader |
| Historical evidence lock and residual search | Passed pre-commit audit: retained event blob is `5b16d08e62765e483aa715bded55e954ba63794d`; no live static `HARNESSES` table remains |
| Agent records, semantic edit gate, skill validation, and routing | Pending final corrected gates |
| Real self-declared commit | Pending content commit after the agent surfaces and supplies its harness subject |
