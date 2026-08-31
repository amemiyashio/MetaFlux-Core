---
id: SC0007
status: Active
created: 2026-08-31
updated: 2026-08-31
decision: D0029
session: S0100-20260831-047-breaking-governance-epoch
scope: breaking-governance-epoch
history_sync: automatic
effective_revision: null
superseded_by: null
---

# SC0007: breaking governance epoch

## Semantic replacement

- Classification: **Breaking (destructive) governance**. This is an intentional
  incompatible replacement of the pre-SC0007 execution workflow.
- Old meaning: SC0006 required one focus owner, but a pre-SC0007
  `in_progress` session could still receive a later focus handoff and recover
  content authority without proving that it had loaded the governing version
  of repository files.
- New meaning: D0029 has one explicit governance epoch. Every executable focus
  and its owner session declare the exact current epoch. Only a newly
  scaffolded epoch-bearing session may receive a future handoff; a legacy
  session never regains product, tooling, plan, record-authority, or governance
  content authority.
- Authority: the user's explicit breaking-governance instruction extends D0029;
  `docs/architecture/execution-focus-governance.md` is the canonical source
  migrated by this SC.
- Compatibility consequence: none. There is no compatibility mode, grandfather
  rule, fallback parser, old-session upgrade in place, or old-file execution
  route. Earlier commits retain original bytes as Git history only. Every schema
  version 1 detailed ledger is removed from the current tree after its exact
  migration inventory is committed; a compact ID tombstone does not make the
  old workflow executable.

## Migration inventory

| Surface | Class | Disposition | Evidence |
| --- | --- | --- | --- |
| `AGENTS.md` | Current | Pending | Require the current D0029 epoch before any durable task and prohibit legacy-session continuation |
| `CLAUDE.md` | Current | Pending | Route Claude work only through the epoch-bearing focus owner |
| `.claude/README.md` | Current | Pending | Document the breaking bridge boundary and removal of legacy authority |
| `.claude/hooks/session_start.py` | Tooling | Pending | Emit the exact current governance epoch and owner at session start |
| `.claude/hooks/pre_edit.py` | Tooling | Pending | Reject missing or mismatched focus/session governance epochs before edit routing |
| `.githooks/pre-commit` | Tooling | Pending | Reject candidates whose focus or owner does not declare the exact D0029 epoch |
| `agent/README.md` | Current | Pending | Make current governed files, not prior session context, the work-start authority |
| `agent/memory/constraints.md` | Current | Pending | Record the no-compatibility D0029 epoch as a durable constraint |
| `agent/memory/decisions-index.md` | Current | Pending | Keep D0029 resolvable to the strengthened canonical decision |
| `agent/progress/README.md` | Current | Pending | Define epoch-bearing focus projection and successor-only handoff semantics |
| `agent/progress/current.md` | Current | Pending | Activation candidate projects SC0007 governance; final migration adds the exact epoch |
| `agent/progress/focus.json` | Current | Pending | Activation candidate transfers authority to SC0007; behavior migration adds the exact epoch key |
| `agent/sessions/README.md` | Current | Pending | Activation candidate closes S0112-046 and indexes the migration owner; final text removes legacy resume semantics |
| `agent/semantic-changes/README.md` | Current | Migrated | Activation candidate indexes SC0007 as Active |
| `agent/semantic-changes/SC0007-breaking-governance-epoch.md` | Current | Migrated | This Active permit records the exact breaking replacement, inventory, and handoff |
| `agent/skills/start-work/SKILL.md` | Current | Pending | Require epoch validation and a newly scaffolded successor before content work |
| `agent/skills/record-session/SKILL.md` | Current | Pending | Forbid terminal legacy records from serving as future handoff owners |
| `docs/architecture/execution-focus-governance.md` | Current | Pending | Mark D0029 as breaking/destructive and define zero compatibility |
| `tools/check-agent-records.py` | Tooling | Pending | Validate exact focus epoch and reject a focus owner without the same epoch |
| `tools/test-check-agent-records.py` | Tooling | Pending | Prove missing, mismatched, and legacy-owner epochs fail in focus, hook, and Claude paths |
| `tools/new-session.py` | Tooling | Pending | Scaffold the exact current governance epoch on every future session |
| `tools/README.md` | Current | Pending | Document successor-only continuation and candidate epoch checks |
| `agent/sessions/2026/08/S0100-20260831-047-breaking-governance-epoch/session.json` | Active session | Migrated | Activation candidate declares governance_epoch D0029 on the sole migration owner |
| `agent/sessions/2026/08/S0100-20260831-047-breaking-governance-epoch/events.jsonl` | Active session | Migrated | Objective records breaking classification, zero compatibility, and epoch enforcement |
| `agent/sessions/2026/08/S0100-20260831-047-breaking-governance-epoch/summary.md` | Active session | Migrated | Activation state and product pause are explicit without claiming completion |
| `agent/sessions/2026/08/S0100-20260831-047-breaking-governance-epoch/notes.md` | Active session | Migrated | Records the legacy handoff loophole and bounded migration state |
| `agent/sessions/2026/08/S0112-20260831-046-live-cdev-exit-gate/session.json` | Active session | Migrated | Activation candidate terminally abandons the unstarted pre-epoch product owner |
| `agent/sessions/2026/08/S0112-20260831-046-live-cdev-exit-gate/events.jsonl` | Active session | Migrated | Event 2 records governance preemption before product work |
| `agent/sessions/2026/08/S0112-20260831-046-live-cdev-exit-gate/summary.md` | Active session | Migrated | Terminal summary requires a new epoch-bearing W0112 successor |
| `agent/sessions/2026/08/S0112-20260831-046-live-cdev-exit-gate/notes.md` | Active session | Migrated | Notes classify the old ledger as evidence rather than compatibility authority |
| `agent/semantic-changes/SC0006-execution-focus-governance.md` | Historical | Retained evidence | Preserve the originally Applied single-focus meaning byte-for-byte; SC0007 replaces only its future compatibility consequence |
| `agent/progress/checkpoints/2026/P20260831-090-execution-focus-governance.md` | Historical | Retained evidence | Preserve the SC0006 implementation and handoff claim at its recorded revisions byte-for-byte |
| `agent/sessions/2026/08/S0100-20260831-045-execution-focus-governance/session.json` | Historical | Retained evidence | Preserve the original migration ledger and final revision as factual evidence only |
| `agent/sessions/2026/08/S0100-20260831-045-execution-focus-governance/events.jsonl` | Historical | Retained evidence | Preserve the original four-event execution-focus account byte-for-byte |
| `agent/sessions/2026/08/S0100-20260831-045-execution-focus-governance/summary.md` | Historical | Retained evidence | Preserve the original SC0006 outcome and handoff wording byte-for-byte |
| `agent/sessions/2026/08/S0100-20260831-045-execution-focus-governance/notes.md` | Historical | Retained evidence | Preserve original design evidence; it supplies no post-SC0007 execution authority |

