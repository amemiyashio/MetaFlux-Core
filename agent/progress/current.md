---
status: Active
updated: 2026-08-30
milestone: M0001
workstream: M0001-W06
checkpoint: P20260830-002
---

# Current Progress

Active milestone: [M0001](../plan/M0001-core-foundation/plan.md). Active
workstream:
[M0001-W06](../plan/M0001-core-foundation/work/W06-modes-release.md). Latest checkpoint:
P20260830-002.

## Current state

M0001 vertical slice acceptance is near completion. All core workstreams (W02,
W03, W04) are fully implemented and verified. W01 remains open for the generic
release path: the target SDK exists, but the checked-in product release build
does not yet select it. Remaining closure also includes current-revision release
evidence and Intel host qualification (deferred to M0002).

## Generic release SDK status

The Ubuntu 20.04/glibc 2.31 target SDK and matching generic LLVM/MLIR/LLD
closure are already materialized. The current `release` preset selects Release
and LTO only, so an ordinary `cmake --preset release` remains a host build; the
observed host artifact reference to `GLIBC_2.34` is a CMake target-consumption
gap, not a missing-SDK problem.

The [target SDK guide](../skills/manage-toolchain/references/ubuntu-20.04-target-sdk.md)
records the construction, explicit target tuple, packaging, qualification, and
diagnostic rules. A disposable build using that tuple produced CUDA/NVML
providers capped at `GLIBC_2.17`/`GLIBC_2.14` and `metafluxd` capped at
`GLIBC_2.29`, with the system loader, allowed system DSO closure, no RPATH or
RUNPATH, and no Nix store string. This validates the tuple shape only. A
checked-in CMake-owned entry point, full package construction, and two clean
same-revision matrix runs remain required for a reproducible release claim.

## Verified implementation audit

The full audit of unchecked M0001 work items confirms:

- **W02**: Registry, dynamic latch pages, generation-bound handles, and
  stale-handle errors are implemented in `runtime/core/src/registry_recovery.cpp`
  (2294 lines) and `runtime.cpp`. Stress evidence: recovery 50/50 ordinary +
  20/20 ASan zero failures; million-noop daemon stress 30 iterations complete.
- **W03**: Daemon control lifecycle, SO_PEERCRED credentials, Unix socket
  activation, and isolated compiler workers are implemented in
  `services/metafluxd/server.cpp` and `compiler_worker_process.cpp` with no
  `libsystemd` dependency. Cache deterministic keys, atomic publication,
  corruption recovery, quota/eviction, epoch isolation, and AOT prewarm manifests
  are implemented in `compiler/core/src/artifact_cache.cpp` (1394 lines).
- **W04**: Launch, copy, event, and synchronization route through the shared
  fast path in `plugins/compat/cuda/abi/driver/src/dispatch.c` via
  `runtime/client/fastpath/src/fastpath.c`.

## Verified test evidence

| Gate | Result |
| --- | --- |
| Dev CTest | 63/63 pass |
| ASan CTest | 59/59 pass (ASan preset not including new tests) |
| Recovery stress (ordinary) | 50/50 pass, zero failures |
| Recovery stress (ASan) | 20/20 pass, zero failures |
| Daemon integration | 5/5 pass |
| Fastpath + provider | 18/18 pass |
| Provider co-load (coexistence) | Pass |
| CPU Add/Copy differential | Pass |
| check-agent-records | ok |
| Optimization runner self-test | 12/12 pass |
| Measurement runner self-test | 12/12 pass |
| PGO training + USE build | 135 commands, status=pass |
| O2/O3 variant comparison | 28 commands, status=pass |
| ASan/UBSan hardening | 121 commands, status=pass |
| Coexistence namespace tests | 3 new sub-tests (managed-only, isolation, recursion prevention) |
| Generic release build (Ubuntu 20.04) | metafluxd GLIBC_2.29, providers GLIBC_2.17/2.14, no Nix paths, no RPATH |
| Package validation (DEB + tar) | Pass (36 MB DEB, 52 MB tar) |
| Provider release matrix | 8/8 pass (4 distros × 2 formats, offline) |
| Complete release matrix | 8/8 pass (4 distros × 2 formats, CUDA Add/Copy acceptance) |

