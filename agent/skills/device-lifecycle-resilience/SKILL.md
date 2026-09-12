---
name: device-lifecycle-resilience
description: Implement or review generation and epoch state machines across cdev, vfio-user, vPCI, registry, workers, QMP events, reset, remove, re-add, provider enumeration freeze, tombstones, idempotence, deadlines, and fault injection. Use for the milestone-0.1.2.0 lifecycle authority and any backend or transport adapter, including milestone-0.1.3.0 device loss. Do not use for layer-local wire layouts or target compiler/runtime behavior.
---

# Device Lifecycle Resilience

Own cross-adapter device generation and retirement state. First trace the
requested event through `runtime/core/src/lifecycle_normalizer.cpp`,
`lifecycle_dispatch.cpp` and `lifecycle.cpp` to its adapter. Identify the
missing guard, side effect, commit or stale-object behavior and implement it
through the affected layers; a model-only pass does not complete the adapter.

Analysis/review requests stay read-only; implementation steps apply to requested
changes. Use the assignment and Exit Gate through [$main](../main/SKILL.md) skill.
Before adapter implementation, read [model checking](references/model-checking.md)
and run its mandatory bounded model command through the clean Nix entry.
After that prerequisite passes, implement the adapter; do not repeatedly run an
unchanged model in place of source work. Follow
[implementation guidance](../review/references/implementation-guidance.md).

## Select The Work

| Task | Read before changing that boundary |
| --- | --- |
| Request acceptance, identity, guards or commit | [State machine](references/state-machine.md) |
| Choose authority/adapter source and a coherent slice | [Adapter implementation](references/adapter-implementation.md) |
| QMP reply/event/disconnect correlation | [QMP events](references/qmp-events.md) |
| Side-effect failure, timeout or stale callback | [Failure injection](references/failure-injection.md) |
| Process-view range/admission/telemetry races | [View races](references/view-races.md) with the runtime owner |
| Integration/stress or promotion scope | [Qualification](references/qualification.md) |

The canonical model is
`contracts/protocol/transport/v1/schema/extensions/lifecycle/v1/model.json`.
The daemon authority alone consumes never-reused generation candidates and
atomically retires old identity, increments device epoch exactly once and,
for reset/recover, installs the staged candidate. Checked capacity precedes
acceptance; transport loss alone preserves epoch. Old objects remain tombstones.
Device epoch is distinct from the workflow Epoch and compiler epoch.

## Compose At The Crossing

Pair the affected adapter with [$linux-device-driver-uapi](../linux-device-driver-uapi/SKILL.md) skill,
[$gpu-virtualization-vfio-user](../gpu-virtualization-vfio-user/SKILL.md) skill,
[$pcie-vpci-device-model](../pcie-vpci-device-model/SKILL.md) skill or
[$vulkan-spirv-compute](../vulkan-spirv-compute/SKILL.md) skill.
[$runtime-contracts-registry](../runtime-contracts-registry/SKILL.md) skill owns
process-view membership, ordering, freeze and first visibility; add it when
those rules change. Existing-event emission alone retains its existing contract.

Return the implemented transition and real adapter outcome, model evidence,
exact fault/terminal behavior and remaining acceptance scope to parent review.
No adapter invents identity or success; deadlines require a terminal outcome
even when physical work remains isolated rather than cancelled.
