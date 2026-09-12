# Descriptor Translation and Status Mapping

Begin in the [C17 provider](../../../../plugins/compat/cuda/libraries/cublas/src/provider.c)
and [current library contract](../../../../plugins/compat/cuda/libraries/cublas/README.md).
Use the reached API sequence to select a branch; do not infer launch support
from successful descriptor construction alone.

| Behavior | Source symbol |
| --- | --- |
| Driver resolution and required bootstrap interface | `mf_cublas_driver_open` |
| Library handle and retained module cleanup | `cublasCreate_v2`, `cublasDestroy_v2` |
| Stream, pointer mode and math mode | `cublasSetStream_v2`, `cublasSetPointerMode_v2`, `cublasSetMathMode` |
| SGEMM descriptor and normalized launch arguments | `cublasSgemm_v2`, `mf_cublas_matrix_span` |
| Lt operation, layout and preference objects | `cublasLtMatmulDescSetAttribute`, `cublasLtMatrixLayoutSetAttribute`, `cublasLtMatmulPreferenceSetAttribute` and their create/destroy functions |
| Shared Lt configuration and heuristic result | `mf_cublas_lt_configuration_locked`, `cublasLtMatmulAlgoGetHeuristic` |
| Scalars, output aliasing, algorithm and launch | `cublasLtMatmul` |
| Driver failure translated to library status | `mf_cublas_map_driver_status` |

## API acceptance and neutral translation

The current SGEMM branch admits host pointer mode with float32 alpha equal to
one and beta equal to zero or one, and normalizes its admitted transpose forms.
`mf_cublas_matrix_span` checks leading dimensions and the bounded element span
using wider arithmetic before launch metadata is constructed. Preserve the
separate output-size checks and the exact API-specific rejection status.

The current Lt configuration requires float32 compute/scale and matrix types,
T/N transpose, column order, a single unstrided batch and a bias epilogue.
Launch additionally checks host scalar bits, alpha one, beta zero, shared C/D
pointer and layout, algorithm identity and valid spans. These are current
implementation boundaries, not a contract for every vendor-library operation.
Read the owning README and code before changing one of them.

Attribute setters validate the requested attribute and exact value size before
updating owned state. Trace create/set/destroy together, including stale or
foreign objects and failed setter state preservation. A descriptor can exist
before its configuration is executable. The heuristic path initializes the
returned count, checks its objects and configuration, and emits the selected
algorithm/workspace result; it does not establish profile shape coverage.

`mf_cublas_prepare_sgemm_locked` and `mf_cublas_prepare_lt_bias_locked` register
neutral MATMUL requests through the selected Driver and retain module/function
handles. Failure unloads a created module and clears those handles; destruction
also releases retained modules. The [admission reference](library-admission.md)
owns the fresh/reused request sequence and composition with the executable
profile. Keep tensor storage access, context/pointer validation, ordering and
completion with the Driver and daemon rather than duplicating those mechanisms.

## Preserve observable errors and prove the affected branch

Follow the actual API's status selection before applying
`mf_cublas_map_driver_status`. The mapping distinguishes invalid value,
allocation failure, initialization failure, unsupported operation and execution
failure. Early argument, object, pointer-mode and configuration checks may select
their own status; avoid a blanket invalid-versus-unsupported rewrite across APIs.
Check output initialization and whether a rejected setter or heuristic preserves
the prior usable state.

Use [provider_test.c](../../../../plugins/compat/cuda/libraries/cublas/tests/provider_test.c)
and [fake_cuda.c](../../../../plugins/compat/cuda/libraries/cublas/tests/fake_cuda.c)
for descriptor/status negatives, module cleanup and the exact Driver calls.
Those fixtures validate the translation boundary. Use the
[real-client admission gate](../../../../tests/compatibility/run_pytorch_cuda_matmul_admission.py)
for final stock outcome, cached variant preservation and actual execution.
Extend the affected producer and consumer together, then select their checks;
broader profile support requires the profile and semantic/backend owners.
