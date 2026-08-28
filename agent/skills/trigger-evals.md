# Expert Skill Trigger Evaluations

This corpus checks routing descriptions for the ten M0001-M0004 domain skills.
Run prompts from a repository-root task with implicit skill discovery enabled.
For a positive case, all expected skills must load; additional workflow skills
such as `$start-work` or `$record-session` are allowed when the prompt requests a
change. For a negative case, the named domain skill must not load solely because
of a nearby keyword. Evaluate routing separately from answer correctness.

## Single-skill cases

| ID | Prompt | Expected domain routing |
| --- | --- | --- |
| CUDA-P1 | Review our `libcuda.so.1` export map, `_v2` aliases, and `cuGetProcAddress` behavior against the pinned CUDA headers. | `$cuda-driver-abi-compatibility` |
| CUDA-P2 | Implement stale CUDA context, stream, event, and device-memory handle errors for the M0001 provider. | `$cuda-driver-abi-compatibility` |
| CUDA-N1 | Explain why a PTX barrier inside divergent control deadlocks in the interpreter. | Do not load CUDA; load `$ptx-simt-semantics` |
| NVML-P1 | Audit repeated `nvmlInit_v2`/shutdown and versioned count/fill APIs in `libnvidia-ml.so`. | `$nvml-telemetry-compatibility` |
| NVML-P2 | Qualify stock `nvidia-smi -L`, CSV, compute-apps, and XML output without inventing thermals. | `$nvml-telemetry-compatibility` |
| NVML-N1 | Tune the CPU worker's 100 ms sampling loop cache locality without changing any NVML contract. | Do not load NVML; load `$cpu-backend-performance` |
| PTX-P1 | Define the supported PTX 9.x `atom` forms, scopes, memory order, and interpreter oracle tests. | `$ptx-simt-semantics` |
| PTX-P2 | Review active masks, reconvergence, early return, and CTA barrier participation in Kernel IR. | `$ptx-simt-semantics` |
| PTX-N1 | Add an MLIR `TypeConverter` materialization for an already-specified Kernel IR pointer type. | Do not load PTX; load `$mlir-compiler-engineering` |
| MLIR-P1 | Design ODS operations, verifiers, dynamic legality, and conversion patterns from Kernel IR. | `$mlir-compiler-engineering` |
| MLIR-P2 | Minimize a pass-manager crash reproducer and fix compiler-epoch cache invalidation. | `$mlir-compiler-engineering` |
| MLIR-N1 | Decide the normative result of a divergent PTX atomic sequence before any lowering exists. | Do not load MLIR; load `$ptx-simt-semantics` |
| CPU-P1 | Choose x86 CPUID/XCR0 feature profiles and a cache-compatible CPU multiversioning plan. | `$cpu-backend-performance` |
| CPU-P2 | Investigate why LLVM 22 did not vectorize the SIMT-to-loop kernel and produce a pinned benchmark. | `$cpu-backend-performance` |
| CPU-N1 | Design a transistor-level Harvard instruction-memory interface for an FPGA core unrelated to MetaFlux. | Do not load the repository CPU backend skill |
| LINUX-P1 | Review a Linux 6.12 cdev ioctl/mmap ABI with VMA krefs and compat callers. | `$linux-device-driver-uapi` |
| LINUX-P2 | Fix `FOLL_PIN|FOLL_LONGTERM` DMA registration unwind and MMIO doorbell ordering. | `$linux-device-driver-uapi` |
| LINUX-N1 | Specify a vfio-user protocol capability reply between QEMU and a userspace server. | Do not load Linux UAPI; load `$gpu-virtualization-vfio-user` |
| VFIO-P1 | Define vfio-user feature negotiation requiring shared memfd guest RAM and mmap-capable DMA fds. | `$gpu-virtualization-vfio-user` |
| VFIO-P2 | Review concurrent `VFIO_USER_DMA_UNMAP`, IOVA epoch reuse, and disconnect drain behavior. | `$gpu-virtualization-vfio-user` |
| VFIO-N1 | Configure physical GPU passthrough with host VFIO/IOMMU and no MetaFlux userspace device server. | Do not load the repository vfio-user skill |
| PCIE-P1 | Specify the Type-0 config image, CI VID/DID/class, BDF allocation, and writable masks for vroot. | `$pcie-vpci-device-model` |
| PCIE-P2 | Review BAR0/BAR2/BAR4 sizing, MSI-X table/PBA, vector masking, and guest driver binding. | `$pcie-vpci-device-model` |
| PCIE-N1 | Diagnose PCIe lane equalization and signal-integrity failures on a physical link. | Do not load the vPCI device-model skill |
| LIFE-P1 | Model idempotent reset with daemon incarnation, generation candidate, epoch, drain deadline, and tombstones. | `$device-lifecycle-resilience` |
| LIFE-P2 | Build QMP reply/event/reconnect fault cases for remove and re-add without stale completion. | `$device-lifecycle-resilience` |
| LIFE-N1 | Change only the byte offset of the guest BAR2 doorbell in a static function with no lifecycle behavior. | Do not load lifecycle; load `$pcie-vpci-device-model` |
| VK-P1 | Select a Vulkan 1.3 compute queue and serialize the exact SPIR-V target environment with BDA limits. | `$vulkan-spirv-compute` |
| VK-P2 | Review Synchronization2, timeline completion, pipeline-cache UUID invalidation, and device loss. | `$vulkan-spirv-compute` |
| VK-N1 | Implement generic MLIR dialect-conversion rollback with no Vulkan or SPIR-V target. | Do not load Vulkan; load `$mlir-compiler-engineering` |

