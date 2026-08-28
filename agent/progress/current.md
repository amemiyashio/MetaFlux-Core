---
status: Active
updated: 2026-08-27
milestone: M0001
workstream: M0001-W01
checkpoint: P20260827-001
---

# Current Progress

Active milestone: [M0001](../plan/M0001-core-foundation/plan.md). Active
workstream:
[M0001-W01](../plan/M0001-core-foundation/work/W01-build-toolchain.md). Latest checkpoint:
[P20260827-001](checkpoints/2026/P20260827-001-engineering-bootstrap-baseline.md).

## Current state

The repository has a reproducible, buildable engineering scaffold for the
M0001 boundaries. Targets remain fixtures: CUDA/NVML compatibility, CPU SIMT
execution, compiler service behavior, device registry, and shared transports are
not yet functionally implemented.

Git is initialized on `main`, but there is no first commit and every repository
file is untracked. The checkpoint is therefore evidence and a resume aid, not a
reconstructable Git revision. Until the first commit, use the
[`path:.` Nix practice](../experience/E0001-nix-untracked-flake.md).

## Established bootstrap boundaries

- CMake/Ninja component selection and pinned Clang/LLVM/MLIR/LLD epoch 1.
- Nix packages and checks with fileset-scoped monorepo sources.
- Narrow client-protocol and backend-plugin contract targets.
- Ecosystem provider, compiler frontend, and execution backend build roles.
- CUDA/NVML hidden symbol surfaces, independent ABI gates, and co-load test.
- Runtime and compiler-toolchain install-consumer qualification.
- Stable M0001-M0004 plans, current memory/progress, validated experience,
  templates, and a date-partitioned MetaFlux project work record.
- A stdlib-only Agent record/link/scope checker with credential-leak detection
  and an independent Nix fileset that does not enter runtime, provider, daemon,
  or toolchain packages.
- Canonical ownership documentation linked from
  [component map](../memory/component-map.md).

## Verified baseline

- Full dev: 14/14 tests passed.
- Full ASan: 14/14 tests passed.
- Provider preset: 9/9 tests passed.
- CUDA-only and NVML-only: 5/5 tests passed each.
- Project records, references, output hashes, and project-scope event types
  passed validation.
- `nix flake check path:. -L`, including `agent-records`, passed.
- `runtime`, `provider`, `daemon`, and `toolchain` packages built.

## Next boundary

1. Create the first intentional Git commit after reviewing the entire untracked
   scaffold.
2. Close the unresolved decisions in
   [M0001](../plan/M0001-core-foundation/plan.md).
3. Close the remaining M0001-W01 exit conditions: the release provider sysroot
   and glibc floor, CUDA/NVML header acquisition, the LLVM 22 patchset, and
   reference-host performance qualification.
4. Then activate M0001-W02 according to its dependencies; do not treat present
   bootstrap functions as frozen production ABI.
5. Refresh this file and create a new checkpoint after the first commit or any
   material acceptance-boundary change.
