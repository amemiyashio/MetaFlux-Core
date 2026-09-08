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
| [detect-agent-tool](detect-agent-tool/SKILL.md) | Active | Reporting the conversation-emitted harness name without probing executables or model metadata |
| [accept-and-advance](accept-and-advance/SKILL.md) | Active | Automatically accepting a qualified committed Iteration and advancing the route |
| [integrate-batch](integrate-batch/SKILL.md) | Active | Explicit or controller-composed candidate merge and combined verification |
| [govern-epoch](govern-epoch/SKILL.md) | Active | The user explicitly requests destructive Epoch governance |
| [replan-roadmap](replan-roadmap/SKILL.md) | Active | The user explicitly requests a two-stage objective and route replan |
| [roast](roast/SKILL.md) | Active | Explicitly promoting valuable knowledge at integration or governance boundaries |
| [manage-toolchain](manage-toolchain/SKILL.md) | Active | Pinning tools, manifests, shells, SDK inputs, or Nix exposure |
| [manage-host-privilege](manage-host-privilege/SKILL.md) | Active | Bounded sudo/su, confirmed-gap package installation, or driver debugging |
| [push-repository](push-repository/SKILL.md) | Active | Configuring, validating, or explicitly pushing one exact revision to canonical GitHub |
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

- Worker Iterations compose `start-work`, automatic `detect-agent-tool`, and
  every affected domain skill. Coding lookup and mutation prefer a parent
  briefing, a bounded coding subagent, and parent review against drift. The
  next dispatch or Iteration cycle waits until that review accepts the
  briefing goal.
- Automatic acceptance composes `accept-and-advance`, `integrate-batch`,
  affected domain skills, and `roast`; only the controller advances Goal and
  work-item state after combined verification.
- Explicit destructive governance composes `govern-epoch` and `roast`; domain
  skills retain product semantics.
- Explicit route replanning composes `replan-roadmap` with
  `implementation-readiness`; a confirmed semantic proposal then composes
  `roast` and `govern-epoch` for the only destructive cutover.
- Tool changes use `manage-toolchain`. A confirmed Nix gap or privileged driver
  action additionally uses `manage-host-privilege`.
- Epoch publication and automatic acceptance use `push-repository` with the
  just-committed full object ID after that commit is on disk. Ordinary
  Iteration completion never triggers a push, branch allocation, or broad
  refspec. Other pushes still require an explicit user or application request
  and one full commit object ID.
- Decision closure uses `close-decision`; a breaking repository-wide
  replacement instead belongs to explicit `govern-epoch`.

The trigger corpus and routing checker validate the routed domain and workflow
descriptions. Routing success does not substitute for domain verification.
Any loaded Skill condition that blocks the next phase uses the sole
[`start-work` task-stop contract](start-work/SKILL.md#task-stop-diagnostics);
individual Skills name their trigger and legal owner but do not redefine the
diagnostic schema or persist failures.
