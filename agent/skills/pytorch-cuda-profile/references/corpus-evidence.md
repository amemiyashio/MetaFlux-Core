# Corpus and Execution Evidence

Use this topic for an operation row, multi-request application outcome, executor
qualification or benchmark. Begin at the exact case in the
[frontier corpus](../../../../tests/compatibility/pytorch_cuda_cpu_frontier_corpus_v1.json),
then `operation_cases`, `application`, `compiled_ptx_sources` and
`qualify_executors` in the [runner](../../../../tests/compatibility/run_pytorch_cuda_cpu_frontier.py).
The [CPU work item](../../../plan/milestone-0.2.0.0-pytorch-cuda-compatibility/work/work-item-0.2.0.2-torch-kernel-intake.md)
owns current coverage and the remaining Exit Gate; preserve its finite,
not-yet-frozen status unless the assigned work actually satisfies that gate.

## One application may emit several requests

Trace the stock API call to all reached provider kernels and library calls.
`expected_requests` describes the corpus expectation; `compiled.ptx` accepts one
source or a list, with one source per baseline request in request order. The
integer reduction case, for example, reaches cast-copy and reduction requests.
Keep the intermediate request's dtype, artifact and observable contribution to
the final output in view. Adding only the last request hides missing execution.

When a row is missing, identify the first absent translation, semantic form,
artifact or executor behavior and implement that coherent producer-to-consumer
slice. Reuse the [profile artifacts](../../../../plugins/compat/cuda/abi/driver/profiles/pytorch-cuda-cpu-v1/)
and neutral operation identity; compose the semantic or backend owner when its
meaning or execution must change. Extend the corpus and oracle for the actual
accepted forms and retain typed rejection for outside forms. Do not replace
stock application code, loosen a tolerance to conceal a defect, or introduce
provider-local math to make a row pass.

For library rejection and stock retry behavior, compose
[$cublas-compatibility](../../cublas-compatibility/SKILL.md) skill and its
[admission reference](../../cublas-compatibility/references/library-admission.md).
Follow the complete client retry and final output/error, including an unfused
path. A handled first error does not establish the application's final outcome.

## Attribute execution to the actual mode

`parse_provider_evidence` collects requests, library calls, launch attempts,
module loads, warm hits and local traces. `qualify_executors` requires correlated
daemon `MF_CPU_EXECUTION` records, including process/session/request, module
generation, operation, executor and status. Distinct session/request identities,
expected operations and completion counts must agree with successful launches.
`MF_LAUNCH`, `MF_SEMANTIC` or a warm-hit marker alone is insufficient.

Read `require_execution_statistics`, `cache_identities` and `prewarm_aot` for
mode-specific evidence. Interpreter claims require the interpreter; compiled
claims require the compiled executor and appropriate cold-JIT, warm-cache or
AOT provenance. An operation-specific daemon tensor branch does not establish
generic CPU coverage, and CPU success does not qualify physical Vulkan execution.
Compose [$compiler-artifact-cache](../../compiler-artifact-cache/SKILL.md) skill
for cache identity/materialization defects and
[$daemon-execution-runtime](../../daemon-execution-runtime/SKILL.md) skill for
accepted requests that fail to reach or complete the executor.

Select the existing check after the implementation delta is concrete:

| Evidence needed | Source |
| --- | --- |
| Stock import, enumeration, copy, initial intake or eager add | [Stock baseline](../../../../tests/compatibility/run_pytorch_cuda_stock_baseline.py) |
| Exact profile operation, multi-request result and executor modes | [Frontier runner](../../../../tests/compatibility/run_pytorch_cuda_cpu_frontier.py) |
| Fresh/cached library rejection and subsequent client retries | [Matmul admission](../../../../tests/compatibility/run_pytorch_cuda_matmul_admission.py) |
| Registered modes and prerequisites | [CTest registration](../../../../tests/compatibility/registration.cmake) |

Harness self-tests qualify harness behavior rather than stock-client acceptance.
Use the exact pinned manifest, selected case/subset, execution mode and required
prerequisites; the workflow's review/verification plan owns the final covering set.
Benchmark only the requested path after correctness, recording raw measurements,
mode, cache state and client/toolchain identity. Structural warm-path evidence
supports allocation/registration claims; latency and throughput need measurements.
Use [$process-activation](../../process-activation/SKILL.md) skill for public
activation behavior rather than promoting harness-private environment setup.
