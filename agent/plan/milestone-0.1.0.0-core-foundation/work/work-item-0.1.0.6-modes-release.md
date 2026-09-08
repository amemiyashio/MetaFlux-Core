---
id: work-item-0.1.0.6
delivery: 0.1.0.6
milestone: milestone-0.1.0.0
status: Complete
area: modes-release
depends_on: [work-item-0.1.0.1, work-item-0.1.0.3, work-item-0.1.0.4, work-item-0.1.0.5]
updated: 2026-09-08
---

# Modes, Performance, and Release

## Outcome

Qualify managed, passthrough, auto, fail-open, coexistence, provisional
performance, and generic packaging without weakening the provider closure.

## Vendor Library Discovery (decision-0013)

An administrator may provide exactly one CUDA/NVML pair as absolute paths in
`/etc/metaflux/vendor-libraries.conf`. Without that override, discovery is
restricted to `/usr/lib/x86_64-linux-gnu/{libcuda.so.1,libnvidia-ml.so.1}` on
Ubuntu and `/usr/lib64/{libcuda.so.1,libnvidia-ml.so.1}` on Rocky Linux. The
implementation resolves every symlink before `open`/`fstat` and rejects a file
that is not root-owned, is group/world writable, is not ELF64 x86_64 `ET_DYN`,
has the wrong SONAME, lies under the MetaFlux install root, or matches the
provider's inode or build ID.

The resolved CUDA and NVML files must identify the same driver build and agree
with `/proc/driver/nvidia/version`. They are loaded only by canonical absolute
path with `dlmopen(LM_ID_NEWLM, RTLD_NOW | RTLD_LOCAL)`; bootstrap symbols and
versions are validated before the vendor view is published. Discovery never
uses the working directory, `LD_LIBRARY_PATH`, a provider RUNPATH, bare-name
`dlopen`, or a shell invocation of `ldconfig`. Any future direct parser for the
loader cache is still constrained by the same directory whitelist and file
validation.

The result is invalidated after `fork`, mount-namespace change, cache
inode/mtime change, or driver-version change. Qualification covers a shadow
provider, symlink replacement, wrong architecture/SONAME, mismatched pairs,
stale cache, every distribution path, and real CUDA/NVML co-loading. This
policy prevents recursion and untrusted search-path capture while preserving an
explicit administrator override.

## Optimization and Durability Qualification

The clean-Git qualification runner is
[`tests/performance/run_milestone_0_1_0_0_optimization.py`](../../../../tests/performance/run_milestone_0_1_0_0_optimization.py).
It produces evidence only from commands it actually executes:

- PGO training uses explicit provider, compiler-worker, interpreter, cold-JIT,
  warm-JIT, AOT, and managed-performance tests. A fresh `%m`/PID-partitioned raw
  directory is mandatory; `llvm-profdata` validates the merged candidate, and
  the same workloads must pass after the `USE` rebuild. The sidecar binds the Git
  revision and tree, build manifest, corpus, raw set, profile, and tool hashes.
- The provider comparison holds Release+ThinLTO constant, changes only `-O2`
  versus `-O3`, requires exact export parity, records all executable ELF section
  sizes, and alternates repeated runtime regressions on one CPU. No unstated
  code-size or timing threshold chooses a winner.
- The ASan/UBSan hardening tree drives deterministic corpus mutation through the
  production PTX parser and repeatedly exercises recovery, lifecycle, policy,
  compiler-service, and four-mode Add/Copy tests. Missing tests and incomplete
  iterations are failures rather than skipped evidence.

Binding host-Copy evidence uses separate schema-v2 native H2D and D2H raw
sample sets with direction, API, completion boundary, workload size, placement,
and host fingerprints. The binding identity also fixes pageable host allocation,
physical device UUID/model/BDF/PCIe/NUMA path, driver hash, native CUDA library
build ID and hash, and the CMake/Ninja-built benchmark binary, command, sample
count, and stopping rule. The full command vector and CUDA device/provider-loading
environment are exact schema fields, not free-form provenance. An unavailable
physical field leaves the gate incomplete. A D2D baseline is an independent
observation and cannot satisfy either host direction. Daemon direct-path
counters must cover at least one full-size setup or correctness transfer plus
every warm-up and measured sample in each direction, while all staged-host
counters remain zero.

This binding route is implemented as a strict harness but is not a milestone-0.1.0.0 /
`v0.1.0` exit gate. Physical NVIDIA execution, passthrough performance, and
budget promotion belong to milestone-2.0.0.0 / `v2.0.0`; missing physical fields must
continue to fail binding mode rather than being treated as a provisional pass.

The harness and its self-test are implemented. The recorded clean-revision run
archives PGO, optimization, hardening, and release evidence and binds the
selected profile identity through the owning performance and packaging
workflows. A project PGO profile does not enter the compiler epoch or
`toolchains/`.

## Work

- [x] Discover vendor CUDA/NVML libraries through validated absolute paths that
  cannot recurse to MetaFlux, and load them with local symbol scope.
- [x] Implement deterministic managed/passthrough/auto selection and failure
  transfer without modifying vendor files or real `/dev/nvidia*` nodes.
- [x] Test real-only, managed-only, and coexistence namespaces.
- [x] Train provider/compiler-service PGO profiles from representative workloads
  and record hashes in owning performance evidence and packaging inputs.
- [x] Compare provider `-O2`/`-O3`, monitor instruction-cache growth, apply project
  ThinLTO, and defer custom PGO LLVM until the corpus is stable.
- [x] Audit allocations, syscalls, locks, cache lines, NUMA, generated assembly,
  relocations, and private dirty RSS.
- [x] Produce relocatable generic DEB, RPM, and tar packages; run ABI,
  sanitizer, parser-mutation, soak, provisional-performance, and generic
  release-closure gates. Recorded clean-revision evidence includes the PGO USE
  build, O2/O3 comparison, ASan/UBSan hardening, coexistence tests, and both
  eight-row generic package matrices.

Native NixOS VM/package qualification remains unallocated `v0.3.0` work.
Physical NVIDIA binding-performance promotion belongs to milestone-2.0.0.0 / `v2.0.0`.
Neither is an unchecked work-item-0.1.0.6 row.

## Exit Gate

Recursion is impossible, injected managed failures leave no partial state and
transfer to the vendor stack, the four generic distribution rows pass from one
declared Git revision, and all in-scope global acceptance criteria in
[milestone-0.1.0.0](../plan.md) pass on the AMD reference host. Passthrough loss and the
other provisional numeric budgets become binding only with milestone-2.0.0.0 / `v2.0.0`
physical NVIDIA evidence. Intel x86_64 support qualification is also a milestone-2.0.0.0
obligation; native NixOS qualification remains an unallocated `v0.3.0` gate.
