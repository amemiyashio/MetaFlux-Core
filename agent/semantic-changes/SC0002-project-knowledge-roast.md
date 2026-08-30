---
id: SC0002
status: Applied
created: 2026-08-30
updated: 2026-08-30
decision: D0026
session: S0100-20260830-006-project-knowledge-roast
scope: project-knowledge-roast
history_sync: automatic
effective_revision: 991e5327c8a3b1f5d05112f895011f8d83f0bff0
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
| `AGENTS.md` | Current | Migrated | Replaced the close/handoff workflow owner and summary contract |
| `agent/README.md` | Current | Migrated | Replaced status rules, read guidance, and durable workflow wording |
| `agent/memory/decisions-index.md` | Current | Migrated | Promoted D0026 source status after the implementation gates passed |
| `agent/progress/README.md` | Current | Migrated | Indexed the applied roast migration checkpoint at closure |
| `agent/progress/current.md` | Current | Migrated | Replaced the active resume point and legacy workflow wording |
| `agent/semantic-changes/README.md` | Current | Migrated | Indexed SC0002 Applied while retaining SC0001 Applied for D0025 governance |
| `agent/sessions/README.md` | Current | Migrated | Replaced the live summary contract and old skill route |
| `agent/skills/README.md` | Current | Migrated | Removed the old package and indexed explicit-only roast composition |
| `agent/skills/distill-project-knowledge/SKILL.md` | Current | Removed | Obsolete callable package absent from revision `29a4e38` |
| `agent/skills/distill-project-knowledge/agents/openai.yaml` | Current | Removed | Obsolete UI metadata and implicit route absent from revision `29a4e38` |
| `agent/skills/distill-project-knowledge/references/routing-matrix.md` | Current | Removed | Canonical-owner routing now exists only under the roast package |
| `agent/skills/roast/SKILL.md` | Current | Migrated | Added D0026 classification, disposition, and authority workflow |
| `agent/skills/roast/agents/openai.yaml` | Current | Migrated | Added explicit-only `$roast` UI and policy metadata |
| `agent/skills/roast/references/routing-matrix.md` | Current | Migrated | Preserved canonical-owner routing under the new semantic model |
| `agent/skills/record-session/SKILL.md` | Current | Migrated | Composed exact `$roast` at breakthrough, handoff, and close |
| `agent/skills/start-work/SKILL.md` | Current | Migrated | Composed exact `$roast` at the repository control boundary |
| `agent/skills/trigger-evals.json` | Current | Migrated | Replaced workflow slug/cases and added explicit/near-miss coverage |
| `agent/skills/trigger-evals.md` | Current | Migrated | Documented explicit workflow scoring and current roster |
| `agent/templates/session-summary.md` | Current | Migrated | Emitted the ordered roast buckets and independent session-only section |
| `docs/architecture/README.md` | Current | Migrated | Promoted D0026 after complete implementation verification |
| `docs/architecture/project-knowledge-roast.md` | Current | Migrated | Promoted Proposed to Verified and removed live legacy terminology |
| `agent/skills/session-guidance/references/protocol.md` | Current | Migrated | Revision `a2df763` reserves guidance IDs referenced by SC handoff rows |
| `tools/README.md` | Tooling | Migrated | Documented the roast schema, package policy, and validation boundary |
| `tools/check-agent-records.py` | Tooling | Migrated | Revision `991e532` enforces exact roast/session-only shape, one resolvable owner, and explicit skill policy |
| `tools/new-session.py` | Tooling | Migrated | Scaffolded the new active-session summary contract |
| `tools/test-check-agent-records.py` | Tooling | Migrated | Revision `991e532` covers strict schema, owner resolution, legacy rejection, and explicit policy in 163 cases |
| `tools/test-check-skill-routing.py` | Tooling | Migrated | Replaced old workflow identities and verified explicit routing cases |
| `agent/skills/session-guidance/scripts/guidance.py` | Tooling | Migrated | Revision `a2df763` allocates after packet, event, and SC reservations |
| `agent/skills/session-guidance/scripts/test_guidance.py` | Tooling | Migrated | 20/20 tests cover reusable no-material IDs and durable G002/G004 allocation |
| `agent/semantic-changes/SC0001-semantic-change-distillation.md` | Historical | Retained evidence | Whole Applied record remains factual D0025 governance evidence; SC0002 replaces only its promotion-model consequence on live surfaces |
| `agent/progress/checkpoints/2026/P20260828-003-agent-record-convergence.md` | Historical | Migrated | Replaced the old summary-gate interpretation without changing recorded counts |
| `agent/progress/checkpoints/2026/P20260828-004-record-gate-hardening.md` | Historical | Migrated | Replaced the old validator label without changing its observed gate evidence |
| `agent/progress/checkpoints/2026/P20260828-005-skills-layer.md` | Historical | Migrated | Replaced obsolete skill-layer terminology without changing package evidence |
| `agent/progress/checkpoints/2026/P20260830-004-semantic-change-governance.md` | Historical | Migrated | Replaced the old workflow name while locking revision and gate results |
| `agent/progress/checkpoints/2026/P20260830-005-semantic-change-distillation-applied.md` | Historical | Migrated | Marked the prior promotion model superseded while locking SC0001 evidence |
| `agent/sessions/2026/08/S0100-20260828-001-spec-consistency/summary.md` | Historical | Migrated | Reclassified its promotion without changing D0008/D0009 or verification facts |
| `agent/sessions/2026/08/S0100-20260828-002-layout-convergence/summary.md` | Historical | Migrated | Reclassified its promotion without changing layout evidence |
| `agent/sessions/2026/08/S0100-20260828-003-agent-record-convergence/summary.md` | Historical | Migrated | Reclassified its promotion without changing record-gate evidence |
| `agent/sessions/2026/08/S0100-20260828-004-record-gate-hardening/summary.md` | Historical | Migrated | Reclassified its promotion without changing validation evidence |
| `agent/sessions/2026/08/S0100-20260828-005-skills-layer/summary.md` | Historical | Migrated | Reclassified its promotion without changing skill package evidence |
| `agent/sessions/2026/08/S0100-20260828-006-agent-guidance-hardening/summary.md` | Historical | Migrated | Reclassified its promotion without changing guidance evidence |
| `agent/sessions/2026/08/S0100-20260828-007-claude-bridge/summary.md` | Historical | Migrated | Reclassified its promotion while preserving observed bridge behavior |
| `agent/sessions/2026/08/S0100-20260828-010-domain-expert-skills/summary.md` | Historical | Migrated | Reclassified its promotions without changing routing counts or results |
| `agent/sessions/2026/08/S0100-20260828-011-skill-design-convergence/summary.md` | Historical | Migrated | Reclassified its promotions without changing package and test evidence |
| `agent/sessions/2026/08/S0100-20260829-002-cleanup-stale-routes/summary.md` | Historical | Migrated | Reclassified its promotion without changing removed-path evidence |
| `agent/sessions/2026/08/S0100-20260829-004-implementation-session-current-standard/summary.md` | Historical | Migrated | Reclassified its promotion without changing 25-to-13 event evidence |
| `agent/sessions/2026/08/S0100-20260829-005-m0100-vertical-slice/summary.md` | Historical | Migrated | Reclassified its promotion without changing implementation gates |
| `agent/sessions/2026/08/S0100-20260829-006-stage-breakthrough-commits/summary.md` | Historical | Migrated | Reclassified its promotion without changing commit-policy evidence |
| `agent/sessions/2026/08/S0100-20260829-007-pytorch-cuda-test-tool/summary.md` | Historical | Migrated | Reclassified its promotion without changing framework-profile evidence |
| `agent/sessions/2026/08/S0100-20260830-003-session-guidance-loop/summary.md` | Historical | Migrated | Reclassified its promotion without changing guidance lifecycle evidence |
| `agent/sessions/2026/08/S0100-20260830-004-semantic-version-line/summary.md` | Historical | Migrated | Reclassified its promotion while locking D0024 mappings, revisions, and counts |
| `agent/sessions/2026/08/S0100-20260830-005-semantic-change-distillation/summary.md` | Historical | Migrated | Reclassified D0025/SC0001 outcomes while locking migration evidence |
| `agent/sessions/2026/08/S0101-20260828-008-codex-entry-points/summary.md` | Historical | Migrated | Moved its local-only claim outside roast and replaced the later-removed Nix owner without changing external-tool evidence |
| `agent/sessions/2026/08/S0101-20260828-009-implementation-readiness-skill/summary.md` | Historical | Migrated | Reclassified its promotion without changing package evidence |
| `agent/sessions/2026/08/S0101-20260828-012-ubuntu-2004-glibc-floor/summary.md` | Historical | Migrated | Reclassified its promotion without changing glibc evidence |
| `agent/sessions/2026/08/S0101-20260829-001-toolchain-boundary-correction/summary.md` | Historical | Migrated | Reclassified its promotion without changing D0022 evidence |
| `agent/sessions/2026/08/S0101-20260829-003-timezone-mirror-priority/summary.md` | Historical | Migrated | Reclassified its promotion while retaining frozen upstream identity evidence |
| `agent/sessions/2026/08/S0101-20260830-002-ubuntu-target-sdk-guide/summary.md` | Historical | Migrated | Reclassified its promotions without changing SDK or GLIBC evidence |
| `agent/sessions/2026/08/S0100-20260828-003-agent-record-convergence/notes.md` | Historical | Migrated | Replaced the old grandfathering interpretation without changing bootstrap facts |
| `agent/sessions/2026/08/S0100-20260828-005-skills-layer/notes.md` | Historical | Migrated | Replaced obsolete workflow terminology without changing the candidate-skill observation |
| `agent/sessions/2026/08/S0100-20260829-006-stage-breakthrough-commits/notes.md` | Historical | Migrated | Replaced the old close-workflow name without changing lifecycle facts |
| `agent/sessions/2026/08/S0100-20260828-003-agent-record-convergence/events.jsonl` | Historical | Retained evidence | Whole event log retains the observed pre-D0026 implementation facts and metadata |
| `agent/sessions/2026/08/S0100-20260828-004-record-gate-hardening/events.jsonl` | Historical | Retained evidence | Whole event log retains exact old validator commands, results, and metadata |
| `agent/sessions/2026/08/S0100-20260829-004-implementation-session-current-standard/events.jsonl` | Historical | Retained evidence | Whole event log retains exact cleanup objective, counts, and results |
| `agent/sessions/2026/08/S0100-20260830-005-semantic-change-distillation/events.jsonl` | Historical | Retained evidence | Whole event log retains exact D0025/SC0001 migration facts and metadata |
| `agent/sessions/2026/08/S0100-20260830-005-semantic-change-distillation/session.json` | Historical | Retained evidence | Stable session identity and referenced paths remain byte-for-byte unchanged |
| `agent/sessions/2026/08/S0100-20260830-006-project-knowledge-roast/summary.md` | Active session | Migrated | Migration owner adopted the final roast/session-only contract before closure |
| `agent/sessions/2026/08/S0100-20260830-006-project-knowledge-roast/events.jsonl` | Active session | Retained evidence | Objective and decision events preserve the exact replacement rationale already recorded |

