---
id: work-item-0.1.3.1
delivery: 0.1.3.1
milestone: milestone-0.1.3.0
status: Active
area: backend.vulkan.contract
depends_on: [milestone-0.1.1.0]
updated: 2026-08-31
---

# Vulkan Capability and ABI 0.x

## Outcome

Define a reproducible Vulkan 1.3 compute target, packed-argument ABI 0.x,
external-memory extension 0.x, benchmark contract, and semantic capability matrix
before freezing a public backend extension.

The C++20 backend calls the Vulkan C API and crosses the runtime only through
`mf_backend_api_v1` plus sized extension records. Vulkan/C++ handles, types,
exceptions, allocator ownership, and STL objects never cross that C ABI. It uses
the milestone-0.1.0.0 compiler epoch and a separate `vulkan_lowering_epoch` containing the
MLIR pipeline, SPIRV-Tools/validator versions, target-environment serializer, and
packed-argument revision. SPIRV-Tools runs only in an isolated compile/validation
stage, never on cache-hit launch.

The baseline verifies Vulkan 1.3 compute queues, timeline semaphores,
Synchronization2/`vkQueueSubmit2`, buffer device address with
PhysicalStorageBuffer addressing, and every feature/limit needed by an advertised
Kernel IR capability. Target identity serializes the actually enabled
`VkPhysicalDevice*Features` and relevant `VkPhysicalDevice*Properties`, not
merely API version.
Subgroup size is never assumed to be 32.

## Work

Implemented stage:

- [x] Declare the milestone-0.1.3.0 capability ABI 0.x record with fixed-width API,
  queue, subgroup, memory-tier, UUID, and target-environment fields.
- [x] Add an optional C++20 Vulkan probe that requires Vulkan 1.3 compute, a
  compute queue, timeline semaphores, Synchronization2, and buffer device
  address; it serializes the queried profile and hashes it with SHA-256.
- [x] Materialize Vulkan headers, loader, `vulkaninfo`, `glslangValidator`, and
  `spirv-val` in the named Nix `vulkan-tools` output and `.#vulkan` shell.
- [x] Add the separate on-demand `vulkan-runtime` tool output and
  `.#vulkan-runtime` shell with pinned Mesa ICD and Khronos validation layers
  for host smoke/probe execution; keep it out of the lean tool-only shell.
- [x] Add C ABI layout and host capability regression tests; an unavailable or
  incompatible host is reported as a skipped local probe.
- [x] Draft the target-digest-bound packed scalar/device-address argument block
  and external-memory 0.x profile, with generation, range, ownership, and
  staging/direct-import negative fixtures.

- [ ] Select the exact feature/limit baseline, minimum versions, and two
  independent Vulkan driver families.
- [ ] Draft packed BDA argument layouts and external-memory 0.x fixtures.
- [ ] Define the exact Kernel IR capability matrix and negative diagnostics.
- [ ] Define target-environment serialization, cache keys, pipeline residency,
  compile-required proof, corruption behavior, and epoch fingerprints.
- [ ] Lock direct-Vulkan baselines plus enqueue, submit, start, and completion
  timestamp points.

## Exit Gate

Positive/negative target fixtures, packed layouts, cache keys, capability reports,
and benchmark contracts reproduce on both driver families. No Vulkan-specific
public extension is frozen yet.
