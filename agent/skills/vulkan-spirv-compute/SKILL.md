---
name: vulkan-spirv-compute
description: Implement or review Vulkan compute lowering, fixed-context physical AMD execution, memory/synchronization, resident pipelines and device loss. Use for the stock PyTorch GPU route or backend changes; MLIR mechanics and CUDA stream translation have separate owners.
---

# Vulkan and SPIR-V Compute

For an implementation request, connect its first missing step to selected-device
completion and readback. For analysis, review or benchmarking, stay within that
requested mode and report evidence without claiming a new GPU route.
Begin with `Session::process_launch` and
module preparation in
[`server.cpp`](../../../services/metafluxd/src/server.cpp), then
`VulkanExecutionRoute::prepare/launch` in
[`vulkan_execution.cpp`](../../../services/metafluxd/src/vulkan_execution.cpp).

For a component task, start instead at its owning compiler/runtime symbol in
the guides below. The current Goal qualifies the CPU profile before the physical
AMD stock-client route; this skill does not reorder that route. Reuse the current
device/capability matrix unless the requested change expands or invalidates it.

| Task | Read only the relevant guide |
| --- | --- |
| Stock request routing, eager add/copy/readback or actual GPU evidence | [Stock daemon route](references/stock-route.md) |
| Device/queue/feature/limit selection or target digest | [Capabilities and target](references/capabilities-target-env.md) |
| New SPIR-V emission, validation, reflection or argument mapping | [SPIR-V validation](references/spirv-validation.md) |
| Memory tiers/imports, visibility, queues, streams/events | [Memory and synchronization](references/memory-sync.md) |
| Resident pipeline/cache, warm launch or loss/replacement generation | [Cache and device loss](references/cache-device-loss.md) |
| Physical throughput, timing windows or overhead attribution | [Benchmarking](references/benchmarking.md) |

Use only queried and enabled features represented by the exact target digest.
Lowering, SPIR-V validation, reflection, runtime and cache must agree before
pipeline creation. Preserve workgroup, FP, address/BDA and packed-argument
semantics; reject unproved subgroup/warp assumptions and unsupported forms.

A logical context fixes its backend before resource success. Per-kernel CPU
fallback never qualifies Vulkan. Complete actual GPU work before publishing
completion, preserving dependencies, visibility and resource generations.
Only explicit staging is the baseline; imported memory requires its exact
handle/ownership/synchronization evidence. Lost devices never reuse old resources.

Prepare reusable pipelines, descriptors, command resources and allocations
before warm execution. Prove no compiler/validator, shader or pipeline creation,
Vulkan allocation or MetaFlux heap allocation on the claimed warm path. A direct
component warm trace does not automatically prove the daemon's warm behavior.

Compose [$mlir-compiler-engineering](../mlir-compiler-engineering/SKILL.md) skill for
conversion mechanics and [$ptx-simt-semantics](../ptx-simt-semantics/SKILL.md) skill for
source meaning. Lifecycle authority uses [$device-lifecycle-resilience](../device-lifecycle-resilience/SKILL.md) skill;
neutral ABI uses [$runtime-contracts-registry](../runtime-contracts-registry/SKILL.md) skill.
Service dispatch uses [$daemon-execution-runtime](../daemon-execution-runtime/SKILL.md) skill;
ordinary client startup uses [$process-activation](../process-activation/SKILL.md) skill.
CUDA default/PTDS translation belongs to provider/runtime before neutral Graph IR.

Verify affected advertised forms on their required device/driver rows and run
the exact SPIR-V validator. Physical stock-client acceptance needs named AMD
device identity and correlated submission/completion; skips, software ICD,
enumeration and CPU results do not qualify it. Dual-driver promotion remains
milestone-2.0.0.0. Hand the implemented behavior and evidence to
[$review](../review/SKILL.md) skill without another unchanged matrix run.
