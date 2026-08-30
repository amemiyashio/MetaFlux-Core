---
id: SC0001
status: Applied
created: 2026-08-30
updated: 2026-08-30
decision: D0025
session: S0100-20260830-005-semantic-change-distillation
scope: semantic-change-distillation
history_sync: automatic
effective_revision: 90c45eda2815c59617fffea581522b7ed6bff1c0
superseded_by: null
---

# SC0001: Semantic change and distillation governance

## Semantic replacement

- Old meaning: D0024 was described as the sole historical synchronization
  exception, terminal sessions and checkpoints were described as absolutely
  immutable, and session distillation used undifferentiated `Distilled` rows.
- New meaning: D0024 remains the first completed pre-framework migration;
  D0025 authorizes later exact-path synchronization through a committed Active
  SC, and terminal summaries distinguish promoted claims from intentionally
  session-only material.
- Authority: D0025 in `docs/architecture/semantic-change-governance.md`.
- Compatibility consequence: current and historical policy wording and terminal
  summary shape converge, while Git retains earlier forms and raw facts remain
  unchanged.

## Migration inventory

| Surface | Class | Disposition | Evidence |
| --- | --- | --- | --- |
| `AGENTS.md` | Current | Migrated | D0025 and both workflow skills landed in `1ecdfb0` |
| `CLAUDE.md` | Current | Migrated | Protected-history wording landed in `1ecdfb0` |
| `agent/README.md` | Current | Migrated | SC authority and protected-checkpoint wording completed in `90c45ed` |
| `agent/memory/constraints.md` | Current | Migrated | D0025 evidence-preservation constraint landed in `1ecdfb0` |
| `agent/memory/decisions-index.md` | Current | Migrated | D0025 resolves to its verified canonical source |
| `agent/progress/README.md` | Current | Migrated | Protected checkpoint policy and latest checkpoint are current |
| `agent/progress/current.md` | Current | Migrated | Resume point records SC0001 Applied and its closed history authority |
| `agent/semantic-changes/README.md` | Current | Migrated | Independent SC namespace and Applied lifecycle state are indexed |
| `agent/sessions/README.md` | Current | Migrated | Terminal synchronization and promotion-map policy are current |
| `agent/skills/README.md` | Current | Migrated | Both workflow skills and composition boundaries are indexed |
| `agent/skills/distill-project-knowledge/SKILL.md` | Current | Migrated | Evidence-aware claim routing landed in `1ecdfb0` |
| `agent/skills/distill-project-knowledge/agents/openai.yaml` | Current | Migrated | Exact workflow trigger metadata passed package validation |
| `agent/skills/distill-project-knowledge/references/routing-matrix.md` | Current | Migrated | Each claim type has one canonical owner |
| `agent/skills/govern-semantic-change/SKILL.md` | Current | Migrated | Exact-path migration workflow landed in `1ecdfb0` |
| `agent/skills/govern-semantic-change/agents/openai.yaml` | Current | Migrated | Exact workflow trigger metadata passed package validation |
| `agent/skills/govern-semantic-change/references/protocol.md` | Current | Migrated | SC schema, mixed-file, handoff, and rename rules are current |
| `agent/skills/record-session/SKILL.md` | Current | Migrated | Checkpoint and close now compose with knowledge distillation |
| `agent/skills/start-work/SKILL.md` | Current | Migrated | Cold start loads relevant SC authority before affected edits |
| `agent/skills/trigger-evals.json` | Current | Migrated | Schema v2 covers 11 domain and two workflow skills in 81 cases |
| `agent/skills/trigger-evals.md` | Current | Migrated | Behavioral scoring boundary matches the v2 corpus |
| `agent/templates/README.md` | Current | Migrated | SC template is indexed |
| `agent/templates/checkpoint.md` | Current | Migrated | Checkpoint protection names the exact D0025 route |
| `agent/templates/semantic-change.md` | Current | Migrated | Canonical SC skeleton landed in `1ecdfb0` |
| `agent/templates/session-summary.md` | Current | Migrated | Summary template emits Promoted and Session-only rows |
| `docs/architecture/README.md` | Current | Migrated | D0025 architecture source is indexed |
| `docs/architecture/semantic-change-governance.md` | Current | Migrated | Verified D0025 authority landed in `1ecdfb0` |
| `docs/release-versioning.md` | Current | Migrated | D0024 is the first migration and D0025 owns later replacements |
| `.claude/README.md` | Tooling | Migrated | Bridge documents committed Active SC authorization |
| `.claude/hooks/pre_edit.py` | Tooling | Migrated | Edit-time guard delegates protected paths to the HEAD gate |
| `.claude/hooks/session_start.py` | Tooling | Migrated | Startup banner names the D0025 route |
| `.githooks/pre-commit` | Tooling | Migrated | Hook checks HEAD authorization and the exact staged tree |
| `tests/CMakeLists.txt` | Tooling | Migrated | Protected-history regression suite runs under architecture label |
| `tools/README.md` | Tooling | Migrated | SC, staged-tree, and routed-skill gates are documented |
| `tools/check-agent-records.py` | Tooling | Migrated | Terminal post-cutoff summaries enforce Promoted and Session-only in `90c45ed` |
| `tools/check-semantic-change-edits.py` | Tooling | Migrated | HEAD-only permits, lifecycle, revision, and rename ownership are enforced |
| `tools/check-skill-routing.py` | Tooling | Migrated | Independent workflow roster and bilingual composition floors are enforced |
| `tools/new-session.py` | Tooling | Migrated | New sessions scaffold Promoted and Session-only rows |
| `tools/test-check-agent-records.py` | Tooling | Migrated | Four terminal/active summary-shape regressions pass in the 136-case suite |
| `tools/test-check-skill-routing.py` | Tooling | Migrated | Workflow coverage and observation branches pass 34 cases |
| `tools/test-semantic-change-edits.py` | Tooling | Migrated | HEAD, staged, lifecycle, and rename regressions pass 21 cases |
| `agent/sessions/2026/08/S0100-20260828-001-spec-consistency/summary.md` | Historical | Migrated | Relabeled promotion without changing D0008/D0009 claims or verification |
| `agent/sessions/2026/08/S0100-20260828-002-layout-convergence/summary.md` | Historical | Migrated | Relabeled promotion without changing decision or component evidence |
| `agent/sessions/2026/08/S0100-20260828-003-agent-record-convergence/summary.md` | Historical | Migrated | Relabeled promotion without changing cleanup or gate evidence |
| `agent/sessions/2026/08/S0100-20260828-004-record-gate-hardening/summary.md` | Historical | Migrated | Relabeled promotion without changing validation evidence |
| `agent/sessions/2026/08/S0100-20260828-005-skills-layer/summary.md` | Historical | Migrated | Relabeled promotion without changing skill-boundary evidence |
| `agent/sessions/2026/08/S0100-20260828-006-agent-guidance-hardening/summary.md` | Historical | Migrated | Relabeled promotion without changing guidance evidence |
| `agent/sessions/2026/08/S0100-20260828-007-claude-bridge/summary.md` | Historical | Migrated | Relabeled promotion while retaining the observed old hook behavior |
| `agent/sessions/2026/08/S0100-20260828-010-domain-expert-skills/summary.md` | Historical | Migrated | Relabeled two promotions without changing counts or architecture claims |
| `agent/sessions/2026/08/S0100-20260828-011-skill-design-convergence/summary.md` | Historical | Migrated | Relabeled three promotions without changing routing evidence |
| `agent/sessions/2026/08/S0100-20260829-002-cleanup-stale-routes/summary.md` | Historical | Migrated | Relabeled promotion without changing removed-path evidence |
| `agent/sessions/2026/08/S0100-20260829-004-implementation-session-current-standard/summary.md` | Historical | Migrated | Relabeled promotion without changing 25-to-13 event evidence |
| `agent/sessions/2026/08/S0100-20260829-005-m0100-vertical-slice/summary.md` | Historical | Migrated | Relabeled promotion without changing implementation gates |
| `agent/sessions/2026/08/S0100-20260829-006-stage-breakthrough-commits/summary.md` | Historical | Migrated | Relabeled promotion without changing commit-policy evidence |
| `agent/sessions/2026/08/S0100-20260829-007-pytorch-cuda-test-tool/summary.md` | Historical | Migrated | Relabeled promotion without changing framework profile evidence |
| `agent/sessions/2026/08/S0100-20260830-003-session-guidance-loop/summary.md` | Historical | Migrated | Relabeled promotion without changing guidance lifecycle evidence |
| `agent/sessions/2026/08/S0100-20260830-004-semantic-version-line/summary.md` | Historical | Migrated | Relabeled promotion and D0024 interpretation while locking revisions and counts |
| `agent/sessions/2026/08/S0101-20260828-008-codex-entry-points/summary.md` | Historical | Migrated | Relabeled session-only result without changing external-tool evidence |
| `agent/sessions/2026/08/S0101-20260828-009-implementation-readiness-skill/summary.md` | Historical | Migrated | Relabeled promotion without changing package evidence |
| `agent/sessions/2026/08/S0101-20260828-012-ubuntu-2004-glibc-floor/summary.md` | Historical | Migrated | Relabeled promotion without changing glibc evidence |
| `agent/sessions/2026/08/S0101-20260829-001-toolchain-boundary-correction/summary.md` | Historical | Migrated | Relabeled promotion without changing D0022 evidence |
| `agent/sessions/2026/08/S0101-20260829-003-timezone-mirror-priority/summary.md` | Historical | Migrated | Relabeled promotion while retaining immutable upstream identity wording |
| `agent/sessions/2026/08/S0101-20260830-002-ubuntu-target-sdk-guide/summary.md` | Historical | Migrated | Relabeled three promotions without changing SDK or GLIBC evidence |
| `agent/sessions/2026/08/S0100-20260828-003-agent-record-convergence/notes.md` | Historical | Migrated | Replaced absolute immutability interpretation while retaining bootstrap facts |
| `agent/sessions/2026/08/S0100-20260830-003-session-guidance-loop/notes.md` | Historical | Migrated | Distinguished guidance disposition from claim promotion |
| `agent/sessions/2026/08/S0100-20260830-004-semantic-version-line/events.jsonl` | Historical | Migrated | Changed only seq 2 conclusion tail and locked all event metadata and mappings |
| `agent/sessions/2026/08/S0100-20260830-004-semantic-version-line/notes.md` | Historical | Migrated | Replaced one-time interpretation and locked the product mapping |
| `agent/progress/checkpoints/2026/P20260828-008-codex-entry-points.md` | Historical | Migrated | Replaced absolute checkpoint immutability footer only |
| `agent/progress/checkpoints/2026/P20260828-009-codex-skill-packages.md` | Historical | Migrated | Replaced absolute checkpoint immutability footer only |
| `agent/progress/checkpoints/2026/P20260828-010-domain-expert-skills.md` | Historical | Migrated | Replaced absolute checkpoint immutability footer only |
| `agent/progress/checkpoints/2026/P20260829-003-session-lifecycle-normalization.md` | Historical | Migrated | Clarified terminal protection and exact D0025 exception |
| `agent/progress/checkpoints/2026/P20260830-003-semantic-delivery-migration.md` | Historical | Migrated | Replaced D0024 exclusivity and resume wording while locking all gate counts |
| `agent/progress/checkpoints/2026/P20260828-007-claude-bridge.md` | Historical | Retained evidence | Whole file retains the hook behavior observed at that revision |
| `agent/sessions/2026/08/S0100-20260828-007-claude-bridge/events.jsonl` | Historical | Retained evidence | Whole event retains the exact old decision and metadata |
| `agent/sessions/2026/08/S0100-20260828-007-claude-bridge/notes.md` | Historical | Retained evidence | Whole note retains the old hook review and test observations |
| `agent/sessions/2026/08/S0100-20260829-004-implementation-session-current-standard/events.jsonl` | Historical | Retained evidence | Whole ledger retains exact objective, 25-to-13 counts, revisions, and results |