## Active-session handoff

| Session | Guidance | Status | Outcome |
| --- | --- | --- | --- |
| `S0100-20260828-013-m0100-foundation` | `G002` | Resolved | Adopted at event seq 15; exact D0026 summary landed and the packet was removed |
| `S0100-20260830-001-m0100-completion-sprint` | `G004` | Resolved | Adopted at event seq 20; duplicate live sections were replaced and the packet was removed |

## Evidence preservation

Initial content revision `29a4e38f3a9097ea3680ed7aa98f67ed41105b2e`
applied the complete schema, skill, routing, and evidence-preserving migration.
Effective content revision `991e5327c8a3b1f5d05112f895011f8d83f0bff0`
then split compound promotions, replaced descriptive or stale owners, and made
single-owner resolution a machine gate. This separate record closure
synchronizes replaceable indexes, current progress, and SC/session lifecycle.
All session JSON and event `schema_version`, `seq`,
`timestamp`, `type`, and `actor` fields, commands, output, counts, revisions,
hashes, provenance, verification tables, product results, and cleanup facts are
locked. A mixed summary, note, checkpoint, or SC is `Migrated` only because its
current label, record shape, or interpretation changes. The six Historical
`Retained evidence` files and the migration owner's existing event log received
zero byte changes.

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
| Active-session handoff | Passed: G002/G004 adopted at event seq 15/20; both guidance inboxes empty |
| Exact residual search | Passed: no old callable package or live summary schema; only registered stable history, migration-audit text, and anti-forgery gates retain old identities |
| Locked-evidence diff audit | Passed: `agent/sessions/2026/08/S0100-20260830-006-project-knowledge-roast/notes.md` records the 17-file Group A, 14-file Group B, and seven-file retained-evidence audits against `faf6f6f` |
| Agent, skill, semantic-change, guidance, and routing gates | Passed: repository 27/210/205; Agent self-test 163/163; semantic edits 21/21; guidance 20/20; routing 82 and 34/34; all skill packages valid |
| Independent roast forward review | Passed: migration-session `notes.md` records exact A-E inputs and observations for light, medium, session-only, omission, and unauthorized replacement |
| Architecture CTest | Passed: Nix dev shell, architecture 6/6 |
