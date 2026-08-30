---
id: SC0003
status: Applied
created: 2026-08-30
updated: 2026-08-30
decision: D0027
session: S1000-20260830-007-v100-qualification-boundary
scope: v100-qualification-boundary
history_sync: automatic
effective_revision: d9c67e47eff7ae17a4b6404412c946c81544afe2
superseded_by: null
---

# SC0003: v1.0 qualification boundary

## Semantic replacement

- Old meaning: D0023 and D0024 assigned Intel x86_64 host qualification,
  physical NVIDIA binding-performance promotion, and native NixOS VM/package
  qualification together to an unallocated `v0.2.0` support expansion, while
  `v1.0.0` remained unassigned.
- New meaning: D0027 assigns Intel x86_64 support qualification and physical
  NVIDIA binding-performance promotion to M1000 / `v1.0.0`; native NixOS
  VM/package qualification alone retains its `v0.2.0` assignment.
- Authority: D0027 in
  `agent/plan/M1000-stable-qualification/plan.md#release-boundary-decision-d0027`.
- Compatibility consequence: M0100 remains AMD-qualified with provisional
  budgets, no absent hardware evidence becomes a pass, and future planning,
  active handoffs, and historical interpretations use the split destination.

## Migration inventory

| Surface | Class | Disposition | Evidence |
| --- | --- | --- | --- |
| `docs/release-versioning.md` | Current | Migrated | D0027 assigns M1000 and preserves the unallocated native-NixOS v0.2.0 line |
| `agent/plan/README.md` | Current | Migrated | M1000 release row resolves to delivery 1.0.0.0 |
| `agent/plan/M1000-stable-qualification/plan.md` | Current | Migrated | Canonical D0027 owner defines scope, work split, acceptance, and DoD |
| `agent/plan/M1000-stable-qualification/work/W1001-intel-host-support.md` | Current | Migrated | Delivery 1.0.0.1 owns Intel x86_64 support qualification |
| `agent/plan/M1000-stable-qualification/work/W1002-nvidia-binding-performance.md` | Current | Migrated | Delivery 1.0.0.2 owns strict physical NVIDIA binding evidence |
| `agent/plan/M1000-stable-qualification/work/W1003-stable-release.md` | Current | Migrated | Delivery 1.0.0.3 owns the major-release compatibility and release gate |
| `agent/memory/decisions-index.md` | Current | Migrated | D0027 resolves and the limited D0023/D0024 supersession is explicit |
| `agent/memory/open-decisions.md` | Current | Migrated | M1000 support, hardware, and stable-surface choices have closure conditions |
| `agent/semantic-changes/README.md` | Current | Migrated | SC0003 Active row resolves to this migration permit |
| `agent/memory/project.md` | Current | Migrated | Split current product-line destination without changing M0100 maturity facts |
| `agent/memory/constraints.md` | Current | Migrated | Replace only Intel/NVIDIA destination constraints; retain native NixOS v0.2.0 |
| `agent/plan/M0100-core-foundation/plan.md` | Current | Migrated | Replace v0.2.0 Intel/NVIDIA references while retaining M0100 exclusions and evidence |
| `agent/plan/M0100-core-foundation/work/W0101-build-toolchain.md` | Current | Migrated | D0023 future destination becomes M1000; native NixOS paragraph stays v0.2.0 |
| `agent/plan/M0100-core-foundation/work/W0106-modes-release.md` | Current | Migrated | Physical binding moves to M1000; M0100 provisional and NixOS boundaries remain |
| `agent/progress/current.md` | Current | Migrated | Refresh current destination and next work without altering recorded gate results |
| `agent/sessions/README.md` | Current | Migrated | Mark the terminal D0024 session description as its capture-time boundary |
| `tests/performance/README.md` | Tooling | Migrated | Strict binding harness destination becomes M1000 without weakening incomplete behavior |
| `agent/progress/checkpoints/2026/P20260828-001-spec-consistency-revisions.md` | Historical | Migrated | Replace only the old physical-NVIDIA destination; lock identity, revisions, and verification |
| `agent/progress/checkpoints/2026/P20260828-002-layout-convergence.md` | Historical | Migrated | Replace only the old physical-NVIDIA destination; lock recorded layout evidence |
| `agent/progress/checkpoints/2026/P20260828-003-agent-record-convergence.md` | Historical | Migrated | Replace only the old physical-NVIDIA destination; lock record counts and results |
| `agent/progress/checkpoints/2026/P20260828-004-record-gate-hardening.md` | Historical | Migrated | Replace only the old physical-NVIDIA destination; lock gate evidence |
| `agent/progress/checkpoints/2026/P20260828-005-skills-layer.md` | Historical | Migrated | Replace only the old physical-NVIDIA destination; lock skill evidence |
| `agent/progress/checkpoints/2026/P20260829-001-toolchain-boundary-correction.md` | Historical | Migrated | Mark the D0023 Intel destination superseded; lock tool and disk observations |
| `agent/progress/checkpoints/2026/P20260829-002-stale-route-cleanup.md` | Historical | Migrated | Split Intel/NVIDIA destination; lock cleanup and validation facts |
| `agent/progress/checkpoints/2026/P20260829-004-glibc-floor-qualification.md` | Historical | Migrated | Move Intel only; retain native NixOS v0.2.0 and GLIBC evidence |
| `agent/progress/checkpoints/2026/P20260829-006-cgroup-cpuset-fix-and-implementation-audit.md` | Historical | Migrated | Split Intel/NVIDIA from native NixOS; lock AMD and test facts |
| `agent/progress/checkpoints/2026/P20260830-001-m0100-completion-sprint.md` | Historical | Migrated | Mark Intel destination superseded while preserving no-host and 62/62 facts |
| `agent/progress/checkpoints/2026/P20260830-003-semantic-delivery-migration.md` | Historical | Migrated | Mark the former three-gate v0.2.0 boundary partially superseded; lock migration counts |
| `agent/progress/checkpoints/2026/P20260830-005-semantic-change-distillation-applied.md` | Historical | Migrated | Split the old boundary while locking SC0001 revisions and gates |
| `agent/progress/checkpoints/2026/P20260830-007-project-knowledge-roast-applied.md` | Historical | Migrated | Split the old boundary while locking SC0002 revisions and gates |
| `agent/sessions/2026/08/S0100-20260830-004-semantic-version-line/summary.md` | Historical | Migrated | Mark its former boundary as capture-time and superseded; lock verification and cleanup facts |
| `agent/sessions/2026/08/S0100-20260830-004-semantic-version-line/events.jsonl` | Historical | Migrated | Preserve event metadata and facts while appending the later D0027 interpretation to affected content |
| `agent/sessions/2026/08/S0100-20260829-005-m0100-vertical-slice/summary.md` | Historical | Migrated | Mark its W0101 Intel/NixOS unresolved assignment as capture-time only; lock test evidence |
| `agent/sessions/2026/08/S0100-20260829-005-m0100-vertical-slice/events.jsonl` | Historical | Retained evidence | Whole event log remains byte-for-byte capture-time implementation evidence |
| `agent/sessions/2026/08/S1000-20260830-007-v100-qualification-boundary/session.json` | Active session | Migrated | M1000/W1001-W1003 identities resolve; terminal lifecycle remains session-owned |
| `agent/sessions/2026/08/S1000-20260830-007-v100-qualification-boundary/events.jsonl` | Active session | Migrated | Objective and D0027 activation decision record the exact replacement boundary |
| `agent/sessions/2026/08/S1000-20260830-007-v100-qualification-boundary/summary.md` | Active session | Migrated | Final summary maps the dark roast to D0027/SC0003 and names verification |
| `agent/sessions/2026/08/S1000-20260830-007-v100-qualification-boundary/notes.md` | Active session | Migrated | Locked-evidence and residual-search audits are recorded |

