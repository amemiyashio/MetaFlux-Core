# Library Admission and Client Retries

Read for cuBLAS/cuBLASLt calls, fixed profile variants or rejection
behavior. Begin at `cublasSgemm_v2`, `cublasLtMatmulAlgoGetHeuristic` or
`cublasLtMatmul` in the [adapter](../../../../plugins/compat/cuda/libraries/cublas/src/provider.c).
Read the [current library contract](../../../../plugins/compat/cuda/libraries/cublas/README.md)
and [CPU work item](../../../plan/milestone-0.2.0.0-pytorch-cuda-compatibility/work/work-item-0.2.0.2-torch-kernel-intake.md)
before widening dtype, dimensions, layout, batching, scalar or epilogue admission.
Those sources own accepted configurations; the profile is not general GEMM.

## Admission across the adapter and Driver

The C17 adapter resolves the selected Driver interface, owns its library handles
and descriptors, and translates accepted calls into neutral requests. It never
loads vendor cuBLAS for computation or reads/writes tensor buffers. The Driver
owns device-pointer/context validation, stream ordering, daemon submission and
completion. Neutral schema changes compose
[$runtime-contracts-registry](../../runtime-contracts-registry/SKILL.md) skill;
actual math and lowering stay with PTX/compiler/backend experts.

Follow [descriptor translation](descriptor-translation.md) through creation,
attribute setters, heuristic selection and launch as one client sequence.
Separate malformed descriptors from well-formed requests outside the implemented
profile. Preserve existing typed statuses and their CUDA-to-cuBLAS mapping;
heuristic acceptance does not promise that every shape has an executable variant.

At Driver launch admission, check the exact supported variant before executable
module materialization. Initial library artifact registration is separate from
an executable load: neither its placeholder body nor a cached supported variant
supplies semantics for another shape. Compose
[$pytorch-cuda-profile](../../pytorch-cuda-profile/SKILL.md) skill for that
variant's argument normalization, PTX artifact and materialization. Its
[intake reference](../../pytorch-cuda-profile/references/stock-client-intake.md)
owns the matching module/operation/variant requirement for warm reuse. A
fixed-profile rejection repair adds no supported dimensions by itself.

## Reachability and evidence

Trace all configurations admitted by the changed branch, not only declared
positive corpus rows. Exercise fresh unsupported handles and a supported →
unsupported → supported sequence. Rejection must leave the prior supported
variant usable and preserve object, module, stream and pending-reference lifetime.

Continue through the stock client's retry. The checked-in admission gate observes
SGEMM's propagated unsupported status and the pinned cuBLASLt bias-linear probe's
warning followed by its unfused path. Inspect that path's final result; the first
typed rejection alone does not establish the application outcome. Do not promote
one observed retry sequence to a guarantee for other client builds or shapes.

Use the [adapter tests](../../../../plugins/compat/cuda/libraries/cublas/tests/provider_test.c)
for API/descriptor negatives and
[real-client admission gate](../../../../tests/compatibility/run_pytorch_cuda_matmul_admission.py)
for stock retries and cached reachability. Correlate exact outputs or errors with
module loads, launch attempts and actual executor completions. Rejected attempts
must not execute a placeholder or mismatched cached module.

The gate distinguishes pre-admission `MF_LAUNCH` attempts from execution. A fresh
library request may register its initial artifact while still rejecting the
shape before any executable module load. A rejected attempt must add neither a
warm-hit claim nor an executor completion. Use the profile expert's
[corpus evidence](../../pytorch-cuda-profile/references/corpus-evidence.md) when
the application produces multiple requests or the final unfused path changes.

Select the applicable mode in [CTest registration](../../../../tests/compatibility/registration.cmake)
after the coherent edit. Inspect successful warm-path allocation/registration
changes structurally; claim latency or throughput only from actual measurements.
