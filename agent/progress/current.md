---
status: Active
updated: 2026-08-29
milestone: M0001
workstream: M0001-W01
checkpoint: P20260829-002
---

# Current Progress

Active milestone: [M0001](../plan/M0001-core-foundation/plan.md). Active
workstream:
[M0001-W01](../plan/M0001-core-foundation/work/W01-build-toolchain.md). Latest checkpoint:
[P20260829-002](checkpoints/2026/P20260829-002-stale-route-cleanup.md).

## Current state

Session
[S20260828-013](../sessions/2026/08/S20260828-013-m0001-foundation/summary.md)
is implementing the M0001 registry/fast path, PTX and Kernel IR path, CPU
backend, compiler worker, and CUDA/NVML compatibility surfaces. Those paths are
still in progress; implementation presence does not close their workstream exit
gates or release qualification.

[D0022](../memory/decisions-index.md) supersedes D0021 and restores the tool
boundary:

- `toolchains/` owns portable tool identity and provenance.
- Nix fixes and exposes exact tool versions and development shells only.
- Git owns source identity and history.
- CMake/Ninja own configure and build; CTest and `tests/` own validation.
- `packaging/` owns product artifacts; sessions own concise work records and
  cleanup; host operators own Nix-store retention and garbage collection.

M0001 defines tasks, dependencies, and acceptance conditions. Its W01 document
points to the canonical [toolchain policy](../../toolchains/README.md) rather
than duplicating repository-wide workflow semantics.

## Verified correction

- The flake exposes tool packages, four development shells, and a formatter; it
  exposes no project checks, product packages, or NixOS module outputs.
- The default shell provides Clang/LLVM `22.1.8`, CMake `4.1.6`, and Ninja
  `1.13.2`. It suppresses Nix's temporary self-RPATH and provides fixed
  tool-runtime libraries through the shell environment, so project ELF files
  do not embed `outputs/out/lib`.
- CMake preset outputs live under the repository-external
  `.metaflux-build/MetaFlux-Core/` hierarchy. The full dev preset builds and
  passes 58/58 CTest cases there.
- Session governance now treats records as curated work ledgers. Failed-route
  artifacts, temporary downloads, generated trees, duplicate source copies,
  and routine logs are removed at the session boundary; Git remains the source
  history.
- [P20260829-001](checkpoints/2026/P20260829-001-toolchain-boundary-correction.md)
  records the first duplicate-source cleanup. Follow-up
  [S20260829-002](../sessions/2026/08/S20260829-002-cleanup-stale-routes/summary.md)
  removed the remaining 2.56 GiB of exact temporary/repository residue and a
  1,294-path dead Nix closure; Nix reported another 26.1 GiB freed without a
  broad garbage collection. Final scans found no old product/source route or
  project GC root, while 104 legitimate tool materializations remain.

## Active M0001 evidence

- D0016 fixes the supported NVIDIA header inputs; D0017 fixes the PTX 9.0,
  `sm_70` semantic oracle; D0018 fixes LLVM `22.1.8` plus the downstream patch;
  D0019 selects the static compiler component closure; D0020 makes mirror
  selection relative to the configured timezone.
- The in-progress dev tree currently passes 58/58 tests, including component
  boundaries, registry recovery, PTX parsing, interpreter and compiled-corpus
  differentials, provider ABI checks, daemon integration, and performance
  runner self-tests.
- Release, fault, quota, Intel/NUMA, stock-tool, generic-package, and optimized
  lowering qualification remain open. The current test result is not a release
  certification.

## Next boundary

1. Continue S20260828-013 through the M0001 vertical-slice acceptance gates,
   using the smallest matching domain skill plus `manage-toolchain` only when
   tool identity or shell materialization changes.
2. Complete MLIR/LLVM PIC-ELF lowering and prove cold JIT, warm cache load, and
   administrator AOT against the D0017 oracle.
3. Qualify the provider/header matrix with stock tools and build generic
   artifacts through `packaging/` and `tests/release/`.
4. Remove session-owned external build and evidence trees when their compact
   verification result has been recorded; preserve concurrent or user-owned
   work.