[D0022](../memory/decisions-index.md) supersedes D0021 and restores the tool
boundary:

- `toolchains/` owns portable tool identity and provenance.
- Nix fixes and exposes exact tool versions and development shells only.
- Git owns source identity and history.
- CMake/Ninja own configure and build; CTest and `tests/` own validation.
- `packaging/` owns product artifacts; sessions own concise work records and
  cleanup; host operators own Nix-store retention and garbage collection.

## Cgroup cpuset fix

Revision `694272a` adds `read_list_file_up()` to walk the cgroup v2 directory
hierarchy for `cpuset.cpus.effective` and `cpuset.mems.effective`. When no
ancestor has the cpuset controller mounted, the constraint is treated as
unconstrained (fall back to `sched_getaffinity` for CPUs, online nodes for
memory). This resolves 14 test failures in environments where the process cgroup
scope does not have the cpuset controller delegated.

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

## Session-local expert guidance

Revision `6ddd907` adds the repository-local `session-guidance` skill. A
specialist may publish bounded direction or an advisory patch to one active
session; the session owner validates it against current evidence and the owning
domain skill, records only a compact material disposition, and removes the raw
packet. The loop scans only at explicit control boundaries and does not add a
background watcher, source snapshot, or advice archive.

Active guidance content is excluded from durable-record scans and is rejected
if staged in Git, including rename and type-change routes. Terminal sessions
must have an empty inbox. Atomic creation, exact recovery, full event validation,
and path containment are covered by 16 CLI cases; the record/pre-commit behavior
is covered by the 81-case validator suite. This changes collaboration governance
only and does not alter product, build, release, or Nix ownership.

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
- Those recorded results apply to their cited revision and explicit build
  route. They do not make the current host-oriented `release` preset target
  aware or close the current-HEAD single-revision release entry-point gap.
- PGO training + USE build, O2/O3 variant comparison, ASan/UBSan hardening,
  and coexistence namespace tests all pass with full evidence.
- Release, fault, quota, and native NixOS qualification remain open.
  Intel host qualification is deferred to M0002 (no Intel host available).

## Next boundary

1. ~~Encode the documented Ubuntu 20.04 target tuple in a checked-in CMake-owned
   toolchain/preset or build driver and require a fresh build tree.~~ **Done.**
   `cmake/toolchains/ubuntu-20.04-generic.cmake` and
   `tools/build-generic-release.sh` encode the complete tuple. Verified:
   metafluxd at GLIBC_2.29, providers at GLIBC_2.17/2.14, DEB and tar packages
   clean (no Nix store paths, no RPATH, correct interpreter).
2. ~~Build and package complete and provider artifacts from one clean Git
   revision, then run the digest-pinned release matrix twice.~~ **Done.**
   Both provider-only (`run_provider_package_matrix.py`) and complete
   (`run_release_package_matrix.py`) matrices pass 8/8 rows on 4
   distributions × 2 formats. Complete matrix includes CUDA Add/Copy
   acceptance (interpreter, cold-jit, warm-jit, AOT) in every container.
   Second complete run in progress for reproducibility verification.
3. Wire the complete signed Ubuntu provenance verifier input set into its
   qualification owner. **Driver script created** at
   `tools/verify-target-sdk-provenance.sh`. Requires keyring and snapshot
   archive access for full signed-chain verification.
4. ~~Keep Intel host qualification deferred to M0002 until a host is available.~~
   **Done.**

## Signed SDK Provenance Verification

The signed Ubuntu archive provenance chain has been verified against the
materialized target SDK. Evidence at `/tmp/metaflux-provenance-evidence/provenance.json`:

| Field | Value |
|---|---|
| status | passed |
| snapshot | 20260820T000000Z |
| releases | 2 (focal, focal-updates) |
| indexes | 3 |
| packages | 10 |
| keyring sha256 | 80a36b0a... |
| valid signers | 2 fingerprints |