## Active-session handoff

| Session | Guidance | Status | Outcome |
| --- | --- | --- | --- |
| `S0100-20260828-013-m0100-foundation` | `G003` | Resolved | Adopted at event seq 16; AMD observations remain and the raw packet was removed |
| `S0100-20260830-001-m0100-completion-sprint` | `G005` | Resolved | Adopted at event seq 21; release evidence remains and the raw packet was removed |

## Evidence preservation

Every historical timestamp, command, output, count, revision, hash, provenance
identity, hardware observation, test result, cleanup fact, and guidance
disposition is locked. Historical prose changes only to state that its former
`v0.2.0` destination was the capture-time policy and that D0027 later superseded
the Intel/physical-NVIDIA allocation. The retained vertical-slice event log is
not edited. No migration text may claim Intel or physical NVIDIA qualification
passed.

## Future-agent reminder

When Intel x86_64 support or physical NVIDIA binding-performance qualification
is scheduled, load M1000 and D0027: both belong to `v1.0.0`. Native NixOS
VM/package qualification remains the unallocated `v0.2.0` expansion. M0100
stays AMD-qualified with provisional budgets; old hardware absence never becomes
new evidence.

## Verification

| Gate | Result |
| --- | --- |
| Decision content revision | Passed at `7baef42ff23293aa0b93093071422acbfbe0b4ed` |
| Active authorization revision | Passed at `d8709e8309b6126c10c1d3a9bec38c7829a8ae42` |
| Active-session handoff | G003 and G005 adopted once, resolved, and removed; guidance self-test 20/20 passed |
| Exact residual search | Passed; no current marker still assigns Intel or physical NVIDIA qualification to v0.2.0 |
| Locked-evidence diff audit | Passed: 13/13 protected checkpoints retained identity and verification; retained event hash is `146f370624e436221ad3596d7b356b4b23409a66` |
| Agent records and self-tests | Passed: repository 28 sessions / 214 events / 212 Markdown at migration commit and 28 / 215 / 213 at record closure; validator 163/163 |
| Semantic-change edit gate | Passed staged-tree gate and 21/21 self-tests |
| Architecture CTest | Passed 6/6 through the Nix development shell |
| Git diff audit | Passed for migration content revision `d9c67e47eff7ae17a4b6404412c946c81544afe2` |