The other 35 affected pre-epoch active session ledgers are named exactly in the
handoff table below and remain only as bounded pre-liquidation input while the
exact protected-file inventory is prepared. Their absence of
`governance_epoch: D0029` is intentional legacy state and becomes a machine
rejection condition, not a compatibility case. They are not resumed or closed
individually: their detailed directories and transient packets are deleted by
SC0007 after inventory authorization. S0112-046 received no packet because its
current owner directly closed it in the atomic activation handoff; its terminal
legacy directory is liquidated with the rest.

## Active-session handoff

| Session | Guidance | Status | Outcome |
| --- | --- | --- | --- |
| `S0111-20260831-023-vfio-user-negotiation` | `G002` | Published | Supersedes G001; close record-only or continue through a new D0029-epoch successor |
| `S01110-20260831-034-cdev-object-table-resolver` | `G002` | Published | Supersedes G001; close record-only or continue through a new D0029-epoch successor |
| `S01112-20260831-036-cdev-daemon-object-activation` | `G002` | Published | Supersedes G001; close record-only or continue through a new D0029-epoch successor |
| `S0112-20260830-013-m0110-local-cdev` | `G002` | Published | Supersedes G001; close record-only or continue through a new D0029-epoch successor |
| `S0112-20260831-021-cuda-cdev-add-copy` | `G002` | Published | Supersedes G001; close record-only or continue through a new D0029-epoch successor |
| `S0112-20260831-024-cdev-async-lease` | `G002` | Published | Supersedes G001; close record-only or continue through a new D0029-epoch successor |
| `S0112-20260831-026-cdev-backend-reference` | `G002` | Published | Supersedes G001; close record-only or continue through a new D0029-epoch successor |
| `S0113-20260830-014-m0110-static-vfio-user` | `G002` | Published | Supersedes G001; close record-only or continue through a new D0029-epoch successor |
| `S0113-20260831-022-vfio-user-guest-ring` | `G002` | Published | Supersedes G001; close record-only or continue through a new D0029-epoch successor |
| `S0113-20260831-025-cdev-dma-map` | `G002` | Published | Supersedes G001; close record-only or continue through a new D0029-epoch successor |
| `S0113-20260831-027-cdev-memory-reference` | `G002` | Published | Supersedes G001; close record-only or continue through a new D0029-epoch successor |
| `S0114-20260831-007-transport-fault-qualification` | `G002` | Published | Supersedes G001; close record-only or continue through a new D0029-epoch successor |
| `S0114-20260831-028-cdev-lifecycle-cancel` | `G002` | Published | Supersedes G001; close record-only or continue through a new D0029-epoch successor |
| `S0115-20260831-029-cdev-lifecycle-drain-cancel` | `G002` | Published | Supersedes G001; close record-only or continue through a new D0029-epoch successor |
| `S0116-20260831-030-cdev-multi-region` | `G002` | Published | Supersedes G001; close record-only or continue through a new D0029-epoch successor |
| `S0117-20260831-031-cdev-memory-reference` | `G002` | Published | Supersedes G001; close record-only or continue through a new D0029-epoch successor |
| `S0118-20260831-032-cdev-memory-import` | `G002` | Published | Supersedes G001; close record-only or continue through a new D0029-epoch successor |
| `S0119-20260831-033-cdev-worker-lease` | `G002` | Published | Supersedes G001; close record-only or continue through a new D0029-epoch successor |
| `S0121-20260830-015-m0120-lifecycle-model` | `G002` | Published | Supersedes G001; close record-only or continue through a new D0029-epoch successor |
| `S0122-20260831-001-m0120-existing-transport-lifecycle` | `G002` | Published | Supersedes G001; close record-only or continue through a new D0029-epoch successor |
| `S0123-20260831-008-lifecycle-core-qualification` | `G002` | Published | Supersedes G001; close record-only or continue through a new D0029-epoch successor |
| `S0123-20260831-015-lifecycle-concurrent-authority` | `G002` | Published | Supersedes G001; close record-only or continue through a new D0029-epoch successor |
| `S0131-20260831-002-vulkan-capability-abi` | `G002` | Published | Supersedes G001; close record-only or continue through a new D0029-epoch successor |
| `S0132-20260831-003-vulkan-device-memory` | `G002` | Published | Supersedes G001; close record-only or continue through a new D0029-epoch successor |
| `S0132-20260831-014-vulkan-memory-visibility` | `G002` | Published | Supersedes G001; close record-only or continue through a new D0029-epoch successor |
| `S01322-20260831-037-vulkan-staging-allocation` | `G003` | Published | Supersedes G002; close record-only or continue through a new D0029-epoch successor |
| `S01323-20260831-038-vulkan-device-local-copy` | `G003` | Published | Supersedes G002; close record-only or continue through a new D0029-epoch successor |
| `S0133-20260831-004-vulkan-spirv-lowering` | `G002` | Published | Supersedes G001; close record-only or continue through a new D0029-epoch successor |
| `S0133-20260831-013-vulkan-module-reflection` | `G002` | Published | Supersedes G001; close record-only or continue through a new D0029-epoch successor |
| `S0134-20260831-005-vulkan-execution-streams` | `G002` | Published | Supersedes G001; close record-only or continue through a new D0029-epoch successor |
| `S0134-20260831-010-vulkan-command-recycling` | `G002` | Published | Supersedes G001; close record-only or continue through a new D0029-epoch successor |
| `S0135-20260831-006-vulkan-cache-warm-path` | `G002` | Published | Supersedes G001; close record-only or continue through a new D0029-epoch successor |
| `S0135-20260831-009-vulkan-cache-filesystem` | `G002` | Published | Supersedes G001; close record-only or continue through a new D0029-epoch successor |
| `S0135-20260831-011-vulkan-cache-catalog` | `G002` | Published | Supersedes G001; close record-only or continue through a new D0029-epoch successor |
| `S0135-20260831-012-vulkan-cache-stampede` | `G002` | Published | Supersedes G001; close record-only or continue through a new D0029-epoch successor |

