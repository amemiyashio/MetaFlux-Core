# MetaFlux Skills

Enter through [$main](main/SKILL.md) skill, then read only the workflows and domain
skills that own the task. The 25 packages remain in this single directory;
`.agents/skills` links here. Shared mechanisms live in `agent/lib/`; stage
helpers and references live with their owning skill. The main entry owns the
[skill-reference convention](main/SKILL.md#skill-references).
The roster and explicit-only policy have one owner in
[`tools/check-agent-state.py`](../../tools/check-agent-state.py).

## Control

| Skill | Status | Use when |
| --- | --- | --- |
| [$main](main/SKILL.md) skill | Active | Read-only entry, task routing and current action card |

## Orchestration

| Skill | Status | Use when |
| --- | --- | --- |
| [$epoch](epoch/SKILL.md) skill | Active | Explicit route proposal or confirmed semantic governance |
| [$batch](batch/SKILL.md) skill | Active | Read-only delivery inspection or automatic integration, acceptance, and progress advancement |
| [$iteration](iteration/SKILL.md) skill | Active | One application-assigned product implementation and candidate delivery |

## Stage

| Skill | Status | Use when |
| --- | --- | --- |
| [$prepare](prepare/SKILL.md) skill | Active | Exact request, initial preparation, identity or scoped readiness |
| [$review](review/SKILL.md) skill | Active | Parent semantic review and material knowledge promotion |
| [$verify](verify/SKILL.md) skill | Active | Covering plan, actual verification and live evidence |
| [$deliver](deliver/SKILL.md) skill | Active | Exact guarded commit and candidate/activation delivery |
| [$publish](publish/SKILL.md) skill | Active | Canonical exact-commit publication and transport diagnosis |
| [$recover](recover/SKILL.md) skill | Active | Failed-cause repair, rescope and interrupted-state recovery |

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
knowledge promotion remain independently accessible through its current stage skill.

The parent reviews coding-subagent work before another dispatch. Maintenance,
Batch acceptance, and Epoch activation publish automatically unless the user
limits publication. No script creates agents, source copies, or application
contexts. Every stop uses the diagnostic contract of [$recover](recover/SKILL.md) skill.
The bilingual [routing corpus](trigger-evals.json) checks entry boundaries;
behavioral tests separately check executable transitions.
