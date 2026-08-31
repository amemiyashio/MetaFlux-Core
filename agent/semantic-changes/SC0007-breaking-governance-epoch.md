---
id: SC0007
status: Active
created: 2026-08-31
updated: 2026-09-01
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
| `AGENTS.md` | Current | Migrated | Revision `8247105` requires the current D0029 epoch and prohibits legacy-session continuation |
| `CLAUDE.md` | Current | Migrated | Revision `47bab19` routes Claude work only through the epoch-bearing focus owner |
| `.claude/README.md` | Current | Migrated | Revision `47bab19` documents the breaking bridge boundary and removal of legacy authority |
| `.claude/hooks/session_start.py` | Tooling | Migrated | Revision `47bab19` emits the exact current governance epoch and rejects legacy resumption at session start |
| `.claude/hooks/pre_edit.py` | Tooling | Migrated | Revision `47bab19` rejects missing or mismatched focus/session governance epochs before edit routing |
| `.githooks/pre-commit` | Tooling | Migrated | Revision `47bab19` removed the one-time cutover permit and rejects non-D0029 focus, owners, and closes |
| `agent/README.md` | Current | Migrated | Revision `8247105` makes current governed files, not prior session context, the work-start authority |
| `agent/memory/constraints.md` | Current | Migrated | Revision `8247105` records the destructive, no-compatibility D0029 epoch constraint |
| `agent/memory/decisions-index.md` | Current | Migrated | Revision `8247105` keeps D0029 resolvable to the strengthened canonical decision |
| `agent/progress/README.md` | Current | Migrated | Revision `8247105` defines epoch-bearing focus projection and successor-only handoff semantics |
| `agent/progress/current.md` | Current | Pending | Activation candidate projects SC0007 governance; final migration adds the exact epoch |
| `agent/progress/focus.json` | Current | Migrated | Revision `8247105` atomically adds the exact schema version 2 and D0029 epoch keys |
| `agent/sessions/README.md` | Current | Pending | Activation candidate closes S0112-046 and indexes the migration owner; final text removes legacy resume semantics |
| `agent/semantic-changes/README.md` | Current | Migrated | Activation candidate indexes SC0007 as Active |
| `agent/semantic-changes/SC0007-breaking-governance-epoch.md` | Current | Migrated | This Active permit records the exact breaking replacement, inventory, and handoff |
| `agent/skills/start-work/SKILL.md` | Current | Migrated | Revision `8247105` requires epoch validation and a newly scaffolded successor before content work |
| `agent/skills/record-session/SKILL.md` | Current | Migrated | Revision `8247105` forbids legacy records from serving as handoff owners and defines destructive settlement |
| `docs/architecture/execution-focus-governance.md` | Current | Pending | Mark D0029 as breaking/destructive and define zero compatibility |
| `tools/check-agent-records.py` | Tooling | Pending | Validate exact focus epoch and reject a focus owner without the same epoch |
| `tools/test-check-agent-records.py` | Tooling | Pending | Prove missing, mismatched, and legacy-owner epochs fail in focus, hook, and Claude paths |
| `tools/new-session.py` | Tooling | Migrated | Revision `8247105` scaffolds the exact current governance epoch on every future session |
| `tools/README.md` | Current | Pending | Document successor-only continuation and candidate epoch checks |
| `agent/sessions/2026/08/S0100-20260831-047-breaking-governance-epoch/session.json` | Active session | Migrated | Activation candidate declares governance_epoch D0029 on the sole migration owner |
| `agent/sessions/2026/08/S0100-20260831-047-breaking-governance-epoch/events.jsonl` | Active session | Migrated | Objective records breaking classification, zero compatibility, and epoch enforcement |
| `agent/sessions/2026/08/S0100-20260831-047-breaking-governance-epoch/summary.md` | Active session | Migrated | Activation state and product pause are explicit without claiming completion |
| `agent/sessions/2026/08/S0100-20260831-047-breaking-governance-epoch/notes.md` | Active session | Migrated | Records the legacy handoff loophole and bounded migration state |
| `agent/sessions/2026/08/S0100-20260827-001-metaflux-bootstrap/events.jsonl` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260827-001-metaflux-bootstrap/notes.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260827-001-metaflux-bootstrap/outputs/0001.txt` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260827-001-metaflux-bootstrap/outputs/README.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260827-001-metaflux-bootstrap/session.json` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260827-001-metaflux-bootstrap/summary.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260828-001-spec-consistency/events.jsonl` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260828-001-spec-consistency/notes.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260828-001-spec-consistency/session.json` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260828-001-spec-consistency/summary.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260828-002-layout-convergence/events.jsonl` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260828-002-layout-convergence/notes.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260828-002-layout-convergence/session.json` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260828-002-layout-convergence/summary.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260828-003-agent-record-convergence/events.jsonl` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260828-003-agent-record-convergence/notes.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260828-003-agent-record-convergence/session.json` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260828-003-agent-record-convergence/summary.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260828-004-record-gate-hardening/events.jsonl` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260828-004-record-gate-hardening/notes.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260828-004-record-gate-hardening/session.json` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260828-004-record-gate-hardening/summary.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260828-005-skills-layer/events.jsonl` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260828-005-skills-layer/notes.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260828-005-skills-layer/session.json` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260828-005-skills-layer/summary.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260828-006-agent-guidance-hardening/events.jsonl` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260828-006-agent-guidance-hardening/notes.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260828-006-agent-guidance-hardening/session.json` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260828-006-agent-guidance-hardening/summary.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260828-007-claude-bridge/events.jsonl` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260828-007-claude-bridge/notes.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260828-007-claude-bridge/session.json` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260828-007-claude-bridge/summary.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260828-010-domain-expert-skills/events.jsonl` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260828-010-domain-expert-skills/notes.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260828-010-domain-expert-skills/session.json` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260828-010-domain-expert-skills/summary.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260828-011-skill-design-convergence/events.jsonl` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260828-011-skill-design-convergence/notes.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260828-011-skill-design-convergence/session.json` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260828-011-skill-design-convergence/summary.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260828-013-m0100-foundation/events.jsonl` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260828-013-m0100-foundation/notes.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260828-013-m0100-foundation/session.json` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260828-013-m0100-foundation/summary.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260829-002-cleanup-stale-routes/events.jsonl` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260829-002-cleanup-stale-routes/notes.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260829-002-cleanup-stale-routes/session.json` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260829-002-cleanup-stale-routes/summary.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260829-004-implementation-session-current-standard/events.jsonl` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260829-004-implementation-session-current-standard/notes.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260829-004-implementation-session-current-standard/session.json` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260829-004-implementation-session-current-standard/summary.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260829-005-m0100-vertical-slice/events.jsonl` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260829-005-m0100-vertical-slice/notes.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260829-005-m0100-vertical-slice/session.json` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260829-005-m0100-vertical-slice/summary.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260829-006-stage-breakthrough-commits/events.jsonl` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260829-006-stage-breakthrough-commits/notes.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260829-006-stage-breakthrough-commits/session.json` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260829-006-stage-breakthrough-commits/summary.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260829-007-pytorch-cuda-test-tool/events.jsonl` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260829-007-pytorch-cuda-test-tool/notes.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260829-007-pytorch-cuda-test-tool/session.json` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260829-007-pytorch-cuda-test-tool/summary.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260830-001-m0100-completion-sprint/events.jsonl` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260830-001-m0100-completion-sprint/notes.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260830-001-m0100-completion-sprint/session.json` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260830-001-m0100-completion-sprint/summary.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260830-003-session-guidance-loop/events.jsonl` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260830-003-session-guidance-loop/notes.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260830-003-session-guidance-loop/session.json` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260830-003-session-guidance-loop/summary.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260830-004-semantic-version-line/events.jsonl` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260830-004-semantic-version-line/notes.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260830-004-semantic-version-line/session.json` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260830-004-semantic-version-line/summary.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260830-005-semantic-change-distillation/events.jsonl` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260830-005-semantic-change-distillation/notes.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260830-005-semantic-change-distillation/session.json` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260830-005-semantic-change-distillation/summary.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260830-006-project-knowledge-roast/events.jsonl` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260830-006-project-knowledge-roast/notes.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260830-006-project-knowledge-roast/session.json` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260830-006-project-knowledge-roast/summary.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260830-008-agent-harness-commit-identity/events.jsonl` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260830-008-agent-harness-commit-identity/notes.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260830-008-agent-harness-commit-identity/session.json` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260830-008-agent-harness-commit-identity/summary.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260830-009-automatic-harness-identity/events.jsonl` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260830-009-automatic-harness-identity/notes.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260830-009-automatic-harness-identity/session.json` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260830-009-automatic-harness-identity/summary.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260830-010-m0100-closure-consistency/events.jsonl` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260830-010-m0100-closure-consistency/notes.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260830-010-m0100-closure-consistency/session.json` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260830-010-m0100-closure-consistency/summary.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260831-045-execution-focus-governance/events.jsonl` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260831-045-execution-focus-governance/notes.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260831-045-execution-focus-governance/session.json` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0100-20260831-045-execution-focus-governance/summary.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0101-20260828-008-codex-entry-points/events.jsonl` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0101-20260828-008-codex-entry-points/notes.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0101-20260828-008-codex-entry-points/session.json` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0101-20260828-008-codex-entry-points/summary.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0101-20260828-009-implementation-readiness-skill/events.jsonl` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0101-20260828-009-implementation-readiness-skill/notes.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0101-20260828-009-implementation-readiness-skill/session.json` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0101-20260828-009-implementation-readiness-skill/summary.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0101-20260828-012-ubuntu-2004-glibc-floor/events.jsonl` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0101-20260828-012-ubuntu-2004-glibc-floor/notes.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0101-20260828-012-ubuntu-2004-glibc-floor/session.json` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0101-20260828-012-ubuntu-2004-glibc-floor/summary.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0101-20260829-001-toolchain-boundary-correction/events.jsonl` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0101-20260829-001-toolchain-boundary-correction/notes.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0101-20260829-001-toolchain-boundary-correction/session.json` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0101-20260829-001-toolchain-boundary-correction/summary.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0101-20260829-003-timezone-mirror-priority/events.jsonl` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0101-20260829-003-timezone-mirror-priority/notes.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0101-20260829-003-timezone-mirror-priority/session.json` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0101-20260829-003-timezone-mirror-priority/summary.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0101-20260830-002-ubuntu-target-sdk-guide/events.jsonl` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0101-20260830-002-ubuntu-target-sdk-guide/notes.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0101-20260830-002-ubuntu-target-sdk-guide/session.json` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0101-20260830-002-ubuntu-target-sdk-guide/summary.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0110-20260830-011-converge-project-changes/events.jsonl` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0110-20260830-011-converge-project-changes/notes.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0110-20260830-011-converge-project-changes/session.json` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0110-20260830-011-converge-project-changes/summary.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0110-20260831-039-execution-priority-convergence/events.jsonl` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0110-20260831-039-execution-priority-convergence/notes.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0110-20260831-039-execution-priority-convergence/session.json` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0110-20260831-039-execution-priority-convergence/summary.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0111-20260830-012-m0110-abi-contract/events.jsonl` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0111-20260830-012-m0110-abi-contract/notes.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0111-20260830-012-m0110-abi-contract/session.json` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0111-20260830-012-m0110-abi-contract/summary.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0111-20260831-023-vfio-user-negotiation/events.jsonl` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0111-20260831-023-vfio-user-negotiation/notes.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0111-20260831-023-vfio-user-negotiation/session.json` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0111-20260831-023-vfio-user-negotiation/summary.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S01110-20260831-034-cdev-object-table-resolver/events.jsonl` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S01110-20260831-034-cdev-object-table-resolver/notes.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S01110-20260831-034-cdev-object-table-resolver/session.json` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S01110-20260831-034-cdev-object-table-resolver/summary.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S01111-20260831-035-cdev-generation-control-plane/events.jsonl` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S01111-20260831-035-cdev-generation-control-plane/notes.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S01111-20260831-035-cdev-generation-control-plane/session.json` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S01111-20260831-035-cdev-generation-control-plane/summary.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S01112-20260831-036-cdev-daemon-object-activation/events.jsonl` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S01112-20260831-036-cdev-daemon-object-activation/notes.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S01112-20260831-036-cdev-daemon-object-activation/session.json` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S01112-20260831-036-cdev-daemon-object-activation/summary.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0112-20260830-013-m0110-local-cdev/events.jsonl` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0112-20260830-013-m0110-local-cdev/notes.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0112-20260830-013-m0110-local-cdev/session.json` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0112-20260830-013-m0110-local-cdev/summary.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0112-20260831-021-cuda-cdev-add-copy/events.jsonl` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0112-20260831-021-cuda-cdev-add-copy/notes.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0112-20260831-021-cuda-cdev-add-copy/session.json` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0112-20260831-021-cuda-cdev-add-copy/summary.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0112-20260831-024-cdev-async-lease/events.jsonl` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0112-20260831-024-cdev-async-lease/notes.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0112-20260831-024-cdev-async-lease/session.json` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0112-20260831-024-cdev-async-lease/summary.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0112-20260831-026-cdev-backend-reference/events.jsonl` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0112-20260831-026-cdev-backend-reference/notes.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0112-20260831-026-cdev-backend-reference/session.json` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0112-20260831-026-cdev-backend-reference/summary.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0112-20260831-046-live-cdev-exit-gate/events.jsonl` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0112-20260831-046-live-cdev-exit-gate/notes.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0112-20260831-046-live-cdev-exit-gate/session.json` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0112-20260831-046-live-cdev-exit-gate/summary.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0113-20260830-014-m0110-static-vfio-user/events.jsonl` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0113-20260830-014-m0110-static-vfio-user/notes.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0113-20260830-014-m0110-static-vfio-user/session.json` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0113-20260830-014-m0110-static-vfio-user/summary.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0113-20260831-022-vfio-user-guest-ring/events.jsonl` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0113-20260831-022-vfio-user-guest-ring/notes.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0113-20260831-022-vfio-user-guest-ring/session.json` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0113-20260831-022-vfio-user-guest-ring/summary.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0113-20260831-025-cdev-dma-map/events.jsonl` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0113-20260831-025-cdev-dma-map/notes.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0113-20260831-025-cdev-dma-map/session.json` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0113-20260831-025-cdev-dma-map/summary.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0113-20260831-027-cdev-memory-reference/events.jsonl` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0113-20260831-027-cdev-memory-reference/notes.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0113-20260831-027-cdev-memory-reference/session.json` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0113-20260831-027-cdev-memory-reference/summary.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0114-20260831-007-transport-fault-qualification/events.jsonl` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0114-20260831-007-transport-fault-qualification/notes.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0114-20260831-007-transport-fault-qualification/session.json` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0114-20260831-007-transport-fault-qualification/summary.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0114-20260831-028-cdev-lifecycle-cancel/events.jsonl` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0114-20260831-028-cdev-lifecycle-cancel/notes.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0114-20260831-028-cdev-lifecycle-cancel/session.json` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0114-20260831-028-cdev-lifecycle-cancel/summary.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0115-20260831-029-cdev-lifecycle-drain-cancel/events.jsonl` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0115-20260831-029-cdev-lifecycle-drain-cancel/notes.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0115-20260831-029-cdev-lifecycle-drain-cancel/session.json` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0115-20260831-029-cdev-lifecycle-drain-cancel/summary.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0116-20260831-030-cdev-multi-region/events.jsonl` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0116-20260831-030-cdev-multi-region/notes.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0116-20260831-030-cdev-multi-region/session.json` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0116-20260831-030-cdev-multi-region/summary.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0117-20260831-031-cdev-memory-reference/events.jsonl` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0117-20260831-031-cdev-memory-reference/notes.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0117-20260831-031-cdev-memory-reference/session.json` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0117-20260831-031-cdev-memory-reference/summary.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0118-20260831-032-cdev-memory-import/events.jsonl` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0118-20260831-032-cdev-memory-import/notes.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0118-20260831-032-cdev-memory-import/session.json` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0118-20260831-032-cdev-memory-import/summary.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0119-20260831-033-cdev-worker-lease/events.jsonl` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0119-20260831-033-cdev-worker-lease/notes.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0119-20260831-033-cdev-worker-lease/session.json` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0119-20260831-033-cdev-worker-lease/summary.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0121-20260830-015-m0120-lifecycle-model/events.jsonl` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0121-20260830-015-m0120-lifecycle-model/notes.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0121-20260830-015-m0120-lifecycle-model/session.json` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0121-20260830-015-m0120-lifecycle-model/summary.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0122-20260831-001-m0120-existing-transport-lifecycle/events.jsonl` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0122-20260831-001-m0120-existing-transport-lifecycle/notes.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0122-20260831-001-m0120-existing-transport-lifecycle/session.json` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0122-20260831-001-m0120-existing-transport-lifecycle/summary.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0122-20260831-040-live-qmp-socket-transport/events.jsonl` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0122-20260831-040-live-qmp-socket-transport/notes.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0122-20260831-040-live-qmp-socket-transport/session.json` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0122-20260831-040-live-qmp-socket-transport/summary.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0122-20260831-042-qmp-socket-lifecycle-bridge/events.jsonl` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0122-20260831-042-qmp-socket-lifecycle-bridge/notes.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0122-20260831-042-qmp-socket-lifecycle-bridge/session.json` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0122-20260831-042-qmp-socket-lifecycle-bridge/summary.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0122-20260831-043-provider-view-freeze/events.jsonl` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0122-20260831-043-provider-view-freeze/notes.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0122-20260831-043-provider-view-freeze/session.json` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0122-20260831-043-provider-view-freeze/summary.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0123-20260831-008-lifecycle-core-qualification/events.jsonl` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0123-20260831-008-lifecycle-core-qualification/notes.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0123-20260831-008-lifecycle-core-qualification/session.json` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0123-20260831-008-lifecycle-core-qualification/summary.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0123-20260831-015-lifecycle-concurrent-authority/events.jsonl` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0123-20260831-015-lifecycle-concurrent-authority/notes.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0123-20260831-015-lifecycle-concurrent-authority/session.json` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0123-20260831-015-lifecycle-concurrent-authority/summary.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0123-20260831-041-lifecycle-node-policy/events.jsonl` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0123-20260831-041-lifecycle-node-policy/notes.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0123-20260831-041-lifecycle-node-policy/session.json` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0123-20260831-041-lifecycle-node-policy/summary.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0123-20260831-044-m0120-producer-ingress/events.jsonl` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0123-20260831-044-m0120-producer-ingress/notes.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0123-20260831-044-m0120-producer-ingress/session.json` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0123-20260831-044-m0120-producer-ingress/summary.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0131-20260831-002-vulkan-capability-abi/events.jsonl` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0131-20260831-002-vulkan-capability-abi/notes.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0131-20260831-002-vulkan-capability-abi/session.json` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0131-20260831-002-vulkan-capability-abi/summary.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0131-20260831-019-vulkan-runtime-tools/events.jsonl` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0131-20260831-019-vulkan-runtime-tools/notes.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0131-20260831-019-vulkan-runtime-tools/session.json` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0131-20260831-019-vulkan-runtime-tools/summary.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0132-20260831-003-vulkan-device-memory/events.jsonl` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0132-20260831-003-vulkan-device-memory/notes.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0132-20260831-003-vulkan-device-memory/session.json` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0132-20260831-003-vulkan-device-memory/summary.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0132-20260831-014-vulkan-memory-visibility/events.jsonl` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0132-20260831-014-vulkan-memory-visibility/notes.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0132-20260831-014-vulkan-memory-visibility/session.json` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0132-20260831-014-vulkan-memory-visibility/summary.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0132-20260831-020-vulkan-device-context/events.jsonl` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0132-20260831-020-vulkan-device-context/notes.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0132-20260831-020-vulkan-device-context/session.json` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0132-20260831-020-vulkan-device-context/summary.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S01322-20260831-037-vulkan-staging-allocation/events.jsonl` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S01322-20260831-037-vulkan-staging-allocation/notes.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S01322-20260831-037-vulkan-staging-allocation/session.json` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S01322-20260831-037-vulkan-staging-allocation/summary.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S01323-20260831-038-vulkan-device-local-copy/events.jsonl` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S01323-20260831-038-vulkan-device-local-copy/notes.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S01323-20260831-038-vulkan-device-local-copy/session.json` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S01323-20260831-038-vulkan-device-local-copy/summary.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0133-20260831-004-vulkan-spirv-lowering/events.jsonl` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0133-20260831-004-vulkan-spirv-lowering/notes.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0133-20260831-004-vulkan-spirv-lowering/session.json` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0133-20260831-004-vulkan-spirv-lowering/summary.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0133-20260831-013-vulkan-module-reflection/events.jsonl` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0133-20260831-013-vulkan-module-reflection/notes.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0133-20260831-013-vulkan-module-reflection/session.json` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0133-20260831-013-vulkan-module-reflection/summary.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0134-20260831-005-vulkan-execution-streams/events.jsonl` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0134-20260831-005-vulkan-execution-streams/notes.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0134-20260831-005-vulkan-execution-streams/session.json` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0134-20260831-005-vulkan-execution-streams/summary.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0134-20260831-010-vulkan-command-recycling/events.jsonl` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0134-20260831-010-vulkan-command-recycling/notes.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0134-20260831-010-vulkan-command-recycling/session.json` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0134-20260831-010-vulkan-command-recycling/summary.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0134-20260831-018-vulkan-submit-admission/events.jsonl` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0134-20260831-018-vulkan-submit-admission/notes.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0134-20260831-018-vulkan-submit-admission/session.json` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0134-20260831-018-vulkan-submit-admission/summary.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0135-20260831-006-vulkan-cache-warm-path/events.jsonl` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0135-20260831-006-vulkan-cache-warm-path/notes.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0135-20260831-006-vulkan-cache-warm-path/session.json` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0135-20260831-006-vulkan-cache-warm-path/summary.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0135-20260831-009-vulkan-cache-filesystem/events.jsonl` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0135-20260831-009-vulkan-cache-filesystem/notes.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0135-20260831-009-vulkan-cache-filesystem/session.json` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0135-20260831-009-vulkan-cache-filesystem/summary.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0135-20260831-011-vulkan-cache-catalog/events.jsonl` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0135-20260831-011-vulkan-cache-catalog/notes.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0135-20260831-011-vulkan-cache-catalog/session.json` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0135-20260831-011-vulkan-cache-catalog/summary.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0135-20260831-012-vulkan-cache-stampede/events.jsonl` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0135-20260831-012-vulkan-cache-stampede/notes.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0135-20260831-012-vulkan-cache-stampede/session.json` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0135-20260831-012-vulkan-cache-stampede/summary.md` | Active session | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0135-20260831-016-vulkan-pipeline-binding/events.jsonl` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0135-20260831-016-vulkan-pipeline-binding/notes.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0135-20260831-016-vulkan-pipeline-binding/session.json` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0135-20260831-016-vulkan-pipeline-binding/summary.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0135-20260831-017-vulkan-warm-path/events.jsonl` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0135-20260831-017-vulkan-warm-path/notes.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0135-20260831-017-vulkan-warm-path/session.json` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S0135-20260831-017-vulkan-warm-path/summary.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S1000-20260830-007-v100-qualification-boundary/events.jsonl` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S1000-20260830-007-v100-qualification-boundary/notes.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S1000-20260830-007-v100-qualification-boundary/session.json` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/sessions/2026/08/S1000-20260830-007-v100-qualification-boundary/summary.md` | Historical | Pending | Delete schema version 1 detail after audited medium/dark ownership; source bytes remain at the settlement revision |
| `agent/progress/checkpoints/2026/P20260828-001-spec-consistency-revisions.md` | Historical | Pending | Replace direct legacy-summary links with the non-executable settlement tombstone while preserving checkpoint facts |
| `agent/progress/checkpoints/2026/P20260828-002-layout-convergence.md` | Historical | Pending | Replace direct legacy-summary links with the non-executable settlement tombstone while preserving checkpoint facts |
| `agent/progress/checkpoints/2026/P20260828-003-agent-record-convergence.md` | Historical | Pending | Replace direct legacy-summary links with the non-executable settlement tombstone while preserving checkpoint facts |
| `agent/progress/checkpoints/2026/P20260828-004-record-gate-hardening.md` | Historical | Pending | Replace direct legacy-summary links with the non-executable settlement tombstone while preserving checkpoint facts |
| `agent/progress/checkpoints/2026/P20260828-005-skills-layer.md` | Historical | Pending | Replace direct legacy-summary links with the non-executable settlement tombstone while preserving checkpoint facts |
| `agent/progress/checkpoints/2026/P20260828-006-agent-guidance-hardening.md` | Historical | Pending | Replace direct legacy-summary links with the non-executable settlement tombstone while preserving checkpoint facts |
| `agent/progress/checkpoints/2026/P20260828-007-claude-bridge.md` | Historical | Pending | Replace direct legacy-summary links with the non-executable settlement tombstone while preserving checkpoint facts |
| `agent/progress/checkpoints/2026/P20260828-008-codex-entry-points.md` | Historical | Pending | Replace direct legacy-summary links with the non-executable settlement tombstone while preserving checkpoint facts |
| `agent/progress/checkpoints/2026/P20260828-009-codex-skill-packages.md` | Historical | Pending | Replace direct legacy-summary links with the non-executable settlement tombstone while preserving checkpoint facts |
| `agent/progress/checkpoints/2026/P20260828-010-domain-expert-skills.md` | Historical | Pending | Replace direct legacy-summary links with the non-executable settlement tombstone while preserving checkpoint facts |
| `agent/progress/checkpoints/2026/P20260828-012-ubuntu-2004-glibc-floor.md` | Historical | Pending | Replace direct legacy-summary links with the non-executable settlement tombstone while preserving checkpoint facts |
| `agent/progress/checkpoints/2026/P20260828-013-skill-design-convergence.md` | Historical | Pending | Replace direct legacy-summary links with the non-executable settlement tombstone while preserving checkpoint facts |
| `agent/progress/checkpoints/2026/P20260829-001-toolchain-boundary-correction.md` | Historical | Pending | Replace direct legacy-summary links with the non-executable settlement tombstone while preserving checkpoint facts |
| `agent/progress/checkpoints/2026/P20260829-002-stale-route-cleanup.md` | Historical | Pending | Replace direct legacy-summary links with the non-executable settlement tombstone while preserving checkpoint facts |
| `agent/progress/checkpoints/2026/P20260829-003-session-lifecycle-normalization.md` | Historical | Pending | Replace direct legacy-summary links with the non-executable settlement tombstone while preserving checkpoint facts |
| `agent/progress/checkpoints/2026/P20260829-004-glibc-floor-qualification.md` | Historical | Pending | Replace direct legacy-summary links with the non-executable settlement tombstone while preserving checkpoint facts |
| `agent/progress/checkpoints/2026/P20260829-005-stage-breakthrough-commit-policy.md` | Historical | Pending | Replace direct legacy-summary links with the non-executable settlement tombstone while preserving checkpoint facts |
| `agent/semantic-changes/SC0006-execution-focus-governance.md` | Historical | Retained evidence | Preserve the originally Applied single-focus meaning byte-for-byte; SC0007 replaces only its future compatibility consequence |
| `agent/progress/checkpoints/2026/P20260831-090-execution-focus-governance.md` | Historical | Retained evidence | Preserve the SC0006 implementation and handoff claim at its recorded revisions byte-for-byte |

