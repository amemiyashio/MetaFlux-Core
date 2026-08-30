# Session Summary

## Objective and outcome

M0100 implementation remains active. Revision `7b86b35` is the implementation
provenance checkpoint for a working registry and fast path, PTX/Kernel IR and
CPU execution path, compiler worker, CUDA/NVML compatibility surfaces,
performance runners, packaging metadata, and release-harness foundation. Its
development suite passed, but remaining release and reference-host gates prevent
a completion claim.

## Durable changes

- `contracts/`, `runtime/`, and `services/metafluxd/`: protocol, shared layout,
  registry recovery, fast path, compiler worker, and daemon execution modes.
- `compiler/` and `plugins/backend/cpu/`: Kernel IR, caches, interpreter,
  compiled kernels, placement, MLIR/LLVM pipeline, and differential tests.
- `plugins/compat/cuda/`: CUDA Driver and NVML ABI surfaces, PTX frontend, and
  validated passthrough discovery.
- `tests/`, `packaging/`, and `toolchains/`: compatibility, performance, and
  release harnesses, package integration, and fixed external inputs.

## Verification

| Command/gate | Result |
| --- | --- |
| External dev CMake configure/build at `7b86b35` | Passed |
| Dev CTest preset | 58/58 passed |
| Optimization and measurement runner self-tests | 12/12 each passed |
| Registry recovery stress | 300 ordinary + 100 sanitizer cycles; independent 20 + 10 repeat passed |
| NVML policy setters | 7/7 ordinary + 5/5 sanitizer tests passed |
| Full release matrix | Previous pass invalidated; qualification remains open |

## Cleanup

- Removed after verification: repository-local and session-owned external build
  trees.
- Removed from the active ledger: routine command chatter, local store
  identities, duplicate mirror wording, superseded D0021 routing, and the
  invalid release-pass claim.
- Retained outside Git: none. The session contains no copied source, build tree,
  dependency store, download, raw log, or output attachment.

## Decisions and experience

- D0012-D0020 own the release matrix, vendor-pair loading, cache and placement,
  ABI inputs, PTX semantics, compiler epoch, static closure, and timezone-based
  mirror routing.
- D0022 supersedes D0021 and defines the current tool/workflow/session boundary.
- D0027 supersedes D0023 only for the future Intel destination and supersedes
  D0024 only for the former assignment of Intel and physical NVIDIA gates to
  `v0.2.0`: Intel x86_64 support and physical NVIDIA binding-performance
  qualification belong to M1000 / `v1.0.0`; native NixOS VM/package
  qualification remains `v0.2.0`. M0100 keeps its AMD x86_64 reference evidence
  and provisional budgets (G003, event 16; migration authority: SC0003).
- Event 9 retains the one failed-route lesson needed for the release-matrix
  rerun; no separate experience record was created.

## roast

### light roasts

- Cgroup cpuset fallback for scopes without the cpuset controller ->
  `plugins/backend/cpu/runtime/src/placement.cpp` (revision `694272a`; event 14
  records dev and ASan CTest plus recovery-stress evidence)

### medium roasts

- Cross-subsystem implementation provenance for the registry, PTX/Kernel IR,
  CPU execution, compiler worker, CUDA/NVML surfaces, performance runners,
  packaging, and release harness ->
  `agent/progress/checkpoints/2026/P20260829-001-toolchain-boundary-correction.md`
  (revision `7b86b35`; event 12 records dev configure/build, 58/58 CTest, and
  12/12 runner self-tests)

### dark roasts

- Four-distribution release-matrix policy ->
  `agent/plan/M0100-core-foundation/work/W0101-build-toolchain.md` (event 3;
  authority: D0012, SC not required)
- Same-build absolute-path CUDA/NVML vendor-pair policy ->
  `agent/plan/M0100-core-foundation/work/W0106-modes-release.md` (event 3;
  authority: D0013, SC not required)
- Per-UID compiler-cache isolation and publication policy ->
  `agent/plan/M0100-core-foundation/work/W0103-compiler-cpu.md` (event 3;
  authority: D0014, SC not required)
- Effective-core and NUMA-local placement policy ->
  `agent/plan/M0100-core-foundation/work/W0103-compiler-cpu.md` (event 3;
  authority: D0015, SC not required)
- PTX 9.0/`sm_70` capability and semantic-oracle policy ->
  `agent/plan/M0100-core-foundation/work/W0103-compiler-cpu.md` (event 5;
  authority: D0017, SC not required)
- Frozen CUDA/NVML ABI-input policy -> `toolchains/README.md` (event 4;
  authority: D0016, SC not required)
- LLVM compiler-epoch policy -> `toolchains/README.md` (event 6; authority:
  D0018, SC not required)
- Timezone-relative mirror-routing policy -> `toolchains/README.md` (event 8;
  authority: D0020, SC not required)
- Static MLIR/LLVM component-closure policy ->
  `agent/plan/M0100-core-foundation/plan.md` (event 7; authority: D0019, SC not
  required)
- Git/tool/session ownership boundary -> `toolchains/README.md` (event 13;
  authority: D0022, SC not required; recorded pre-D0025 boundary)

## session-only

- The invalid offline release-pass assertion and its `! grep` failure lesson -
  reason: event 9 preserves the minimum rerun warning without creating a
  separate experience record.

## Unresolved items

- Current M0100 gate and lifecycle state is owned by `agent/progress/current.md`
  and the active completion session; this implementation checkpoint remains
  evidence for its named revision rather than release certification.
- Intel x86_64 support and physical NVIDIA binding-performance qualification
  are queued under M1000 / `v1.0.0`; native NixOS VM/package qualification
  remains the unallocated `v0.2.0` expansion. None reopens M0100.

## Handoff

Continue from current main, not by checking out `7b86b35`. Read current
progress and M0100 for current foundation state; read M1000, D0027, and SC0003
before scheduling Intel x86_64 support or physical NVIDIA binding-performance
qualification. Keep native NixOS qualification under `v0.2.0`. Use the expert
skill matching the selected work and use `manage-toolchain` only for tool
identity or provisioning changes; run owned build, test, packaging, and
evidence workflows in external work directories.
