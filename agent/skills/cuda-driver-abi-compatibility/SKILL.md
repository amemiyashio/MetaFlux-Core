---
name: cuda-driver-abi-compatibility
description: Implement or review CUDA Driver ABI exports, version resolution, contexts, modules, memory, streams, events and observable errors. Own generic CUDA object and call semantics; compose the PyTorch profile and cuBLAS experts for stock-kernel intake and library translation.
---

# CUDA Driver ABI Compatibility

Start with the requested client call and its first missing translation or
lifecycle behavior. Read the relevant work item and selected provider manifest,
then trace that call in
[Driver dispatch](../../../plugins/compat/cuda/abi/driver/src/dispatch.c),
[Driver behavior](../../../plugins/compat/cuda/abi/driver/src/provider.c).
Locate the failing ABI binding, object transition or error observation point
before choosing an edit. Delegate stock profile and library-specific behavior
through their owning experts below.

Read the complete reference for the affected task. Explicit analysis or
review-only requests stay read-only; benchmark requests measure the requested
path without implicitly changing behavior. For implementation, deliver the missing
behavior and real consumer using the shared
[implementation guidance](../review/references/implementation-guidance.md).

| Task | Read |
| --- | --- |
| Export, version alias, typed stub, `cuGetProcAddress` | [Symbols and versioning](references/symbols-and-versioning.md) |
| Context/module/function/memory/stream/event lifetime or CUDA errors | [Object semantics](references/object-semantics.md) |
| Stock PyTorch internal tables, fatbins, argument normalization or executable profile | [$pytorch-cuda-profile](../pytorch-cuda-profile/SKILL.md) skill |
| cuBLAS/cuBLASLt descriptors, scalars, heuristics, status mapping or retries | [$cublas-compatibility](../cublas-compatibility/SKILL.md) skill |
| Select ABI and handle checks for the completed slice | [Qualification](references/qualification.md) |

Keep these boundaries visible throughout implementation:

- Pinned headers and manifests fix the ABI. Label behavior as normative,
  observed on a named driver/client build, or MetaFlux-strengthened; an
  observation is not a universal CUDA guarantee.
- Loading a DSO creates no provider state. Initialization is lazy and reentrant;
  handles bind owner, context and generation. Preserve asynchronous error
  observation points and teardown of outstanding references.
- Providers remain C17. They translate requests through
  the neutral client protocol; tensor execution and result materialization
  belong to the daemon/backend. Keep C++/LLVM/MLIR and backend dependencies out.
- Reuse unchanged manifests and object models. Preserve qualified warm-path
  allocation, locking and registration properties when changing call behavior.
  Use the profile and library experts for their admission invariants.

Compose [$runtime-contracts-registry](../runtime-contracts-registry/SKILL.md) skill
for neutral requests, shared views or freeze rules and
[$device-lifecycle-resilience](../device-lifecycle-resilience/SKILL.md) skill for
reset/loss/replacement. PTX meaning belongs to
[$ptx-simt-semantics](../ptx-simt-semantics/SKILL.md) skill; conversion mechanics to
[$mlir-compiler-engineering](../mlir-compiler-engineering/SKILL.md) skill; CPU execution to
[$cpu-backend-performance](../cpu-backend-performance/SKILL.md) skill; telemetry to
[$nvml-telemetry-compatibility](../nvml-telemetry-compatibility/SKILL.md) skill.

Return the behavior delta, affected ABI/lifetime rows, actual client and
executor evidence, typed errors, and remaining unsupported scope. Select checks
for that delta before formal verification; fixture or export success alone
does not qualify the stock client or every execution mode.
