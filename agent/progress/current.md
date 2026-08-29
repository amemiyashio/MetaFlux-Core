---
status: Active
updated: 2026-08-29
milestone: M0001
workstream: M0001-W06
checkpoint: P20260829-007
---

# Current Progress

Active milestone: [M0001](../plan/M0001-core-foundation/plan.md). Active
workstream:
[M0001-W06](../plan/M0001-core-foundation/work/W06-modes-release.md). Latest checkpoint:
[P20260829-007](checkpoints/2026/P20260829-007-pytorch-cuda-client-profiles.md).

## Current state

Session
[S20260828-013](../sessions/2026/08/S20260828-013-m0001-foundation/summary.md)
is implementing the M0001 vertical slice. Revision `694272a` fixes cgroup cpuset
fallback for scopes without cpuset controller and confirms W02/W03/W04 are fully
implemented. The active session is now a 14-event curated ledger.

## Verified implementation audit

The full audit of unchecked M0001 work items confirms:

- **W02**: Registry, dynamic latch pages, generation-bound handles, and
  stale-handle errors are implemented in `runtime/core/src/registry_recovery.cpp`
  (2294 lines) and `runtime.cpp`. Stress evidence: recovery50/50 ordinary +
  20/20 ASan zero failures; million-noop daemon stress30 iterations in progress.
- **W03**: Daemon control lifecycle, SO_PEERCRED credentials, Unix socket
  activation, and isolated compiler workers are implemented in
  `services/metafluxd/server.cpp` and `compiler_worker_process.cpp` with no
  `libsystemd` dependency. Cache deterministic keys, atomic publication,
  corruption recovery, quota/eviction, epoch isolation, and AOT prewarm manifests
  are implemented in `compiler/core/src/artifact_cache.cpp` (1394 lines).
- **W04**: Launch, copy, event, and synchronization route through the shared
  fast path in `plugins/compat/cuda/abi/driver/src/dispatch.c` via
  `runtime/client/fastpath/src/fastpath.c`.

## Cgroup cpuset fix

Revision `694272a` adds `read_list_file_up()` to walk the cgroup v2 directory
hierarchy for `cpuset.cpus.effective` and `cpuset.mems.effective`. When no
ancestor has the cpuset controller mounted, the constraint is treated as
unconstrained (fall back to `sched_getaffinity` for CPUs, online nodes for
memory). This resolves14 test failures in environments where the process cgroup
scope does not have the cpuset controller delegated.

## Verified test evidence

| Gate | Result |
| --- | --- |
| Dev CTest |59/59 pass |
| ASan CTest |59/59 pass |
| Recovery stress (ordinary) |50/50 pass, zero failures |
| Recovery stress (ASan) |20/20 pass, zero failures |
| Daemon integration (cross-process, execution-modes, compiler-worker, process-snapshot, pre-negotiation-admission) |5/5 pass |
| Fastpath + provider (18 tests) |18/18 pass |
| Provider co-load (coexistence) | Pass |
| CPU Add/Copy differential | Pass |
| check-agent-records | ok |
| Optimization runner self-test |12/12 pass |
| Measurement runner self-test |12/12 pass |

[D0022](../memory/decisions-index.md) supersedes D0021 and restores the tool
boundary:

- `toolchains/` owns portable tool identity and provenance.
- Nix fixes and exposes exact tool versions and development shells only.
- Git owns source identity and history.
- CMake/Ninja own configure and build; CTest and `tests/` own validation.
- `packaging/` owns product artifacts; sessions own concise work records and
  cleanup; host operators own Nix-store retention and garbage collection.

## Optional PyTorch CUDA client profiles

Revisions `7fd83f6` and `b4e78bf` add two exact, on-demand test clients and a
five-stage diagnostic probe. `pytorch-baseline` pins Python 3.13.15, PyTorch
`2.11.0+cu126`, CUDA 12.6, and a wheel containing `sm_70` artifacts.
`pytorch-frontier` pins PyTorch `2.13.0+cu132`, CUDA 13.2, and wheel artifacts
starting at `sm_75`, including the intended future `sm_80` target. Each closure
contains 29 hash- and size-locked wheels.

The two packages and shells are explicit opt-ins. Recursive derivation scans
confirm the default, provider, runtime, and release shells do not reference
them. Both closures were fully materialized and imported in disposable local
Nix stores, then those stores were removed without host GC. The offline probe
self-test passes 7/7 and its two focused CTest gates pass 2/2. M0001 and D0017
remain frozen at `sm_70`; neither client is product or release evidence.

M0001 defines tasks, dependencies, and acceptance conditions. Its W01 document
points to the canonical [toolchain policy](../../toolchains/README.md) rather
than duplicating repository-wide workflow semantics.

## Verified correction

- The flake exposes tool packages, four primary development shells, two explicit
  PyTorch client shells, and a formatter; it exposes no project checks, product
  packages, or NixOS module outputs.
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