## Composition cases

| ID | Prompt | Required domain skills | Boundary assertion |
| --- | --- | --- | --- |
| COMBO-1 | Add a divergent PTX reduction, lower it through MLIR, vectorize it for x86, and differential-test the CPU result. | `$ptx-simt-semantics`, `$mlir-compiler-engineering`, `$cpu-backend-performance` | PTX defines meaning, MLIR lowers it, CPU chooses code shape and evidence |
| COMBO-2 | Fix a vfio-user guest reset racing BAR2 doorbells, MSI-X completion, pinned-page unmap, and generation replacement. | `$linux-device-driver-uapi`, `$gpu-virtualization-vfio-user`, `$pcie-vpci-device-model`, `$device-lifecycle-resilience` | Pin lifetime, protocol DMA, PCI notification, and lifecycle commit remain separate |
| COMBO-3 | Lower Kernel IR to target-constrained SPIR-V, validate reflection, and create a compatible Vulkan compute pipeline cache entry. | `$mlir-compiler-engineering`, `$vulkan-spirv-compute` | MLIR owns conversion; Vulkan owns target/runtime/cache constraints |
| COMBO-4 | Diagnose why CUDA sees one UUID while stock `nvidia-smi` reports another BDF and device order. | `$cuda-driver-abi-compatibility`, `$nvml-telemetry-compatibility` | Both reconcile against one registry snapshot without sharing provider globals |
| COMBO-5 | Handle Vulkan device loss while a vfio-user guest DMA operation is in flight and publish one terminal generation. | `$vulkan-spirv-compute`, `$gpu-virtualization-vfio-user`, `$device-lifecycle-resilience` | Backend detects loss, transport drains mappings, lifecycle owns public state |

## Static package assertions

The repository check should additionally assert or inspect:

- exactly 15 unique skill directory slugs and matching frontmatter names;
- one index entry for every directory and no stale entry;
- `.agents/skills` resolves to `../agent/skills`;
- every new package contains `SKILL.md` and `agents/openai.yaml`;
- every new default prompt includes its exact `$skill-id`;
- each domain `SKILL.md` contains `Inputs`, `Routing`, `Workflow`, `Output`, and
  `Verification` in that order;
- reference links are local or primary sources, with exact versions deferred to
  active milestone plans, compiler epoch, and pinned headers.