The inventory names all 330 Git-tracked files under the 82 schema version 1
session directories: 190 files from 47 terminal sessions are Historical and
140 files from 35 `in_progress` sessions are Active session. It also names 17
Historical checkpoints containing 19 direct links to old summaries. A set
comparison against `git ls-files agent/sessions` reports zero missing, extra, or
duplicate legacy paths. This inventory commit changes no listed historical
surface; it must be present in `HEAD` before deletion or link migration begins.

The 35 affected pre-epoch active session ledgers are named exactly in both the
inventory and handoff table below and remain only as bounded pre-liquidation
input. Their absence of
`governance_epoch: D0029` is intentional legacy state and becomes a machine
rejection condition, not a compatibility case. They are not resumed or closed
individually: their detailed directories and transient packets are deleted by
SC0007 after inventory authorization. S0112-046 received no packet because its
current owner directly closed it in the atomic activation handoff; its terminal
legacy directory is liquidated with the rest.

## Active-session handoff

| Session | Guidance | Status | Outcome |
| --- | --- | --- | --- |
| `S0111-20260831-023-vfio-user-negotiation` | `G002` | Resolved | Packet superseded; SC0007 deletes target detail and guidance, and the objective may continue only in a new current-epoch successor |
| `S01110-20260831-034-cdev-object-table-resolver` | `G002` | Resolved | Packet superseded; SC0007 deletes target detail and guidance, and the objective may continue only in a new current-epoch successor |
| `S01112-20260831-036-cdev-daemon-object-activation` | `G002` | Resolved | Packet superseded; SC0007 deletes target detail and guidance, and the objective may continue only in a new current-epoch successor |
| `S0112-20260830-013-m0110-local-cdev` | `G002` | Resolved | Packet superseded; SC0007 deletes target detail and guidance, and the objective may continue only in a new current-epoch successor |
| `S0112-20260831-021-cuda-cdev-add-copy` | `G002` | Resolved | Packet superseded; SC0007 deletes target detail and guidance, and the objective may continue only in a new current-epoch successor |
| `S0112-20260831-024-cdev-async-lease` | `G002` | Resolved | Packet superseded; SC0007 deletes target detail and guidance, and the objective may continue only in a new current-epoch successor |
| `S0112-20260831-026-cdev-backend-reference` | `G002` | Resolved | Packet superseded; SC0007 deletes target detail and guidance, and the objective may continue only in a new current-epoch successor |
| `S0113-20260830-014-m0110-static-vfio-user` | `G002` | Resolved | Packet superseded; SC0007 deletes target detail and guidance, and the objective may continue only in a new current-epoch successor |
| `S0113-20260831-022-vfio-user-guest-ring` | `G002` | Resolved | Packet superseded; SC0007 deletes target detail and guidance, and the objective may continue only in a new current-epoch successor |
| `S0113-20260831-025-cdev-dma-map` | `G002` | Resolved | Packet superseded; SC0007 deletes target detail and guidance, and the objective may continue only in a new current-epoch successor |
| `S0113-20260831-027-cdev-memory-reference` | `G002` | Resolved | Packet superseded; SC0007 deletes target detail and guidance, and the objective may continue only in a new current-epoch successor |
| `S0114-20260831-007-transport-fault-qualification` | `G002` | Resolved | Packet superseded; SC0007 deletes target detail and guidance, and the objective may continue only in a new current-epoch successor |
| `S0114-20260831-028-cdev-lifecycle-cancel` | `G002` | Resolved | Packet superseded; SC0007 deletes target detail and guidance, and the objective may continue only in a new current-epoch successor |
| `S0115-20260831-029-cdev-lifecycle-drain-cancel` | `G002` | Resolved | Packet superseded; SC0007 deletes target detail and guidance, and the objective may continue only in a new current-epoch successor |
| `S0116-20260831-030-cdev-multi-region` | `G002` | Resolved | Packet superseded; SC0007 deletes target detail and guidance, and the objective may continue only in a new current-epoch successor |
| `S0117-20260831-031-cdev-memory-reference` | `G002` | Resolved | Packet superseded; SC0007 deletes target detail and guidance, and the objective may continue only in a new current-epoch successor |
| `S0118-20260831-032-cdev-memory-import` | `G002` | Resolved | Packet superseded; SC0007 deletes target detail and guidance, and the objective may continue only in a new current-epoch successor |
| `S0119-20260831-033-cdev-worker-lease` | `G002` | Resolved | Packet superseded; SC0007 deletes target detail and guidance, and the objective may continue only in a new current-epoch successor |
| `S0121-20260830-015-m0120-lifecycle-model` | `G002` | Resolved | Packet superseded; SC0007 deletes target detail and guidance, and the objective may continue only in a new current-epoch successor |
| `S0122-20260831-001-m0120-existing-transport-lifecycle` | `G002` | Resolved | Packet superseded; SC0007 deletes target detail and guidance, and the objective may continue only in a new current-epoch successor |
| `S0123-20260831-008-lifecycle-core-qualification` | `G002` | Resolved | Packet superseded; SC0007 deletes target detail and guidance, and the objective may continue only in a new current-epoch successor |
| `S0123-20260831-015-lifecycle-concurrent-authority` | `G002` | Resolved | Packet superseded; SC0007 deletes target detail and guidance, and the objective may continue only in a new current-epoch successor |
| `S0131-20260831-002-vulkan-capability-abi` | `G002` | Resolved | Packet superseded; SC0007 deletes target detail and guidance, and the objective may continue only in a new current-epoch successor |
| `S0132-20260831-003-vulkan-device-memory` | `G002` | Resolved | Packet superseded; SC0007 deletes target detail and guidance, and the objective may continue only in a new current-epoch successor |
| `S0132-20260831-014-vulkan-memory-visibility` | `G002` | Resolved | Packet superseded; SC0007 deletes target detail and guidance, and the objective may continue only in a new current-epoch successor |
| `S01322-20260831-037-vulkan-staging-allocation` | `G003` | Resolved | Packet superseded; SC0007 deletes target detail and guidance, and the objective may continue only in a new current-epoch successor |
| `S01323-20260831-038-vulkan-device-local-copy` | `G003` | Resolved | Packet superseded; SC0007 deletes target detail and guidance, and the objective may continue only in a new current-epoch successor |
| `S0133-20260831-004-vulkan-spirv-lowering` | `G002` | Resolved | Packet superseded; SC0007 deletes target detail and guidance, and the objective may continue only in a new current-epoch successor |
| `S0133-20260831-013-vulkan-module-reflection` | `G002` | Resolved | Packet superseded; SC0007 deletes target detail and guidance, and the objective may continue only in a new current-epoch successor |
| `S0134-20260831-005-vulkan-execution-streams` | `G002` | Resolved | Packet superseded; SC0007 deletes target detail and guidance, and the objective may continue only in a new current-epoch successor |
| `S0134-20260831-010-vulkan-command-recycling` | `G002` | Resolved | Packet superseded; SC0007 deletes target detail and guidance, and the objective may continue only in a new current-epoch successor |
| `S0135-20260831-006-vulkan-cache-warm-path` | `G002` | Resolved | Packet superseded; SC0007 deletes target detail and guidance, and the objective may continue only in a new current-epoch successor |
| `S0135-20260831-009-vulkan-cache-filesystem` | `G002` | Resolved | Packet superseded; SC0007 deletes target detail and guidance, and the objective may continue only in a new current-epoch successor |
| `S0135-20260831-011-vulkan-cache-catalog` | `G002` | Resolved | Packet superseded; SC0007 deletes target detail and guidance, and the objective may continue only in a new current-epoch successor |
| `S0135-20260831-012-vulkan-cache-stampede` | `G002` | Resolved | Packet superseded; SC0007 deletes target detail and guidance, and the objective may continue only in a new current-epoch successor |

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