## Active-session handoff

| Session | Guidance | Status | Outcome |
| --- | --- | --- | --- |
| `S0100-20260828-013-m0100-foundation` | `G001` | Published | Owner validates the new summary and D0025 wording at its next control boundary |
| `S0100-20260830-001-m0100-completion-sprint` | `G003` | Published | Owner processes earlier guidance first, then validates this migration before close |

## Evidence preservation

Migration content revision `90c45eda2815c59617fffea581522b7ed6bff1c0`
applied the content, tooling, and protected-history changes. This separate
record closure synchronizes the replaceable indexes, current progress, and SC
lifecycle. All session JSON, event `schema_version`, `seq`, `timestamp`, `type`,
and `actor` fields, commands, output, counts, revisions, hashes, provenance,
verification tables, and cleanup facts are locked. A mixed summary or
checkpoint is `Migrated` only because its label or current interpretation
changes; its Evidence cell names the facts that stay unchanged. The four
`Retained evidence` files receive zero byte changes.

## Future-agent reminder

When established repository meaning changes, load D0025 and the relevant SC.
Ordinary corrections append; protected history changes only through exact
`Historical + Pending` rows in an Active SC already committed to `HEAD`.
Promote claims to one canonical owner, keep unverified reusable methods as
Candidate experience, and never create a generic distillation archive.

## Verification

| Gate | Result |
| --- | --- |
| Governance content revision | Passed: `1ecdfb01497610c5042e12bda4a16839d2b9c734` |
| Active-session handoff | Passed: G001 and G003 published; existing G002 untouched |
| Exact residual search | Passed: only inventoried retained evidence, the G003 target, explicit anti-forgery policy, and unrelated technical identity wording remain |
| Locked-evidence diff audit | Passed: 31 protected files migrated; four Retained evidence blobs match their committed hashes |
| Agent records and self-tests | Passed: Agent 136, semantic edits 21/21, guidance 17/17, routing 81 and 34/34, skill packages 2/2 |
| Architecture CTest | Passed: Nix dev configure and architecture 6/6 |
