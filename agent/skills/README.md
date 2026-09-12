# MetaFlux Skills

Enter through [$main](main/SKILL.md) skill, then read only the workflows and domain
skills that own the task. The 31 packages remain in this single directory;
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

Start with the expert that owns the requested behavior. Its short entry gives a
first source action and a task-to-reference table; read the selected topic fully
before editing, and compose another expert only at its affected boundary.
Specialist guides preserve the current work-item Exit Gate. They do not create
execution stages or turn a read-only question into implementation.

| Skill | Status | Use when |
| --- | --- | --- |
| [$pytorch-cuda-profile](pytorch-cuda-profile/SKILL.md) skill | Active | Stock kernel/argument intake, finite profile admission, corpus and final client outcome |
| [$cublas-compatibility](cublas-compatibility/SKILL.md) skill | Active | cuBLAS/Lt descriptors, layouts, heuristics, epilogues and typed library errors |
| [$cuda-driver-abi-compatibility](cuda-driver-abi-compatibility/SKILL.md) skill | Active | Generic Driver exports, resolver/version ABI, contexts, objects and CUDA errors |
| [$nvml-telemetry-compatibility](nvml-telemetry-compatibility/SKILL.md) skill | Active | Real telemetry producer-to-snapshot-to-NVML behavior and stock nvidia-smi |
| [$process-activation](process-activation/SKILL.md) skill | Active | Process provider selection, loader coexistence, daemon/socket activation and device access |
| [$runtime-contracts-registry](runtime-contracts-registry/SKILL.md) skill | Active | Neutral kernel requests/lifetimes, registry, shared layout, backend ABI and schemas |
| [$daemon-execution-runtime](daemon-execution-runtime/SKILL.md) skill | Active | Session objects, prepared modules, backend dispatch, completion and teardown |
| [$ptx-simt-semantics](ptx-simt-semantics/SKILL.md) skill | Active | Reusable PTX/Kernel IR forms, SIMT meaning and independent oracles |
| [$mlir-compiler-engineering](mlir-compiler-engineering/SKILL.md) skill | Active | Actual Kernel IR/MLIR emitters, legality, compiler passes and cache identity |
| [$compiler-worker-isolation](compiler-worker-isolation/SKILL.md) skill | Active | Compiler child protocol, process limits, deadlines, cancellation and reaping |
| [$compiler-artifact-cache](compiler-artifact-cache/SKILL.md) skill | Active | Semantic cache identity, per-UID mutable/AOT tiers, atomic publication, pins and eviction |
| [$cpu-backend-performance](cpu-backend-performance/SKILL.md) skill | Active | Generic CPU execution, shape families, SIMD, topology and same-path performance |
| [$linux-device-driver-uapi](linux-device-driver-uapi/SKILL.md) skill | Active | Linux cdev UAPI, mapping, DMA, ordering, and lifetime |
| [$gpu-virtualization-vfio-user](gpu-virtualization-vfio-user/SKILL.md) skill | Active | vfio-user transport and containment |
| [$pcie-vpci-device-model](pcie-vpci-device-model/SKILL.md) skill | Active | PCI/vPCI config, BAR/MSI-X, binding, and hotplug |
| [$device-lifecycle-resilience](device-lifecycle-resilience/SKILL.md) skill | Active | Cross-adapter generation, drain, and fault state machines |
| [$vulkan-spirv-compute](vulkan-spirv-compute/SKILL.md) skill | Active | Fixed AMD Vulkan request-to-GPU completion, SPIR-V and warm runtime |

The 17 experts form three practical groups without extra directory nesting:
client integration (PyTorch, cuBLAS, Driver, NVML and activation), execution and
compilation (neutral contracts, daemon, PTX, MLIR, compiler worker/cache and
CPU/Vulkan), and device transport/lifecycle (Linux UAPI, vfio-user, PCI/vPCI and
lifecycle). These groups organize discovery; they are not new workflow stages.
Select by the missing behavior, then compose only the crossed owners. For
example, a new stock shape family uses profile admission plus PTX/backend
semantics; a compiler timeout starts at worker isolation, without loading a
registry or Driver manual. Path-owned rules select implementation modules;
semantic cross-boundary changes explicitly load their additional owners.

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
The English [routing corpus](trigger-evals.json) checks entry boundaries;
behavioral tests separately check executable transitions.
