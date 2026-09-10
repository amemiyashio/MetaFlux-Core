---
id: work-item-0.1.3.1
delivery: 0.1.3.1
milestone: milestone-0.1.3.0
status: Complete
area: backend.vulkan.contract
depends_on: [milestone-0.1.1.0]
updated: 2026-09-06
---

# Vulkan Capability and Extension Boundaries

## Outcome

Define a reproducible Vulkan 1.3 compute target, packed-argument contract,
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
- [x] Add C ABI layout and host capability regression tests. An unavailable or
  incompatible host produces an unqualified-probe message and zero exit status;
  that generic CTest pass is not a physical-device qualification result.
- [x] Draft the target-digest-bound packed scalar/device-address argument block
  and external-memory 0.x profile, with generation, range, ownership, and
  staging/direct-import negative fixtures.

- [x] Select the exact feature/limit baseline, minimum versions, and two
  independent Vulkan driver families.
  Locked in `contracts/plugin/backend/v1/include/metaflux/backend/vulkan.h`:
  minimum API `MF_VULKAN_BASELINE_MIN_API_VERSION` (= Vulkan 1.3), required
  feature set `MF_VULKAN_BASELINE_REQUIRED_FEATURE_FLAGS` (timeline semaphore +
  Synchronization2 + buffer device address), and dual driver families AMD
  (`0x1002`) / NVIDIA (`0x10DE`) via `mf_vulkan_driver_family_from_vendor_id_v1`.
  Host-independent ABI coverage is `metaflux.contract.backend-vulkan-abi.v1`;
  the physical dual-family matrix is deferred to
  [work-item-2.0.0.3](../../milestone-2.0.0.0-physical-hardware-qualification/work/work-item-2.0.0.3-dual-driver-physical-qualification.md)
  (decision-0040) and is not a completion gate for this milestone.
- [x] Draft packed BDA argument layouts and external-memory 0.x fixtures.
- [x] Define the currently advertised Kernel IR capability subset and stable
  negative diagnostics; complete Kernel IR coverage remains open.
- [x] Define target-environment serialization, cache keys, pipeline residency,
  compile-required handling, corruption behavior, and epoch fingerprints for
  the current host-independent/runtime paths.
- [x] Lock direct-Vulkan baselines plus enqueue, submit, start, and completion
  timestamp points.
  `mf_vulkan_execution_timestamps_v1` freezes the four ordered host-side anchors
  (enqueue → submit → start → completion) with
  `mf_vulkan_execution_timestamps_ordered_v1`. Live GPU sampling still runs under
  the direct component fixtures; physical dual-driver evidence belongs to
  work-item-2.0.0.3.

## Exit Gate

Positive/negative target fixtures, packed layouts, cache keys, capability reports,
and benchmark contracts reproduce on the host-independent dual-family fixtures;
the physical dual-family matrix is owned by milestone-2.0.0.0 (decision-0040).
Packed arguments are ABI v1 under work-item-0.1.3.4's layout freeze;
external memory remains ABI 0.x until its physical qualification and promotion.
