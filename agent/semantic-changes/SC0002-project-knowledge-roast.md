---
id: SC0002
status: Active
created: 2026-08-30
updated: 2026-08-30
decision: D0026
session: S0100-20260830-006-project-knowledge-roast
scope: project-knowledge-roast
history_sync: automatic
effective_revision: null
superseded_by: null
---

# SC0002: Project knowledge roast

## Semantic replacement

- Old meaning: project-knowledge refinement used the implicitly discoverable
  `distill-project-knowledge` skill and one `Distillation` summary section that
  coupled promoted claims with the `Session-only` disposition without recording
  semantic transformation depth.
- New meaning: explicit-only `$roast` classifies only materially promoted durable
  claims as `light roasts`, `medium roasts`, or authorized `dark roasts`; an
  independent lowercase `session-only` section owns local retention reasons.
- Authority: D0026 in `docs/architecture/project-knowledge-roast.md`; D0025
  governs this complete evidence-preserving migration.
- Compatibility consequence: the old skill, callable slug, live schema, labels,
  and implicit route are removed without an alias. Stable historical IDs and
  inventoried raw evidence retain their factual form.

## Migration inventory

| Surface | Class | Disposition | Evidence |
| --- | --- | --- | --- |
| `AGENTS.md` | Current | Pending | Replace the close/handoff workflow owner and summary contract |
| `agent/README.md` | Current | Pending | Replace status rules, read guidance, and durable workflow wording |
| `agent/memory/decisions-index.md` | Current | Pending | Promote D0026 source status only after the implementation gates pass |
| `agent/progress/README.md` | Current | Pending | Index the applied roast migration checkpoint at closure |
| `agent/progress/current.md` | Current | Pending | Replace the active resume point and legacy workflow wording |
| `agent/semantic-changes/README.md` | Current | Pending | Index SC0002 and later close SC0001 as Superseded |
| `agent/sessions/README.md` | Current | Pending | Replace the live summary contract and old skill route |
| `agent/skills/README.md` | Current | Pending | Remove the old package and index explicit-only roast composition |
| `agent/skills/distill-project-knowledge/SKILL.md` | Current | Pending | Remove the obsolete callable skill package |
| `agent/skills/distill-project-knowledge/agents/openai.yaml` | Current | Pending | Remove obsolete UI metadata and implicit invocation surface |
| `agent/skills/distill-project-knowledge/references/routing-matrix.md` | Current | Pending | Remove obsolete package path after routing content moves to roast |
| `agent/skills/roast/SKILL.md` | Current | Pending | Add D0026 classification, disposition, and authority workflow |
| `agent/skills/roast/agents/openai.yaml` | Current | Pending | Add explicit-only `$roast` UI and policy metadata |
| `agent/skills/roast/references/routing-matrix.md` | Current | Pending | Preserve canonical-owner routing under the new semantic model |
| `agent/skills/record-session/SKILL.md` | Current | Pending | Compose exact `$roast` at breakthrough, handoff, and close |
| `agent/skills/start-work/SKILL.md` | Current | Pending | Compose exact `$roast` at the repository control boundary |
| `agent/skills/trigger-evals.json` | Current | Pending | Replace workflow slug/cases and add explicit/near-miss coverage |
| `agent/skills/trigger-evals.md` | Current | Pending | Document explicit workflow scoring and current roster |
| `agent/templates/session-summary.md` | Current | Pending | Emit the ordered roast buckets and independent session-only section |
| `docs/architecture/README.md` | Current | Pending | Promote D0026 only after complete implementation verification |
| `docs/architecture/project-knowledge-roast.md` | Current | Pending | Promote Proposed to Verified and remove live legacy terminology |
| `agent/skills/session-guidance/references/protocol.md` | Current | Migrated | Revision `a2df763` reserves guidance IDs referenced by SC handoff rows |
| `tools/README.md` | Tooling | Pending | Document the roast schema, package policy, and validation boundary |
| `tools/check-agent-records.py` | Tooling | Pending | Enforce exact roast/session-only shape and explicit skill policy |
| `tools/new-session.py` | Tooling | Pending | Scaffold the new active-session summary contract |
| `tools/test-check-agent-records.py` | Tooling | Pending | Cover strict schema, legacy rejection, and explicit policy regressions |
| `tools/test-check-skill-routing.py` | Tooling | Pending | Replace old workflow identities and verify explicit routing cases |
| `agent/skills/session-guidance/scripts/guidance.py` | Tooling | Migrated | Revision `a2df763` allocates after packet, event, and SC reservations |
| `agent/skills/session-guidance/scripts/test_guidance.py` | Tooling | Migrated | 20/20 tests cover reusable no-material IDs and durable G002/G004 allocation |
| `agent/semantic-changes/SC0001-semantic-change-distillation.md` | Historical | Pending | Supersede only its lifecycle fields; lock effective revision, inventory, and factual migration evidence |
| `agent/progress/checkpoints/2026/P20260828-003-agent-record-convergence.md` | Historical | Pending | Replace the old summary-gate interpretation without changing recorded counts |
| `agent/progress/checkpoints/2026/P20260828-004-record-gate-hardening.md` | Historical | Pending | Replace the old validator label without changing its observed gate evidence |
| `agent/progress/checkpoints/2026/P20260828-005-skills-layer.md` | Historical | Pending | Replace obsolete skill-layer terminology without changing package evidence |
| `agent/progress/checkpoints/2026/P20260830-004-semantic-change-governance.md` | Historical | Pending | Replace the old workflow name while locking revision and gate results |
| `agent/progress/checkpoints/2026/P20260830-005-semantic-change-distillation-applied.md` | Historical | Pending | Mark the prior promotion model superseded while locking SC0001 evidence |
| `agent/sessions/2026/08/S0100-20260828-001-spec-consistency/summary.md` | Historical | Pending | Reclassify its promotion without changing D0008/D0009 or verification facts |
| `agent/sessions/2026/08/S0100-20260828-002-layout-convergence/summary.md` | Historical | Pending | Reclassify its promotion without changing layout evidence |
| `agent/sessions/2026/08/S0100-20260828-003-agent-record-convergence/summary.md` | Historical | Pending | Reclassify its promotion without changing record-gate evidence |
| `agent/sessions/2026/08/S0100-20260828-004-record-gate-hardening/summary.md` | Historical | Pending | Reclassify its promotion without changing validation evidence |
| `agent/sessions/2026/08/S0100-20260828-005-skills-layer/summary.md` | Historical | Pending | Reclassify its promotion without changing skill package evidence |
| `agent/sessions/2026/08/S0100-20260828-006-agent-guidance-hardening/summary.md` | Historical | Pending | Reclassify its promotion without changing guidance evidence |
| `agent/sessions/2026/08/S0100-20260828-007-claude-bridge/summary.md` | Historical | Pending | Reclassify its promotion while preserving observed bridge behavior |
| `agent/sessions/2026/08/S0100-20260828-010-domain-expert-skills/summary.md` | Historical | Pending | Reclassify its promotions without changing routing counts or results |
| `agent/sessions/2026/08/S0100-20260828-011-skill-design-convergence/summary.md` | Historical | Pending | Reclassify its promotions without changing package and test evidence |
| `agent/sessions/2026/08/S0100-20260829-002-cleanup-stale-routes/summary.md` | Historical | Pending | Reclassify its promotion without changing removed-path evidence |
| `agent/sessions/2026/08/S0100-20260829-004-implementation-session-current-standard/summary.md` | Historical | Pending | Reclassify its promotion without changing 25-to-13 event evidence |
| `agent/sessions/2026/08/S0100-20260829-005-m0100-vertical-slice/summary.md` | Historical | Pending | Reclassify its promotion without changing implementation gates |
| `agent/sessions/2026/08/S0100-20260829-006-stage-breakthrough-commits/summary.md` | Historical | Pending | Reclassify its promotion without changing commit-policy evidence |
| `agent/sessions/2026/08/S0100-20260829-007-pytorch-cuda-test-tool/summary.md` | Historical | Pending | Reclassify its promotion without changing framework-profile evidence |
| `agent/sessions/2026/08/S0100-20260830-003-session-guidance-loop/summary.md` | Historical | Pending | Reclassify its promotion without changing guidance lifecycle evidence |
| `agent/sessions/2026/08/S0100-20260830-004-semantic-version-line/summary.md` | Historical | Pending | Reclassify its promotion while locking D0024 mappings, revisions, and counts |
| `agent/sessions/2026/08/S0100-20260830-005-semantic-change-distillation/summary.md` | Historical | Pending | Reclassify D0025/SC0001 outcomes while locking migration evidence |
| `agent/sessions/2026/08/S0101-20260828-008-codex-entry-points/summary.md` | Historical | Pending | Move its local-only claim outside roast without changing external-tool evidence |
| `agent/sessions/2026/08/S0101-20260828-009-implementation-readiness-skill/summary.md` | Historical | Pending | Reclassify its promotion without changing package evidence |
| `agent/sessions/2026/08/S0101-20260828-012-ubuntu-2004-glibc-floor/summary.md` | Historical | Pending | Reclassify its promotion without changing glibc evidence |
| `agent/sessions/2026/08/S0101-20260829-001-toolchain-boundary-correction/summary.md` | Historical | Pending | Reclassify its promotion without changing D0022 evidence |
| `agent/sessions/2026/08/S0101-20260829-003-timezone-mirror-priority/summary.md` | Historical | Pending | Reclassify its promotion while retaining frozen upstream identity evidence |
| `agent/sessions/2026/08/S0101-20260830-002-ubuntu-target-sdk-guide/summary.md` | Historical | Pending | Reclassify its promotions without changing SDK or GLIBC evidence |
| `agent/sessions/2026/08/S0100-20260828-003-agent-record-convergence/notes.md` | Historical | Pending | Replace the old grandfathering interpretation without changing bootstrap facts |
| `agent/sessions/2026/08/S0100-20260828-005-skills-layer/notes.md` | Historical | Pending | Replace obsolete workflow terminology without changing the candidate-skill observation |
| `agent/sessions/2026/08/S0100-20260829-006-stage-breakthrough-commits/notes.md` | Historical | Pending | Replace the old close-workflow name without changing lifecycle facts |
| `agent/sessions/2026/08/S0100-20260828-003-agent-record-convergence/events.jsonl` | Historical | Retained evidence | Whole event log retains the observed pre-D0026 implementation facts and metadata |
| `agent/sessions/2026/08/S0100-20260828-004-record-gate-hardening/events.jsonl` | Historical | Retained evidence | Whole event log retains exact old validator commands, results, and metadata |
| `agent/sessions/2026/08/S0100-20260829-004-implementation-session-current-standard/events.jsonl` | Historical | Retained evidence | Whole event log retains exact cleanup objective, counts, and results |
| `agent/sessions/2026/08/S0100-20260830-005-semantic-change-distillation/events.jsonl` | Historical | Retained evidence | Whole event log retains exact D0025/SC0001 migration facts and metadata |
| `agent/sessions/2026/08/S0100-20260830-005-semantic-change-distillation/session.json` | Historical | Retained evidence | Stable session identity and referenced paths remain byte-for-byte unchanged |
| `agent/sessions/2026/08/S0100-20260830-006-project-knowledge-roast/summary.md` | Active session | Pending | Migration owner adopts the final roast/session-only contract before closure |
| `agent/sessions/2026/08/S0100-20260830-006-project-knowledge-roast/events.jsonl` | Active session | Retained evidence | Objective and decision events preserve the exact replacement rationale already recorded |

