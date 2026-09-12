---
name: pytorch-cuda-profile
description: Implement or review the pinned stock PyTorch CUDA execution profile, fatbin and argument decoding, exact shape admission, PTX artifacts, multi-request outcomes and corpus evidence. Use for stock kernel intake or a missing profile row; compose Driver ABI, cuBLAS, semantics and backend experts for their owned layers.
---

# PyTorch CUDA Profile

Start from the failing stock operation in the
[frontier corpus](../../../tests/compatibility/pytorch_cuda_cpu_frontier_corpus_v1.json)
and selected [client manifest](../../../toolchains/pytorch-cuda-clients-1.json).
Trace its first missing behavior in `cuLibraryLoadData`, `mf_module_parse_kernels`,
`mf_cuda_launch_kernel` or `mf_cuda_materialize_pytorch_baseline_locked` in
[Driver provider.c](../../../plugins/compat/cuda/abi/driver/src/provider.c).
Distinguish startup, name/argument decoding, profile admission, materialization
and actual executor completion before selecting the implementation boundary.

Read the complete reference for the affected task. Explicit analysis or
review-only requests stay read-only; benchmark requests measure the specified
path without implicitly expanding support. For implementation, complete the
missing behavior and its consumer using the shared
[implementation guidance](../review/references/implementation-guidance.md).

| Task | Read |
| --- | --- |
| Internal tables, fatbin names, argument decoding, shape or artifact admission | [Stock-client intake](references/stock-client-intake.md) |
| Add a profile row, follow multiple requests/retries, qualify executors or measure a mode | [Corpus and execution evidence](references/corpus-evidence.md) |

Keep these invariants visible:

- The selected stock source, wheel and public API remain unchanged. Pin client
  observations; do not infer stable internal layouts from a different release.
- This is a finite profile. Its [current work item](../../plan/milestone-0.2.0.0-pytorch-cuda-compatibility/work/work-item-0.2.0.2-torch-kernel-intake.md)
  and corpus own coverage and remaining gaps; a supported row is not a complete
  profile freeze, general CUDA support or Vulkan qualification.
- Providers decode metadata and submit neutral requests in C17. Actual math
  runs through canonical Kernel IR and the selected executor, with no provider
  tensor computation or operation-specific daemon substitute.
- Admit exact dtype, shape, pointers and launch form before executable loading.
  Registration and placeholder PTX are not execution; warm reuse requires the
  matching module, operation and variant. Preserve teardown and pending lifetime.
- Follow every request and client retry to the final result. Match output/error,
  successful launches and executor completions; a trace marker alone proves no
  execution mode or additional supported shape.

Compose [$cuda-driver-abi-compatibility](../cuda-driver-abi-compatibility/SKILL.md) skill
for exports, CUDA objects and errors; [$cublas-compatibility](../cublas-compatibility/SKILL.md) skill
for library descriptors; [$runtime-contracts-registry](../runtime-contracts-registry/SKILL.md) skill
for neutral schema/lifetime; and [$daemon-execution-runtime](../daemon-execution-runtime/SKILL.md) skill
for daemon dispatch/completion. PTX meaning belongs to
[$ptx-simt-semantics](../ptx-simt-semantics/SKILL.md) skill, conversion to
[$mlir-compiler-engineering](../mlir-compiler-engineering/SKILL.md) skill, and CPU execution to
[$cpu-backend-performance](../cpu-backend-performance/SKILL.md) skill. Compose
[$process-activation](../process-activation/SKILL.md) skill for client activation.

Return the admitted/rejected forms, changed profile/artifact/corpus rows, final
client outcome and per-request executor evidence. Implement the coherent slice
before selecting its covering checks; report remaining gaps without changing
the accepted CPU-to-Vulkan route or claiming unmeasured performance.
