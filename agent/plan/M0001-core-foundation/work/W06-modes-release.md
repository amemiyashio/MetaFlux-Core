---
id: M0001-W06
milestone: M0001
status: Active
area: modes-release
depends_on: [M0001-W01, M0001-W03, M0001-W04, M0001-W05]
updated: 2026-08-29
---

# Modes, Performance, and Release

## Outcome

Qualify managed, passthrough, auto, fail-open, coexistence, performance, and
generic/native packaging without weakening the provider closure.

## Vendor Library Discovery (D0013)

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
[`tests/performance/run_m0001_optimization.py`](../../../../tests/performance/run_m0001_optimization.py).
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

The harness and its self-test are implemented, but the PGO, optimization, and
release checklist rows remain open until a clean Git revision/tree run is
archived and the selected profile identity is recorded by the performance and
packaging workflows that consume it. A project PGO profile does not enter the
compiler epoch or `toolchains/`.

## Work

- [x] Discover vendor CUDA/NVML libraries through validated absolute paths that
  cannot recurse to MetaFlux, and load them with local symbol scope.
- [x] Implement deterministic managed/passthrough/auto selection and failure
  transfer without modifying vendor files or real `/dev/nvidia*` nodes.
- [ ] Test real-only, managed-only, and coexistence namespaces.
- [ ] Train provider/compiler-service PGO profiles from representative workloads
  and record hashes in owning performance evidence and packaging inputs.
- [ ] Compare provider `-O2`/`-O3`, monitor instruction-cache growth, apply project
  ThinLTO, and defer custom PGO LLVM until the corpus is stable.
- [ ] Audit allocations, syscalls, locks, cache lines, NUMA, generated assembly,
  relocations, and private dirty RSS.
- [ ] Produce relocatable generic and native NixOS packages; run ABI, sanitizer,
  fuzz, soak, performance, and release-closure gates.

## Exit Gate

Recursion is impossible, injected managed failures leave no partial state and
transfer to the vendor stack, passthrough loss is at most 1%, and all global
acceptance criteria in [M0001](../plan.md) pass on reference Intel and AMD hosts.
