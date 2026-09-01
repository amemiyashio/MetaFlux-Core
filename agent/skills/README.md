# MetaFlux Skills

Repository skills provide task-specific instructions. Read `start-work` first,
then load only skills that own a material part of the current Iteration,
integration, or explicit Epoch governance.

Packages follow the Codex skill format: a lowercase-hyphenated directory with a
frontmatter-bearing `SKILL.md`, optional `agents/openai.yaml`, and only the
references or scripts the workflow actually needs. `.agents/skills` resolves to
this directory.

## Index

| Skill | Status | Use when |
| --- | --- | --- |
| [start-work](start-work/SKILL.md) | Active | Beginning any repository task or Iteration |
| [integrate-batch](integrate-batch/SKILL.md) | Active | The user explicitly requests integration of committed Iterations |
| [govern-epoch](govern-epoch/SKILL.md) | Active | The user explicitly requests destructive Epoch governance |
| [roast](roast/SKILL.md) | Active | Explicitly promoting valuable knowledge at integration or governance boundaries |
| [manage-toolchain](manage-toolchain/SKILL.md) | Active | Pinning tools, manifests, shells, SDK inputs, or Nix exposure |
| [manage-host-privilege](manage-host-privilege/SKILL.md) | Active | Bounded sudo/su, confirmed-gap package installation, or driver debugging |
| [add-component](add-component/SKILL.md) | Active | Adding a new checked build/component boundary |
| [close-decision](close-decision/SKILL.md) | Active | Resolving an open product decision into canonical authority |
| [implementation-readiness](implementation-readiness/SKILL.md) | Active | Assessing architecture and workstream implementation readiness |
| [runtime-contracts-registry](runtime-contracts-registry/SKILL.md) | Active | Registry, client protocol, shared layout, backend ABI, or schema ownership |
| [cuda-driver-abi-compatibility](cuda-driver-abi-compatibility/SKILL.md) | Active | CUDA Driver ABI, object, symbol, and error behavior |
| [nvml-telemetry-compatibility](nvml-telemetry-compatibility/SKILL.md) | Active | NVML lifecycle, telemetry, and stock nvidia-smi compatibility |
| [ptx-simt-semantics](ptx-simt-semantics/SKILL.md) | Active | PTX subset and SIMT semantic-oracle meaning |
| [mlir-compiler-engineering](mlir-compiler-engineering/SKILL.md) | Active | Kernel IR/MLIR conversion mechanics and compiler passes |
| [cpu-backend-performance](cpu-backend-performance/SKILL.md) | Active | CPU execution, topology, SIMD lowering, and performance evidence |
| [linux-device-driver-uapi](linux-device-driver-uapi/SKILL.md) | Active | Linux cdev UAPI, mmap, pinning, DMA, ordering, and lifetime |
| [gpu-virtualization-vfio-user](gpu-virtualization-vfio-user/SKILL.md) | Active | vfio-user negotiation, DMA, reset, disconnect, and containment |
| [pcie-vpci-device-model](pcie-vpci-device-model/SKILL.md) | Active | PCI/vPCI config, BAR/MSI-X, identity, binding, reset, and hotplug |
| [device-lifecycle-resilience](device-lifecycle-resilience/SKILL.md) | Active | Cross-adapter generation, replacement, drain, and fault state machines |
| [vulkan-spirv-compute](vulkan-spirv-compute/SKILL.md) | Active | Vulkan compute, SPIR-V, memory/sync, caches, and device loss |

## Composition

- Worker Iterations compose `start-work` with every affected domain skill.
- Explicit Batch integration composes `integrate-batch`, affected domain
  skills, and `roast` before final acceptance.
- Explicit destructive governance composes `govern-epoch` and `roast`; domain
  skills retain product semantics.
- Tool changes use `manage-toolchain`. A confirmed Nix gap or privileged driver
  action additionally uses `manage-host-privilege`.
- Decision closure uses `close-decision`; a breaking repository-wide
  replacement instead belongs to explicit `govern-epoch`.

The trigger corpus and routing checker validate the routed domain and workflow
descriptions. Routing success does not substitute for domain verification.