The pre-liquidation `$roast` audit parsed 72 medium and 24 dark entries from the
82 legacy summaries with zero parse errors. Their 96 mappings resolve to 62
unique existing canonical owners. No owner is a legacy session ID or path under
a legacy detail directory; `agent/sessions/README.md` is the sole owner in the
sessions namespace and retains only lifecycle semantics. Light roasts and
session-only material are deliberately excluded. The 72 untracked guidance
packets are transient session detail and are deleted with their 35 target
directories; they are not committed or promoted.

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
| Behavior gates | Passed in `8247105` and `47bab19`: strict schema version 2 / D0029 focus, owner, scaffold, pre-commit, and Claude paths; 189 Agent-record cases pass |
| Exact legacy inventory | Passed in the candidate: 330/330 tracked legacy files, 17 protected checkpoint files, zero missing, extra, or duplicate paths; commit to `HEAD` is required before edits |
| Medium/dark owner audit | Passed: 72 medium plus 24 dark mappings, 62 unique existing owners, zero parse errors, and no owner in a deleted legacy detail directory |
| Active-session handoff | Resolved: 35 targets cannot close or resume under the old epoch; 72 transient packets are deleted with their ledgers |
| Legacy liquidation | Compact tombstone implementation, deletion, checkpoint-link migration, and residual check remain required before application |
