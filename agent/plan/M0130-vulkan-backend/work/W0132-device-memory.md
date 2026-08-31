---
id: W0132
delivery: 0.1.3.2
milestone: M0130
status: Active
area: backend.vulkan.memory
depends_on: [W0131]
updated: 2026-08-31
---

# Vulkan Device, Queue, and Memory

## Outcome

Bring up one physical Vulkan device per generation-bound backend instance, with a
bounded queue set, truthful memory tiers, suballocation, timeline synchronization,
and M0120 device-loss hooks before compute shader execution.

A logical context fixes CPU or Vulkan, and fixes its transport, before module
loading/allocation and the first visible success. Supported profiles are M0100
memfd, M0110 cdev, and guest vfio-user only when negotiated capabilities satisfy
the memory contract. An established context never changes backend or transport
after a fault.

Memory tiers are advertised independently:

1. OPAQUE_FD or DMA-BUF external buffers with compatible external semaphores,
   only when the exact device proves import/export support.
2. External host memory only when alignment, pinning, coherency, and driver
   limits permit it.
3. Explicit host-visible staging plus a device-local copy.

Tier 3 alone is required for M0130 completion. A memfd, host pointer, or guest RAM
range is never mislabeled DMA-BUF or zero-copy. The guest bridge accepts only a
validated M0110 generation-bound handle/range/permission set, selects direct
import only for an exactly compatible handle/sync capability, and otherwise
reports staging. Unregister waits for both transport and Vulkan references. No
descriptor or transport UAPI field changes.

The extension records handle type, ownership transfer, size, alignment, memory
type, permissions, generation, dedicated-only status, compatible handle types,
and synchronization capability; Vulkan handles remain private. Large allocations
use backend-owned blocks/suballocation and steady-state launch allocates no Vulkan
memory or host heap object.

## Work

- [x] Add a generation-bound staging suballocation ledger and timeline
  admission model. It consumes the capability profile's truthful staging
  budget, enforces power-of-two alignment and non-overlap, and rejects stale
  generations or completions outside the submitted timeline.
- [x] Add a host-independent visibility ledger for staging allocations. It
  tracks host/device dirty ranges, non-coherent atom-size alignment, explicit
  flush/invalidate operations, submission completion, and in-flight teardown
  rejection without owning Vulkan handles.
- [x] Bind the existing capability profile to a private Vulkan 1.3
  instance/device/compute queue/timeline context. The context rechecks the
  selected device's identity, properties, limits, queue, and required feature
  chain before enabling it; no Vulkan handle crosses `mf_backend_api_v1`.
- [x] Add a source-local physical host-visible staging adapter. It creates a
  generation-bound `VkBuffer` and `VkDeviceMemory`, selects a required
  host-visible memory type while preferring host-coherent memory, maps the
  allocation, and applies queried non-coherent atom-size range normalization to
  flush/invalidate operations without changing `mf_backend_api_v1`.
- [ ] Expose the context through the backend admission path without changing
  the stable C ABI, then implement allocation/suballocation, staging, optional
  direct tiers, non-coherent flush/invalidate, and timeline synchronization.
- [ ] Implement allocation/suballocation, staging, optional direct tiers,
  non-coherent flush/invalidate, and timeline synchronization.
- [ ] Test fd ownership on success/failure, `memoryTypeBits`, overlapping imports,
  cross-process semaphore visibility, teardown, reset, and device loss.
- [ ] Keep external-memory ABI 0.x until baseline staging and every advertised
  direct-tier ownership/coherence matrix pass.

## Exit Gate

Buffer allocate/copy/fill and lifecycle tests pass without compute shader
execution; each advertised tier passes its ownership, coherence, permissions,
generation, and teardown matrix.