## Evidence preservation

Git remains the sole owner of prior session bytes at the settlement source
revision. The current tree retains no schema version 1 metadata, event log,
notes, summary, output, guidance, light roast, or session-only detail after
application. `$roast` keeps only claims already promoted as medium or dark in
their existing canonical owners; SC0007 creates no replacement roast archive.
A compact settlement manifest retains only canonical IDs, the source revision,
and liquidation status so old durable references resolve as non-executable
tombstones. This preserves factual history without relabeling old evidence as a
pass under the new epoch.

## Future-agent reminder

This governance is breaking. Read the current `AGENTS.md`,
`agent/progress/focus.json`, `agent/progress/current.md`, matching skill, and
current tool gates. A pre-SC0007 session is never a content owner or task-context
source; its detailed current-tree ledger is liquidated. Scaffold a new session,
confirm that it and focus both declare `governance_epoch: D0029`, and obtain an
atomic handoff before continuing any old objective from current canonical
project files.

## Verification

| Gate | Result |
| --- | --- |
| User breaking-change authority | Passed: explicit instruction requires destructive governance, zero compatibility, and mandatory new engineering files for later sessions |
| Current owner convergence | Activation candidate terminally closes unstarted S0112-046 and installs one epoch-bearing SC0007 migration owner |
| Active-session handoff | Activation boundary published 35 superseding packets; SC0007 now owns their deletion with the legacy ledgers instead of individual resumption |
| Legacy liquidation | Exact protected-file inventory, medium/dark owner audit, compact tombstone, deletion, and residual check remain required before application |
| Behavior and residual gates | Migration must still enforce the epoch in validator, hook, Claude bridge, scaffold, documentation, and regression tests before application |
