---
id: M0004-W02
milestone: M0004
status: Queued
area: backend.vulkan.memory
depends_on: [M0004-W01]
updated: 2026-08-27
---

# Vulkan Device, Queue, and Memory

## Outcome

Bring up one physical Vulkan device per generation-bound backend instance, with a
bounded queue set, truthful memory tiers, suballocation, timeline synchronization,
and M0003 device-loss hooks before compute shader execution.

A logical context fixes CPU or Vulkan, and fixes its transport, before module
loading/allocation and the first visible success. Supported profiles are M0001
memfd, M0002 cdev, and guest vfio-user only when negotiated capabilities satisfy
the memory contract. An established context never changes backend or transport
after a fault.

Memory tiers are advertised independently:

1. OPAQUE_FD or DMA-BUF external buffers with compatible external semaphores,
   only when the exact device proves import/export support.
2. External host memory only when alignment, pinning, coherency, and driver
   limits permit it.
3. Explicit host-visible staging plus a device-local copy.

Tier 3 alone is required for M0004 completion. A memfd, host pointer, or guest RAM
range is never mislabeled DMA-BUF or zero-copy. The guest bridge accepts only a
validated M0002 generation-bound handle/range/permission set, selects direct
import only for an exactly compatible handle/sync capability, and otherwise
reports staging. Unregister waits for both transport and Vulkan references. No
descriptor or transport UAPI field changes.

The extension records handle type, ownership transfer, size, alignment, memory
type, permissions, generation, dedicated-only status, compatible handle types,
and synchronization capability; Vulkan handles remain private. Large allocations
use backend-owned blocks/suballocation and steady-state launch allocates no Vulkan
memory or host heap object.

## Work

- [ ] Implement instance/device/queue discovery and actual enabled feature,
  property, limit, and capability capture behind `mf_backend_api_v1`.
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
