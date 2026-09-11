# MetaFlux Skills

Enter through [$main](main/SKILL.md) skill, then read only the workflows and domain
skills that own the task. The 19 skill packages remain in this single directory;
`.agents/skills` links here. Shared scripts and references belong inside the
[$main](main/SKILL.md) skill package; it owns the
[skill-reference convention](main/SKILL.md#skill-references).
The roster and explicit-only policy have one owner in
[`tools/check-agent-state.py`](../../tools/check-agent-state.py).

## Workflow

| Skill | Status | Use when |
| --- | --- | --- |
| [$main](main/SKILL.md) skill | Active | Entry, status/resume, maintenance, readiness, identity, knowledge promotion, guarded commit, and publication |
| [$epoch](epoch/SKILL.md) skill | Active | Explicit route proposal or confirmed semantic governance |
| [$batch](batch/SKILL.md) skill | Active | Read-only delivery inspection or automatic integration, acceptance, and progress advancement |
| [$iteration](iteration/SKILL.md) skill | Active | One application-assigned product implementation and candidate delivery |

## Domain

| Skill | Status | Use when |
| --- | --- | --- |
| [$runtime-contracts-registry](runtime-contracts-registry/SKILL.md) skill | Active | Neutral registry, client protocol, shared layout, backend ABI, and schema ownership |
| [$cuda-driver-abi-compatibility](cuda-driver-abi-compatibility/SKILL.md) skill | Active | CUDA Driver symbols, objects, and error behavior |
| [$nvml-telemetry-compatibility](nvml-telemetry-compatibility/SKILL.md) skill | Active | NVML telemetry and stock nvidia-smi |
| [$ptx-simt-semantics](ptx-simt-semantics/SKILL.md) skill | Active | PTX and SIMT semantic-oracle meaning |
| [$mlir-compiler-engineering](mlir-compiler-engineering/SKILL.md) skill | Active | Kernel IR/MLIR conversion and compiler passes |
| [$cpu-backend-performance](cpu-backend-performance/SKILL.md) skill | Active | CPU execution, SIMD, topology, and performance |
| [$linux-device-driver-uapi](linux-device-driver-uapi/SKILL.md) skill | Active | Linux cdev UAPI, mapping, DMA, ordering, and lifetime |
| [$gpu-virtualization-vfio-user](gpu-virtualization-vfio-user/SKILL.md) skill | Active | vfio-user transport and containment |
| [$pcie-vpci-device-model](pcie-vpci-device-model/SKILL.md) skill | Active | PCI/vPCI config, BAR/MSI-X, binding, and hotplug |
| [$device-lifecycle-resilience](device-lifecycle-resilience/SKILL.md) skill | Active | Cross-adapter generation, drain, and fault state machines |
| [$vulkan-spirv-compute](vulkan-spirv-compute/SKILL.md) skill | Active | Vulkan/SPIR-V compute and target-runtime behavior |

## Utility

| Skill | Status | Use when |
| --- | --- | --- |
| [$manage-toolchain](manage-toolchain/SKILL.md) skill | Active | Pinning tools, manifests, shells, SDK inputs, and Nix exposure |
| [$manage-host-privilege](manage-host-privilege/SKILL.md) skill | Active | Bounded privilege, confirmed-gap package provisioning, or driver debugging |
| [$add-component](add-component/SKILL.md) skill | Active | Registering a checked component boundary |
| [$close-decision](close-decision/SKILL.md) skill | Active | Closing an evidenced product decision in its canonical owner |

## Composition

[$main](main/SKILL.md) skill dispatches [$iteration](iteration/SKILL.md) skill,
then [$batch](batch/SKILL.md) skill for product delivery; it dispatches
[$epoch](epoch/SKILL.md) skill only for an explicit request. That is the sole
explicit-only workflow entry.
Read-only readiness, delivery checks, identity, publication diagnostics, and
knowledge promotion remain independently accessible through [$main](main/SKILL.md) skill.

The parent reviews coding-subagent work before another dispatch. Maintenance,
Batch acceptance, and Epoch activation publish automatically unless the user
limits publication. No script creates agents, source copies, or application
contexts. Every stop uses the diagnostic contract of [$main](main/SKILL.md#task-stop-diagnostics) skill.
The bilingual [routing corpus](trigger-evals.json) checks entry boundaries;
behavioral tests separately check executable transitions.
