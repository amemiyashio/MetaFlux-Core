# CPU Memory Architecture

## Source entry and change boundary

Start capability/codegen work at `host_compile_options` in
[`compiler.cpp`](../../../../plugins/backend/cpu/compiler/src/compiler.cpp), and
placement work at `ensure_placement` in
[`executor.cpp`](../../../../plugins/backend/cpu/runtime/src/executor.cpp) and the
placement implementation beside it. Reuse the current profile; investigate the
specific incompatible feature, effective mask or NUMA-policy transition before
expanding a host matrix. Existing qualified AMD evidence does not qualify Intel;
Intel expansion remains in milestone-2.0.0.0 under decision-0040.

## Engineering model

For MetaFlux, classify a target by observable properties instead of giving it a
single textbook label:

- one process-visible virtual address space and its page-table/TLB behavior;
- separate or shared instruction/data caches at each level;
- cache coherence and cache-line ownership among logical processors;
- memory types, ordering rules, fences, and atomic guarantees;
- sockets, NUMA nodes, cores, SMT siblings, and shared cache domains;
- MMIO and DMA visibility rules at kernel/transport boundaries.

Classic Von Neumann means instructions and data share a memory organization;
classic Harvard means separate instruction and data memories/buses. Modern x86
systems typically expose a unified coherent address space while using split L1
instruction/data caches and deeper shared caches. This mixed organization is why
the labels alone do not decide code placement, synchronization, or performance.
Arm similarly cautions that a broad "modified Harvard" label is less useful than
the actual cache-maintenance and coherence contract; see [Caches and
self-modifying code](https://developer.arm.com/community/arm-community-blogs/b/architectures-and-processors-blog/posts/caches-and-self-modifying-code).

## x86 capability checklist

- Derive vendor/family/model/stepping, architectural leaves, cache/topology
  leaves, invariant timing support, and SIMD features from CPUID.
- Gate AVX-family use on required OSXSAVE/XCR0 state. Canonicalize features so
  compilation and load-time compatibility use the same ordering and meaning.
- Record microcode, kernel, firmware-relevant mitigations, frequency policy, and
  SMT state for benchmark evidence; do not put unstable runtime frequency into
  object compatibility identity.
- Use Intel and AMD manuals for instruction, ordering, cache, and topology
  details, then verify generated instructions on real qualified hosts.

## Memory placement and ordering

First-touch pages on the intended NUMA node, pin workers deliberately, and keep
hot per-stream cursors on separate cache lines. Distinguish compiler ordering,
CPU memory ordering, kernel DMA ordering, and MMIO ordering; one fence does not
stand in for all four. Inspect cache-line ownership/false sharing, prefetch,
TLB/page size, first touch and shared-ring ownership before arithmetic tuning.
Select worker count, work stealing and oversubscription within the effective
placement profile; scheduling defaults do not become a helper or object ABI.

## Effective execution placement

- Read `sched_getaffinity(0, ...)` and `/sys/devices/system/cpu/online`; do not
  schedule from firmware or CPUID topology alone.
- Read cgroup v2 `cpuset.cpus.effective` and `cpuset.mems.effective`, or the
  cgroup v1 `cpuset.effective_cpus` and `cpuset.effective_mems`. Account for parent
  cgroups, systemd/service policy, and container restrictions; Linux may silently
  intersect an affinity request with these constraints.
- Record `/sys/devices/system/node` topology and distances, permitted memory
  nodes, the process policy reported by `get_mempolicy`, and first-touch behavior
  before selecting worker and allocation placement.
- Refresh or invalidate placement after CPU hotplug, affinity, cpuset, or NUMA
  policy changes. If a requested pin becomes invalid, use the declared fallback
  or return a stable error rather than silently scheduling outside the effective
  set.

Keep this dynamic placement profile out of stable object compatibility identity
unless it changes generated code or helper ABI. Record its exact masks and policy
generation in runtime diagnostics and benchmark metadata.

The current executor checks whether the placement snapshot is still current on
launch and rebuilds workers at a launch boundary when needed. Preserve that
refresh path while removing avoidable discovery/allocation work. The checklist
above is the effective-placement contract, not a claim that every cgroup variant
has an implemented reader. Test the changed reader or refresh transition with
restricted masks, offline/hotplug CPUs, invalid explicit pins and NUMA policy;
include shutdown/concurrent launch coverage when worker ownership changes.
`metaflux.unit.cpu-placement` owns focused placement checks. Its physical AMD
integration row may report exit 77 when unavailable; a skip is no host evidence.

Primary sources:

- [Intel Software Developer Manuals](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html)
- [AMD64 Architecture Programmer's Manual](https://docs.amd.com/v/u/en-US/40332_4.09_APM_PUB)
