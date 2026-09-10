# MetaFlux Skills

Enter through [main](main/SKILL.md), then read only the workflows and domain
skills that own the task. The 19 skill packages remain in this single directory;
`.agents/skills` links here. Shared scripts and references belong inside main.
The roster and explicit-only policy have one owner in
[`tools/check-agent-state.py`](../../tools/check-agent-state.py).

## Workflow

| Skill | Status | Use when |
| --- | --- | --- |
| [main](main/SKILL.md) | Active | Entry, status/resume, maintenance, readiness, identity, knowledge promotion, guarded commit, and publication |
| [epoch](epoch/SKILL.md) | Active | Explicit route proposal or confirmed semantic governance |
| [batch](batch/SKILL.md) | Active | Read-only delivery inspection or automatic integration, acceptance, and progress advancement |
| [iteration](iteration/SKILL.md) | Active | One application-assigned product implementation and candidate delivery |

## Domain

| Skill | Status | Use when |
| --- | --- | --- |
| [runtime-contracts-registry](runtime-contracts-registry/SKILL.md) | Active | Neutral registry, client protocol, shared layout, backend ABI, and schema ownership |
| [cuda-driver-abi-compatibility](cuda-driver-abi-compatibility/SKILL.md) | Active | CUDA Driver symbols, objects, and error behavior |
| [nvml-telemetry-compatibility](nvml-telemetry-compatibility/SKILL.md) | Active | NVML telemetry and stock nvidia-smi |
| [ptx-simt-semantics](ptx-simt-semantics/SKILL.md) | Active | PTX and SIMT semantic-oracle meaning |
| [mlir-compiler-engineering](mlir-compiler-engineering/SKILL.md) | Active | Kernel IR/MLIR conversion and compiler passes |
| [cpu-backend-performance](cpu-backend-performance/SKILL.md) | Active | CPU execution, SIMD, topology, and performance |
| [linux-device-driver-uapi](linux-device-driver-uapi/SKILL.md) | Active | Linux cdev UAPI, mapping, DMA, ordering, and lifetime |
| [gpu-virtualization-vfio-user](gpu-virtualization-vfio-user/SKILL.md) | Active | vfio-user transport and containment |
| [pcie-vpci-device-model](pcie-vpci-device-model/SKILL.md) | Active | PCI/vPCI config, BAR/MSI-X, binding, and hotplug |
| [device-lifecycle-resilience](device-lifecycle-resilience/SKILL.md) | Active | Cross-adapter generation, drain, and fault state machines |
| [vulkan-spirv-compute](vulkan-spirv-compute/SKILL.md) | Active | Vulkan/SPIR-V compute and target-runtime behavior |

## Utility

| Skill | Status | Use when |
| --- | --- | --- |
| [manage-toolchain](manage-toolchain/SKILL.md) | Active | Pinning tools, manifests, shells, SDK inputs, and Nix exposure |
| [manage-host-privilege](manage-host-privilege/SKILL.md) | Active | Bounded privilege, confirmed-gap package provisioning, or driver debugging |
| [add-component](add-component/SKILL.md) | Active | Registering a checked component boundary |
| [close-decision](close-decision/SKILL.md) | Active | Closing an evidenced product decision in its canonical owner |

## Composition

Main dispatches iteration, then batch for product delivery; it dispatches epoch
only for an explicit request. Epoch is the sole explicit-only workflow entry.
Read-only readiness, delivery checks, identity, publication diagnostics, and
knowledge promotion remain independently accessible through main.

The parent reviews coding-subagent work before another dispatch. Maintenance,
Batch acceptance, and Epoch activation publish automatically unless the user
limits publication. No script creates agents, source copies, or application
contexts. Every stop uses [main's diagnostic contract](main/SKILL.md#task-stop-diagnostics).
The bilingual [routing corpus](trigger-evals.json) checks entry boundaries;
behavioral tests separately check executable transitions.
