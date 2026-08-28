# Expert Skills

Skills are repository-scoped Codex skill packages: focused instructions,
references, assets, and optional scripts for repeatable work. They are
load-on-demand expertise, not daily reading; Codex first sees each package's
name and description, then reads its full instructions only when the task
matches.

Skills differ from their neighbors by intent:

| Neighbor | Owns | A skill instead owns |
| --- | --- | --- |
| [`experience/`](../experience/README.md) | Validated observations about the world (`Candidate` until reproduced) | Prescribed steps that are correct because the repository machinery enforces them |
| [`templates/`](../templates/README.md) | Record shapes (passive skeletons) | Task-specific instructions and supporting resources |
| `memory/` | Stable context | Task-scoped procedure |

## Codex package form

The [OpenAI Codex skill format](https://developers.openai.com/codex/skills/)
is authoritative. Each skill is one directory named by a durable
lowercase-hyphenated slug with this shape:

```text
skill-name/
  SKILL.md             # required instructions and metadata
  agents/openai.yaml   # optional Codex UI, policy, and dependencies
  scripts/             # optional deterministic helpers
  references/          # optional documentation loaded on demand
  assets/              # optional templates and resources
```

`SKILL.md` uses YAML frontmatter. `name` and `description` are required;
`license`, `allowed-tools`, and `metadata` are accepted standard extensions.
The `name` must equal the directory slug. Lifecycle status is repository
catalog metadata, kept in the index below rather than added to standard skill
frontmatter. The old minimal package containing only `SKILL.md` remains valid.

Physical packages remain under `agent/skills/` so existing durable links do
not move. The repository root's `.agents/skills` symlink is the Codex-native
discovery entry point and must resolve to this directory. Package instructions
should name concrete inputs and outputs; procedures that mutate the repository
should also name proportionate verification evidence.

## Index

`tools/check-agent-records.py` enforces that this README indexes every skill
directory, every package satisfies the Codex `SKILL.md` contract, names match
their slugs, and `.agents/skills` resolves to this catalog. `Draft`, `Active`,
and `Retired` remain the repository lifecycle states; retired packages remain
for history.

| Skill | Status | Use when |
| --- | --- | --- |
| [start-work](start-work/SKILL.md) | Active | Beginning any task, before the first change |
| [add-component](add-component/SKILL.md) | Active | Adding any new boundary target to the build |
| [close-decision](close-decision/SKILL.md) | Active | Resolving a row of the open-decisions ledger |
| [record-session](record-session/SKILL.md) | Active | Recording any work session from start to checkpoint |
| [implementation-readiness](implementation-readiness/SKILL.md) | Active | Assessing whether architecture or a workstream is ready for implementation |
| [cuda-driver-abi-compatibility](cuda-driver-abi-compatibility/SKILL.md) | Active | Implementing or reviewing CUDA Driver ABI, objects, and errors |
| [nvml-telemetry-compatibility](nvml-telemetry-compatibility/SKILL.md) | Active | Implementing or reviewing NVML lifecycle, telemetry, and stock `nvidia-smi` |
| [ptx-simt-semantics](ptx-simt-semantics/SKILL.md) | Active | Defining PTX subset, SIMT control, memory semantics, and the interpreter oracle |
| [mlir-compiler-engineering](mlir-compiler-engineering/SKILL.md) | Active | Engineering Kernel IR/MLIR dialects, conversions, passes, and target lowering |
| [cpu-backend-performance](cpu-backend-performance/SKILL.md) | Active | Mapping SIMT to x86 execution and qualifying CPU correctness/performance |
| [linux-device-driver-uapi](linux-device-driver-uapi/SKILL.md) | Active | Implementing cdev/UAPI, mappings, pinning, DMA, ordering, and kernel lifetime |
| [gpu-virtualization-vfio-user](gpu-virtualization-vfio-user/SKILL.md) | Active | Implementing QEMU vfio-user negotiation, guest DMA, and transport failure |
| [pcie-vpci-device-model](pcie-vpci-device-model/SKILL.md) | Active | Implementing PCI config, BAR/MSI-X, enumeration, binding, and hotplug presentation |
| [device-lifecycle-resilience](device-lifecycle-resilience/SKILL.md) | Active | Coordinating reset/remove/re-add generations, QMP, tombstones, and faults |
| [vulkan-spirv-compute](vulkan-spirv-compute/SKILL.md) | Active | Implementing Vulkan compute capabilities, SPIR-V, memory/sync, cache, and loss |

## Composition routing

Load every skill that owns a material part of a cross-boundary request. Keep one
owner for each decision: source semantics, compiler mechanism, target execution,
transport, presentation, and lifecycle remain separate even when one vertical
slice needs all of them.

| Request shape | Skills to compose | Ownership order |
| --- | --- | --- |
| PTX form lowered and optimized for CPU | `$ptx-simt-semantics` + `$mlir-compiler-engineering` + `$cpu-backend-performance` | PTX owns meaning/oracle; MLIR owns conversion; CPU owns target mapping and measurement |
| CUDA launch plus PTX compiler failure | `$cuda-driver-abi-compatibility` + `$ptx-simt-semantics` + `$mlir-compiler-engineering` | CUDA owns visible API/error; PTX owns accepted form; MLIR owns failing pass |
| CUDA and stock `nvidia-smi` identity mismatch | `$cuda-driver-abi-compatibility` + `$nvml-telemetry-compatibility` | Both inspect one registry view; neither recreates the other's behavior |
| vfio-user BAR/MSI-X reset or unmap race | `$linux-device-driver-uapi` + `$gpu-virtualization-vfio-user` + `$pcie-vpci-device-model` + `$device-lifecycle-resilience` | Linux owns guest pin/UAPI; vfio-user owns wire/DMA; PCI owns regions/IRQs; lifecycle owns generation commit |
| Kernel IR lowered for Vulkan execution | `$mlir-compiler-engineering` + `$vulkan-spirv-compute` | MLIR owns dialect conversion; Vulkan owns target environment, validation, runtime, and cache |
| Vulkan device loss during guest DMA | `$vulkan-spirv-compute` + `$gpu-virtualization-vfio-user` + `$device-lifecycle-resilience` | Vulkan detects target loss; vfio-user isolates guest mappings; lifecycle publishes terminal state |
| vroot add/remove with open cdev mappings | `$linux-device-driver-uapi` + `$pcie-vpci-device-model` + `$device-lifecycle-resilience` | Linux owns refs/VMAs; PCI owns enumeration/binding; lifecycle owns identity and ordering |

The checked prompt corpus in [trigger evaluations](trigger-evals.md) supplies two
positive and one near-miss negative case per domain skill plus composition cases.
It is a routing regression set, not proof of domain correctness.
