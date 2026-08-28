---
name: cpu-backend-performance
description: Design or review x86 CPU backend capability detection, topology, SIMT-to-loop or SIMD mapping, cache and NUMA behavior, memory ordering, LLVM vectorization, and reproducible performance evidence. Use for M0001 CPU execution and tuning. Do not use to redefine PTX semantics, own MLIR framework mechanics, or design processor circuits.
---

# CPU Backend Performance

## Inputs

- The active milestone/work item, exact x86_64 target hosts, topology, kernel,
  microcode, compiler epoch, CPU name, and canonical feature set.
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
- Route PTX meaning to `$ptx-simt-semantics` and MLIR dialect/pass implementation
  to `$mlir-compiler-engineering`.

## Workflow

1. Freeze the target matrix and derive a canonical capability profile from CPUID
   plus OS-enabled state, not CPUID feature bits alone.
2. Separate semantic choices from scheduling choices. Preserve lane predicates,
   divergence, barriers, atomics, FP behavior, and memory visibility before
   selecting loop, thread, or SIMD structure.
3. Choose CTA placement, worker count, affinity, NUMA policy, work stealing, and
   oversubscription rules explicitly. Mark open M0001 topology decisions rather
   than embedding provisional defaults as ABI.
4. Shape loops and memory accesses for analyzable aliasing, alignment, stride,
   and trip counts. Inspect vectorizer legality/cost remarks and generated code.
5. Account for cache lines, false sharing, prefetch behavior, TLB/page size,
   first-touch placement, and shared-ring ownership before micro-optimizing
   arithmetic.
6. Build deterministic target/cache identity from CPU and compiler inputs, then
   verify incompatible artifacts miss rather than execute.
7. Benchmark against the same-path direct baseline with pinned topology and raw
   distributions. Attribute time to dispatch, scheduling, generated kernel,
   copies, and synchronization separately.

## Output

Return or implement:

- A canonical CPU capability/topology profile and compatibility predicate.
- A SIMT-to-loop/SIMD execution plan with semantic guards and fallback forms.
- Vectorization evidence including remarks, IR/disassembly observations, and any
  minimized LLVM epoch reproducer.
- A reproducible benchmark record with raw samples, distributions, environment,
  baseline definition, and interpretation.

## Verification

- Differentially compare scalar reference, interpreter, JIT, and AOT over the
  entire advertised corpus and edge cases before accepting performance changes.
- Test Intel and AMD x86_64 targets, feature mismatch, affinity/NUMA variants,
  tails, divergent masks, barriers, atomics, and deterministic shutdown.
- Confirm cache keys change for target triple, CPU/features, compiler epoch,
  pass pipeline, FP policy, helper ABI, PGO ID, or kernel content changes.
- Inspect vectorization remarks and machine code; do not infer SIMD from source
  spelling or one favorable timing sample.
- Apply the active milestone's provisional/binding budget status and archive the
  harness version and raw data used for any gate claim.