## Active-session handoff

| Session | Guidance | Status | Outcome |
| --- | --- | --- | --- |
| `S0100-20260828-013-m0100-foundation` | `G002` | Published | Owner migrates its summary under D0026; packet supersedes cleaned G001 |
| `S0100-20260830-001-m0100-completion-sprint` | `G004` | Published | Owner folds duplicate live knowledge sections into D0026; packet supersedes cleaned G003 |

## Evidence preservation

All session JSON and event `schema_version`, `seq`, `timestamp`, `type`, and
`actor` fields, commands, output, counts, revisions, hashes, provenance,
verification tables, product results, and cleanup facts are locked. A mixed
summary, note, checkpoint, or SC is `Migrated` only because its current label,
record shape, or interpretation changes. The five Historical `Retained
evidence` files and the migration owner's existing event log receive zero byte
changes.

## Future-agent reminder

Invoke `$roast` explicitly only at a material project-knowledge boundary. First
choose disposition; only a materially created or updated canonical claim enters
one roast depth. `session-only` remains independent. Every dark roast resolves
to a decision, while D0025 independently requires an Active SC for any breaking
replacement regardless of roast depth. Never recreate the old callable skill,
mixed summary schema, alias, or generic archive.

## Verification

| Gate | Result |
| --- | --- |
| D0026 decision revisions | Passed: `21f1d47cd0522682e4047542fc53627bff2967fe`, refined by `5b9e0d3` |
| Guidance ID reservation | Passed: 20/20 self-tests; repository probes allocate G002 and G004 |
| Active-session handoff | Passed: G002 and G004 published as ready packets |
| Exact residual search | Pending |
| Locked-evidence diff audit | Pending |
| Agent, skill, semantic-change, guidance, and routing gates | Pending |
| Architecture CTest | Pending |
