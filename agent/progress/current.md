---
status: Active
updated: 2026-08-29
milestone: M0001
workstream: M0001-W01
checkpoint: P20260829-005
---

# Current Progress

Active milestone: [M0001](../plan/M0001-core-foundation/plan.md). Active
workstream:
[M0001-W01](../plan/M0001-core-foundation/work/W01-build-toolchain.md). Latest checkpoint:
[P20260829-005](checkpoints/2026/P20260829-005-stage-breakthrough-commit-policy.md).

## Current state

Session
[S20260828-013](../sessions/2026/08/S20260828-013-m0001-foundation/summary.md)
is implementing the M0001 registry/fast path, PTX and Kernel IR path, CPU
backend, compiler worker, and CUDA/NVML compatibility surfaces. Those paths are
still in progress; implementation presence does not close their workstream exit
gates or release qualification.

That active session is now a 13-event curated ledger. It records no end time
until closure, resumes from current main rather than its implementation evidence
revision, and directs each continuation to reload the expert skill matching the
selected work item.

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
  history. Active sessions use `ended_at: null` and may be distilled before
  closure; terminal sessions record an end time and remain immutable.
- `record-session` now has explicit checkpoint and close modes. A coherent,
  independently valuable stage with passing focused gates is committed before
  work enters the next risk or scope phase. The agent decides that semantic
  boundary; the pre-commit hook validates an attempted commit but never creates
  one automatically. Content and compact session/checkpoint records remain
  separate commits.
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
- The current integration tree passes 65/65 tests, including component
  boundaries, registry recovery, PTX parsing, interpreter and compiled-corpus
  differentials, provider ABI checks, daemon integration, release assertion
  regressions, and the million-noop daemon stress test.
- The canonical package builder now enforces D0009 for complete artifacts and
  applies matching ELF/glibc checks to release fixtures. Complete and provider
  package matrices each pass all eight digest-pinned rows; two independent
  builds also produce identical DEB, RPM, and tar bytes. Commit `b05901a`
  preserves this verified boundary; commit `267edc4` preserves the checkpoint
  trigger used for subsequent phases.
- Release, fault, quota, Intel/NUMA, stock-tool, native NixOS, and optimized
  lowering qualification remain open. The current result is not a full M0001
  release certification.

## Next boundary

1. Continue the remaining S20260828-013 M0001 vertical-slice acceptance gates,
   using the smallest matching domain skill plus `manage-toolchain` only when
   tool identity or shell materialization changes.
2. Qualify the remaining stock-tool, optimized-lowering, fault/quota/soak,
   Intel/AMD host, and native NixOS rows without changing the glibc floor.
3. Use `packaging/build.py` and `tests/release/` for any new generic artifact
   claim; keep their temporary outputs outside Git and remove them after the
   compact result is recorded.
