---
name: cpu-backend-performance
description: Design or review x86 CPU interpreter/runtime and target-lowering implementation, capability detection, effective CPU and NUMA placement, SIMT-to-loop or SIMD mapping, memory ordering, LLVM vectorization, and reproducible performance evidence. Use for M0100 CPU execution and tuning. Do not use to redefine the PTX semantic oracle, own MLIR conversion mechanics, or design processor circuits.
---

# CPU Backend Performance

## Inputs

- The active milestone/work item, exact x86_64 target hosts, hardware topology,
  kernel, microcode, compiler epoch, CPU name, and canonical feature set.
- The current `sched_getaffinity` mask, online CPU set, cpuset v1/v2 effective CPU
  and memory-node masks, NUMA topology/mempolicy, service or container limits, and
  any CPU-hotplug or affinity generation relevant to execution.
- Kernel IR semantic requirements, generated loop/vector IR, optimization
  remarks, disassembly, profiles, and the direct/reference benchmark harness.
- Worker scheduling, affinity, NUMA, helper ABI, cache key, and FP policy relevant
  to the change.

Correctness precedes tuning. Do not use host-native features unless the cache key
and runtime dispatch prove the loaded object is compatible with that host.

## Routing

- Use [CPU memory architecture](references/cpu-memory-architecture.md) for x86
  capability, hierarchy, coherence, NUMA, ordering, MMIO, DMA, and the practical
  Von Neumann/Harvard distinction.
- Use [SIMT to SIMD](references/simt-to-simd.md) for CTA scheduling, lane masks,
  barriers, and vector-width strategy.
- Use [LLVM vectorization](references/llvm-vectorization.md) for loop shape,
  legality, cost-model evidence, remarks, and epoch reproducers.
- Use [benchmarking](references/benchmarking.md) for controlled comparisons and
  archived evidence.
- Route PTX/Kernel IR meaning and semantic-oracle expectations to
  `$ptx-simt-semantics`. Keep CPU interpreter implementation, CPU target policy
  and lowering, and runtime execution here; compose MLIR dialect, conversion, and
  pass mechanics with `$mlir-compiler-engineering`.

## Workflow

1. Freeze the target matrix and derive a stable compatibility profile from CPUID,
   required XCR0/OS-enabled state, microarchitecture inputs, and compiler/codegen
   policy, not CPUID feature bits alone.
2. Build a separate dynamic placement profile from `sched_getaffinity`, online
   CPUs, effective cpuset CPU/memory-node masks, NUMA topology and mempolicy, and
   service/container restrictions. Define refresh, fallback, or error behavior for
   hotplug and affinity/cgroup changes.
3. Separate semantic choices from scheduling choices. Preserve lane predicates,
   divergence, barriers, atomics, FP behavior, and memory visibility before
   selecting loop, thread, or SIMD structure.
4. Choose CTA placement, worker count, affinity, NUMA policy, work stealing, and
   oversubscription only within the effective placement profile. Mark open M0100
   topology decisions rather than embedding provisional defaults as ABI.
5. Shape loops and memory accesses for analyzable aliasing, alignment, stride,
   and trip counts. Inspect vectorizer legality/cost remarks and generated code.
6. Account for cache lines, false sharing, prefetch behavior, TLB/page size,
   first-touch placement, and shared-ring ownership before micro-optimizing
   arithmetic.
7. Build deterministic target/cache identity from stable hardware, compiler, and
   codegen inputs, then verify incompatible artifacts miss rather than execute.
   Keep transient affinity, cpuset, and NUMA placement out of object compatibility
   identity unless they change code shape or ABI; always record them as run data.
8. Benchmark against the same-path direct baseline with pinned topology and raw
   distributions. Attribute time to dispatch, scheduling, generated kernel,
   copies, and synchronization separately.

## Output

Return or implement:

- A stable CPU capability profile and object-compatibility predicate, plus a
  separate dynamic effective-placement profile and refresh/fallback policy.
- A CPU interpreter/runtime or target-lowering implementation plan, when in
  scope, whose expected semantics come from the PTX/Kernel IR oracle.
- A SIMT-to-loop/SIMD execution plan with semantic guards and fallback forms.
- Vectorization evidence including remarks, IR/disassembly observations, and any
  minimized LLVM epoch reproducer.
- A reproducible benchmark record with raw samples, distributions, environment,
  baseline definition, and interpretation.

## Verification

- Differentially compare the semantic reference, CPU-owned interpreter, JIT, and
  AOT over the entire advertised corpus and edge cases before accepting
  performance changes.
- Test Intel and AMD x86_64 targets, feature mismatch, restricted affinity and
  cpuset CPU/memory-node masks, invalid requested pins, offline/hotplug changes,
  NUMA placement variants, tails, divergent masks, barriers, atomics, and
  deterministic shutdown.
- Confirm cache keys change for target triple, CPU/features, compiler epoch,
  pass pipeline, FP policy, helper ABI, PGO ID, or kernel content changes.
- Inspect vectorization remarks and machine code; do not infer SIMD from source
  spelling or one favorable timing sample.
- Apply the active milestone's provisional/binding budget status and archive the
  harness version and raw data used for any gate claim.
