---
status: Active
updated: 2026-08-28
milestone: M0001
workstream: M0001-W01
checkpoint: P20260828-001
---

# Current Progress

Active milestone: [M0001](../plan/M0001-core-foundation/plan.md). Active
workstream:
[M0001-W01](../plan/M0001-core-foundation/work/W01-build-toolchain.md). Latest checkpoint:
[P20260828-001](checkpoints/2026/P20260828-001-spec-consistency-revisions.md).

## Current state

The repository has a reproducible, buildable engineering scaffold for the
M0001 boundaries. Targets remain fixtures: CUDA/NVML compatibility, CPU SIMT
execution, compiler service behavior, device registry, and shared transports are
not yet functionally implemented.

Git history now exists on `main`: baseline
`9703559ef0056b6dd8ef5432b645a1362e72d734` and the spec-consistency revision
`5360d51a09234f9753f260f218dc5a87e52c7eef`. The `path:.` Nix practice from
[E0001](../experience/E0001-nix-untracked-flake.md) is no longer required.

A 2026-08-28 self-review removed five specification contradictions
([S20260828-001](../sessions/2026/08/S20260828-001-spec-consistency/summary.md)):
D0008 records the vroot synthetic NVIDIA identity as a presentation disguise
with registration/legal review required before release promotion; D0009 closes
the userspace glibc floor at 2.31 (Ubuntu 20.04) with a restricted provider
`DT_NEEDED` universe; the v0.1 memfd transport carries an explicit
at-most-one-wake-syscall budget with the zero-syscall obligation deferred to
the M0002 doorbell transports; numeric performance budgets are provisional
until the named harness archives a baseline; and the authoritative
logical-device view is scoped per managed domain.

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
  passed validation; re-validated after the 2026-08-28 spec amendments.
- `nix flake check path:. -L`, including `agent-records`, passed.
- `runtime`, `provider`, `daemon`, and `toolchain` packages built.

## Next boundary

1. Close the remaining unresolved decisions in
   [M0001](../plan/M0001-core-foundation/plan.md); the glibc baseline is now
   closed by D0009, leaving the release distribution matrix, LLVM 22 patchset,
   PTX corpus, CUDA/NVML header acquisition, vendor library discovery, cache
   policy, CPU worker topology, and compiler closure decisions.
2. Close the remaining M0001-W01 exit conditions: the release provider sysroot,
   CUDA/NVML header acquisition, the LLVM 22 patchset, and reference-host
   performance qualification that promotes the provisional budgets to binding.
3. Then activate M0001-W02 according to its dependencies; do not treat present
   bootstrap functions as frozen production ABI.
4. Refresh this file and create a new checkpoint after any material
   acceptance-boundary change.
