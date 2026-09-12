# Stock-Client and Artifact Intake

Read this topic for pinned stock PyTorch startup, internal tables, fatbin and
kernel-argument decoding, shape admission or module intake. Begin with the
actual failing stage and selected
[client manifest](../../../../toolchains/pytorch-cuda-clients-1.json), then the
[CPU work item](../../../plan/milestone-0.2.0.0-pytorch-cuda-compatibility/work/work-item-0.2.0.2-torch-kernel-intake.md).
The [corpus](../../../../tests/compatibility/pytorch_cuda_cpu_frontier_corpus_v1.json)
owns current rows; the work item owns their prose summary and remaining scope.
Do not duplicate counts or infer broader application support from a finite row.

## First source path

| Observed stage | Start here |
| --- | --- |
| Public symbol or version resolution | `cuGetProcAddress_v2` in [provider.c](../../../../plugins/compat/cuda/abi/driver/src/provider.c); compose [$cuda-driver-abi-compatibility](../../cuda-driver-abi-compatibility/SKILL.md) skill for the ABI binding |
| cudart internal-table handshake | `cuGetExportTable` in [provider_stubs.c](../../../../plugins/compat/cuda/abi/driver/src/provider_stubs.c) and [profile matrix](../../../../plugins/compat/cuda/abi/driver/profiles/pytorch-cuda-cpu-v1.json) |
| Context-free library/fatbin intake and kernel names | `cuLibraryLoadData`, `mf_module_parse_kernels`, `mf_module_collect_elf_kernels` in the Driver provider |
| Initial PTX image or neutral request registration | `cuModuleLoadData` in the Driver provider |
| Exact profile variant and normalized arguments | `mf_cuda_launch_kernel` and `mf_cuda_materialize_pytorch_baseline_locked` in the Driver provider |
| Neutral registration, module retention, actual completion | [Runtime kernel-request lifetime](../../runtime-contracts-registry/references/kernel-request-lifetime.md) |

Internal UUIDs and slots are observations of the pinned client builds. Keep
their status and binding in the existing profile; an unclassified reached slot
returns its typed unsupported result. Do not turn a successful loader handshake
into an execution capability or a CUDA Driver guarantee.

## Fatbin and argument decoding

`cuLibraryLoadData` can retain a library before a context exists. Follow the
actual body: it records the blob and deferred module locally, with no daemon
artifact registration or executable module load at that intake point.
`mf_module_parse_kernels` walks fatbin entries; `mf_module_collect_elf_kernels`
extracts bounded ELF symbol names and binds them to module generation. Extracted
names are admission inputs, not execution of the embedded cubin instructions.
Preserve blob lifetime, section/string bounds, name-arena limits, module generation
and cleanup when changing this path.

Trace a reached kernel name into `mf_cuda_launch_kernel`, then its argument
decoding, pointer spans, scalar representation, dimensions and launch geometry.
Matching a familiar template name alone does not admit every dtype or shape.
Keep exact accepted forms aligned between Driver admission, the
[corpus](../../../../tests/compatibility/pytorch_cuda_cpu_frontier_corpus_v1.json)
and corresponding [PTX artifacts](../../../../plugins/compat/cuda/abi/driver/profiles/pytorch-cuda-cpu-v1/).
The [Driver build](../../../../plugins/compat/cuda/abi/driver/CMakeLists.txt)
generates `pytorch-cuda-cpu-kernels.h`; edit the owned profile inputs rather than
generated output. Use the pinned client manifests and recorded trace to justify
an argument-layout observation; do not generalize it across PyTorch releases.

## Registration and execution are distinct

`cuModuleLoadData` distinguishes raw PTX from a validated neutral kernel request.
The matmul registration path deliberately defers executable module loading.
A registration artifact can therefore exist without a daemon executable module.
The real launch must admit the exact descriptor and select a profile artifact
with actual semantics before materialization. A `ret`-only placeholder cannot
implement an unmatched shape, dtype or operation.
Compose [$cublas-compatibility](../../cublas-compatibility/SKILL.md) skill for
library descriptors, scalars, heuristics, epilogues and status mapping. This
profile owns the executable variant selected from that normalized request.

Normalize only arguments and descriptors in the provider. Validate pointer range,
context, generation, launch shape and profile constraints before submission.
Use [$ptx-simt-semantics](../../ptx-simt-semantics/SKILL.md) skill when source meaning
changes and [$runtime-contracts-registry](../../runtime-contracts-registry/SKILL.md) skill
when the neutral request changes. The daemon retains canonical Kernel IR;
target compilation/execution belongs to its backend experts.

Trace fresh and reused module/function handles. Warm registration reuse depends
on module/operation/variant identity, not just a previous successful launch.
Diagnostic `MF_PYTORCH_BASELINE_WARM_HIT` is emitted only after that match;
registration/module/launch traces still require correlated executor completion.
Preserve context/module teardown and outstanding-reference lifetime when changing
this path. Reuse existing contracts and qualified shapes unless the task extends
their acceptance scope.

For a missing row, implement its first absent producer-to-executor behavior,
then select the [corpus evidence](corpus-evidence.md) path. Stock source/wheel/API
remain unchanged. Generic interpreter and compiled claims require their actual
executors and mode-specific provenance/cache identity; an operation-specific
daemon tensor branch does not establish generic CPU or Vulkan coverage.
