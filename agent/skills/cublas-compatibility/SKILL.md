---
name: cublas-compatibility
description: Implement or review the C17 cuBLAS/cuBLASLt compatibility provider, descriptor and scalar translation, layouts, heuristics, epilogues, status mapping and stock-client retries. Own the bounded library API adapter; compose the PyTorch profile expert for executable variants and backend experts for math.
---

# cuBLAS Compatibility

Start at the reached `cublasSgemm_v2`, `cublasLtMatmulAlgoGetHeuristic` or
`cublasLtMatmul` in the [library provider](../../../plugins/compat/cuda/libraries/cublas/src/provider.c).
Read the [library contract](../../../plugins/compat/cuda/libraries/cublas/README.md),
then follow handle/descriptor creation, attribute setters, heuristic selection,
Driver submission and the client's final result. Locate the first missing
adapter behavior before choosing an edit.

Read the complete reference for the affected task. Explicit analysis or
review-only requests stay read-only; benchmark requests measure the specified
path without implicitly changing admission. For implementation, complete the
behavior and its consumer using the shared
[implementation guidance](../review/references/implementation-guidance.md).

| Task | Read |
| --- | --- |
| Handles, layouts, host scalars, heuristics, epilogues or status mapping | [Descriptor translation](references/descriptor-translation.md) |
| Fresh/cached shape rejection, executable variants, stock retries or evidence | [Library admission](references/library-admission.md) |

Keep these boundaries visible:

- This C17 adapter resolves the selected Driver interface, owns library objects
  and translates metadata. It neither loads vendor cuBLAS for computation nor
  reads or writes tensor buffers; actual math belongs to the executor.
- The library contract and implementation own supported types, layouts,
  transpose forms, scalars, batching and epilogues. Bounded SGEMM/Lt support
  establishes neither general GEMM nor cuDNN coverage.
- Preserve exact setter sizes, span/overflow checks, output initialization and
  typed statuses. Malformed input and a well-formed outside-profile request
  may have different results; follow each API's implemented mapping.
- A heuristic describes an accepted descriptor configuration. Executable shape
  admission remains a later profile boundary, before materialization or launch.
  Registration placeholders and cached variants supply no unmatched semantics.
- Preserve handles, streams, module cleanup and pending-reference lifetime.
  Reject a fresh or reused unsupported request without corrupting a previously
  supported variant, and follow stock retries to their final output or error.

Compose [$pytorch-cuda-profile](../pytorch-cuda-profile/SKILL.md) skill for exact
executable variants, PTX artifacts and application request chains;
[$cuda-driver-abi-compatibility](../cuda-driver-abi-compatibility/SKILL.md) skill for
Driver exports, device pointers, contexts, streams and CUDA errors; and
[$runtime-contracts-registry](../runtime-contracts-registry/SKILL.md) skill for
neutral request changes. Math belongs to
[$ptx-simt-semantics](../ptx-simt-semantics/SKILL.md) skill and
[$cpu-backend-performance](../cpu-backend-performance/SKILL.md) skill;
[$daemon-execution-runtime](../daemon-execution-runtime/SKILL.md) skill owns daemon execution.

Return the API/descriptor delta, exact accepted and rejected forms, status
mapping, fresh/reused sequence outcome and actual executor evidence. Select
covering checks after the coherent edit. Claim performance only from requested
measurements, keeping structural warm-path evidence distinct from timing.
