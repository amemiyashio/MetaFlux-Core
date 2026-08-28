---
name: vulkan-spirv-compute
description: Design or review Vulkan 1.3 compute device and queue selection, capability profiles, buffer device address, memory and Synchronization2, SPIR-V target environments and validation, pipeline caches, and device loss. Use for M0004 Vulkan backend work. Do not use for MLIR conversion mechanics, graphics, presentation, or unsupported subgroup assumptions.
---

# Vulkan and SPIR-V Compute

## Inputs

- The active M0004 work item, exact Vulkan loader/ICD/driver-family matrix,
  physical-device properties/features/limits, and serialized target environment.
- Kernel IR semantic requirements, SPIR-V module/reflection, packed argument ABI,
  memory tier, queue/timeline plan, cache keys, and lifecycle generation.
- Validation output, differential fixtures, API traces, pipeline/cache evidence,
  and device-loss fault traces relevant to the task.

Use only features actually queried and enabled for the selected logical context.
A reported physical capability is not usable until enabled and represented in the
target environment and cache identity.

## Routing

- Use [capabilities and target environment](references/capabilities-target-env.md)
  for device/queue selection, features, limits, BDA, and SPIR-V constraints.
- Use [memory and synchronization](references/memory-sync.md) for memory tiers,
  imports, queues, Synchronization2, timelines, and stream/event ordering.
- Use [SPIR-V validation](references/spirv-validation.md) for module environment,
  validation, reflection, packed arguments, and semantic diagnostics.
- Use [cache and device loss](references/cache-device-loss.md) for portable and
  device caches, warm launch, eviction, corruption, and lifecycle integration.
- Route dialect/conversion/pass implementation to `$mlir-compiler-engineering`,
  PTX source meaning to `$ptx-simt-semantics`, and cross-transport generation
  handling to `$device-lifecycle-resilience`.

## Workflow

1. Freeze the driver-family/device matrix and query API version, extensions,
   features, limits, queue families, memory properties, UUIDs, and BDA support.
   Select and enable one coherent capability profile.
2. Serialize an exact target environment used by lowering, validation,
   reflection, runtime checks, diagnostics, and cache keys. Reject any mismatch
   before pipeline creation.
3. Define CTA/workgroup, builtin, Workgroup storage, barrier, atomic, FP,
   subgroup, pointer/BDA, and packed-argument mappings. Unsupported CUDA/PTX
   semantics fail explicitly; never fall back per kernel inside a Vulkan context.
4. Select one truthful memory tier per allocation/import and define ownership,
   permissions, flush/invalidate, external synchronization, generation, and
   unregister/device-loss lifetime.
5. Preserve stream FIFO, cross-stream dependencies, default/PTDS behavior, copy
   visibility, and event semantics using `vkQueueSubmit2` and timeline semaphores.
   Batching may preserve but never weaken dependencies.
6. Validate and reflect SPIR-V before creating a shader module/pipeline. Publish
   portable and device-bound cache entries atomically with complete identities.
7. Integrate device loss with the M0003 authority: stop admission, isolate old
   resources, publish lost by deadline, and never reuse a failed context or its
   device addresses.

## Output

Return or implement:

- A selected capability/limit/queue/memory profile and serialized target digest.
- A semantic mapping and unsupported-diagnostic table.
- Memory/synchronization/resource-lifetime and packed-argument contracts.
- Portable/device cache identity plus validation, differential, warm-path, and
  device-loss qualification evidence across the required driver families.

## Verification

- Cross-check queried, enabled, target-environment, SPIR-V-declared, reflected,
  and cache-key capabilities; any disagreement is a hard pre-pipeline failure.
- Run `spirv-val` for the exact Vulkan environment and negative tests for missing
  features, limits, storage classes, scopes, memory semantics, layouts, and BDA.
- Differentially compare every advertised form on at least two independent
  Vulkan driver families with CPU/interpreter/native references as applicable.
- Test FIFO, cross-stream, default/PTDS, copies, barriers, atomics, imports,
  unregister, cache corruption/change, and device loss over memfd, cdev, and
  guest vfio-user where the milestone requires them.
- Trace warm launch to prove no MLIR/SPIR-V compiler/validator, shader module,
  pipeline creation, Vulkan allocation, or MetaFlux heap allocation occurs.
