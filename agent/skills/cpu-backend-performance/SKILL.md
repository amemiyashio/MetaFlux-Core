---
name: cpu-backend-performance
description: Implement or review canonical CPU kernel execution, interpreter/JIT/AOT behavior, x86 lowering, effective CPU/NUMA placement and avoidable warm-path costs. Use for the stock PyTorch CUDA CPU profile or CPU tuning; PTX meaning and MLIR conversion mechanics have separate owners.
---

# CPU Backend Performance

For an implementation request, deliver CPU behavior through its actual executor.
For analysis, review or benchmarking, stay within that requested mode and report
evidence; do not infer an implementation request.
Start with `PreparedModule::launch` in
[`services/metafluxd/src/execution.cpp`](../../../services/metafluxd/src/execution.cpp):
it selects the generic interpreter or loaded compiled entry. Read the active
work item's accepted corpus and remaining boundary before extending a family.

For an implementation request, identify the first missing semantic or execution
step and change its owner. For tuning, identify a warm operation to eliminate,
move invariant work to preparation, then measure the same request path. A
compilation counter or faster suite is not an execution/performance result.

| Task | Read only the relevant guide |
| --- | --- |
| Stock client operator/shape, four CPU modes, helper or launch lifetime | [Execution path](references/execution-path.md) |
| CTA scheduling, masks, barriers, register storage or SIMD eligibility | [SIMT mapping](references/simt-to-simd.md) |
| Generated loop/vector code, exact FP or missed vectorization | [LLVM vectorization](references/llvm-vectorization.md) |
| Capability/cache compatibility, affinity, cpuset, NUMA, MMIO/DMA | [CPU memory architecture](references/cpu-memory-architecture.md) |
| Dispatch/kernel/copy cost, benchmark design or budget claim | [Benchmarking](references/benchmarking.md) |

Reuse the current target and placement profiles. Rebuild their evidence only
when the change affects compatibility, placement or the advertised host scope.
Stable CPUID/OS-enabled code compatibility and dynamic effective placement are
different inputs; transient masks enter object identity only if they change
code shape or ABI. Preserve refresh and explicit-pin error behavior.

Preserve predicates, divergence, full-CTA barriers, address provenance, bounds,
permissions, alignment, overflow and exact FP semantics. Retain scalar fallback
where its admitted inputs still need it. Source spelling alone proves no SIMD.
No host-native instruction may execute without compatible dispatch/cache identity.

Keep parsing, compilation, symbol resolution, hashing, allocation and reusable
launch setup outside steady execution whenever their inputs permit preparation.
Validate mutable launch values and generations at use; reuse scratch only under
an ownership/concurrency contract. A constant allocation count is not zero cost.

Compose [$ptx-simt-semantics](../ptx-simt-semantics/SKILL.md) skill for source meaning
and independent oracles; compose [$mlir-compiler-engineering](../mlir-compiler-engineering/SKILL.md) skill
for dialect/conversion mechanics. CPU interpreter, target lowering, scheduling,
compiled-helper and runtime ownership remain here.

During implementation, use focused checks to resolve the changed path. Before
accepting a performance change, preserve whole advertised-corpus differential
correctness across interpreter/JIT/AOT. Select that coverage once per required
phase through [$verify](../verify/SKILL.md) skill. The active Exit Gate owns full
profile acceptance; Intel rows remain in milestone-2.0.0.0 under decision-0040.
Report the implemented behavior, actual executor evidence, measured limits and
remaining cost; then hand the coherent diff to [$review](../review/SKILL.md) skill.
