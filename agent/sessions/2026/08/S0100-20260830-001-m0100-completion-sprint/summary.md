# Session Summary

## Objective and outcome

Complete M0100 Core Foundation: close W0101-W0106 acceptance gates, deliver
runnable CPU-backed CUDA/NVML Add/Copy vertical slice with release evidence,
process G001/G002/G003 guidance, and execute signed provenance verification.

Outcome: all M0100 acceptance gates pass. Complete release matrix (8/8 × 2)
includes CUDA Add/Copy acceptance on all four distributions. Signed SDK
provenance verified. v0.1.0 boundary confirmed; Intel/NVIDIA/NixOS deferred
to v0.2.0 per D0024.

## Promoted claim-to-owner rows

| Claim | Owner evidence |
|---|---|
| PGO training + USE build pass | `run_m0001_optimization.py` 135 commands, `/tmp/metaflux-pgo-evidence-10/` |
| O2/O3 variant comparison pass | `run_m0001_optimization.py` 28 commands |
| ASan/UBSan hardening pass | `run_m0001_optimization.py` 121 commands |
| W0102 stress coverage complete | `recovery_model.cpp`, `registry_recovery.cpp`, `multiprocess_stress.cpp`, `ring.c`, `noop_stress.c` — all 15 sub-items covered |
| W0106 coexistence tests | `provider_mode_test.c` — managed-only, isolation, recursion prevention |
| Generic release build | `tools/build-generic-release.sh` — GLIBC_2.29 ≤ 2.31 |
| CMake target toolchain | `cmake/toolchains/ubuntu-20.04-generic.cmake` |
| Provider release matrix 8/8 | `run_provider_package_matrix.py` — 4 distros × 2 formats |
| Complete release matrix 8/8 × 2 | `run_release_package_matrix.py` — CUDA Add/Copy acceptance |
| Signed provenance verification | `tools/verify-target-sdk-provenance.sh` — 2 releases, 10 packages |
| D0023 Intel deferral | `decisions-index.md` — v0.2.0 scope |
| D0024/D0025 migration | Semantic identity and governance applied |

## Session-only retention reasons

- PGO evidence directories (`/tmp/metaflux-pgo-evidence-*`): retained for
  audit but not committed; superseded by latest run.
- Package build artifacts (`/tmp/metaflux-*-packages`): retained for matrix
  reproducibility but not committed.
- Acceptance build tree (`.metaflux-build/MetaFlux-Core/generic-acceptance`):
  retained for fixture reuse but not committed.

## Verification

| Command/gate | Result |
|---|---|
| Dev CTest | 64/64 pass |
| Agent records | ok (26 sessions, 201 events) |
| PGO training + USE build | 135 commands, status=pass |
| O2/O3 variant | 28 commands, status=pass |
| ASan/UBSan hardening | 121 commands, status=pass |
| Provider release matrix | 8/8 pass |
| Complete release matrix (run 1) | 8/8 pass |
| Complete release matrix (run 2) | 8/8 pass |
| Signed provenance verification | passed |
| G001 guidance disposition | adopted |
| G002 guidance disposition | adopted |
| G003 guidance disposition | adopted |

## Cleanup

- Removed: G001 guidance packet (resolved), `/tmp/metaflux-pgo-evidence-1` through `-9` (superseded).
- Retained: latest PGO evidence, package artifacts, acceptance build tree.

## Decisions and experience

- D0023: Intel host qualification deferred to v0.2.0.
- D0024: Breaking semantic-identity migration (M0001→M0100).
- D0025: Semantic change governance.
- G001 (previous session): adopted — distinguished provider vs complete matrix.
- G002: adopted — v0.1.0 boundary confirmed, deferred work names v0.2.0.
- G003: adopted — D0025 format applied to this summary.

## Distillation

- Promoted: PGO, release matrix, provenance verification, and v0.1.0 boundary
  evidence into progress/current.md Recorded M0100 Evidence table.
- Promoted: D0023 Intel deferral into decisions-index.md.

## Unresolved items

None for v0.1.0. The following are v0.2.0 scope per D0024:
- Intel x86_64 host qualification (D0023).
- Physical NVIDIA binding performance evidence.
- Native NixOS VM/package qualification.

## Handoff

M0100 v0.1.0 acceptance is complete. The generic release entry point is
`tools/build-generic-release.sh`. The complete release matrix evidence is at
`/tmp/metaflux-release-matrix-evidence-4/` and `-5/`. The provenance evidence
is at `/tmp/metaflux-provenance-evidence/provenance.json`. All v0.2.0 work is
tracked in the milestone plan.
