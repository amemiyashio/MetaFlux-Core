# Memory and Synchronization

## Memory tiers

Advertise three independent tiers only when their queried external-handle and
synchronization capabilities pass:

1. compatible OPAQUE_FD or DMA-BUF plus external synchronization;
2. capability-proven external host memory;
3. explicit host-visible staging and device-local copy.

Only staging is required for baseline completion. A memfd, arbitrary guest RAM,
or host pointer is not automatically DMA-BUF or zero-copy. For every allocation
record memory type/heap, size/alignment, mapped range, coherent/cached flags,
flush/invalidate obligations, export/import ownership, BDA, permissions,
generation, in-flight use, and destruction order.

## External handle contract

Reuse the exact matrix for every advertised memory and semaphore handle type;
add or refresh a row when that handle or device contract changes. Record:

- queried import/export, compatible-handle, dedicated-only, timeline, and
  temporary-import capabilities for the exact physical device;
- whether a successful Vulkan import consumes the fd and which side closes it
  on every failure before ownership transfer;
- whether the imported semaphore payload is permanent or temporary; `SYNC_FD`
  is a temporary binary payload and is not a timeline-semaphore interchange;
- whether export returns a reference to the same underlying payload or transfers
  a temporary synchronization payload, as defined for that handle type;
- the producer/consumer process, generation, permissions, and destruction order.

Do not treat OPAQUE_FD, DMA-BUF, and SYNC_FD as interchangeable because they all
use integer file descriptors. Negotiate one exact handle type and verify it
against `vkGetPhysicalDeviceExternalBufferProperties` or
`vkGetPhysicalDeviceExternalSemaphoreProperties` before import.

For resources shared with another Vulkan or external queue owner, pair the
external synchronization primitive with explicit release/acquire ownership
transfers. Use `VK_QUEUE_FAMILY_EXTERNAL` for Vulkan-defined external ownership
and `VK_QUEUE_FAMILY_FOREIGN_EXT` only when the negotiated producer is foreign
under the extension's rules. Record the old/new queue-family indices, stage and
access masks, image layout where applicable, and the process that owns the
resource after each barrier. A semaphore signal alone does not perform a missing
queue-family ownership transfer.

## Stream/event mapping

- One logical context fixes CPU or Vulkan and its transport before visible
  resource success.
- Preserve stream FIFO. Encode cross-stream event waits as explicit timeline
  dependencies. Consume only ecosystem-neutral Graph IR edges; CUDA
  legacy-default/PTDS translation is completed by the provider/runtime before this
  boundary.
- Use Synchronization2 stage/access masks for copy, host, shader, transfer, and
  external visibility. A semaphore orders only the declared dependencies and
  does not replace queue-family transfer or non-coherent flush/invalidate.
- Complete backend work before publishing the milestone-0.1.1.0 completion timeline with the
  required release relationship.
- Queue batching may combine submissions only when all observable dependencies,
  errors, and completion points remain equivalent.

Unregister and device loss reject new use, remove address lookup, wait or isolate
in-flight references, then destroy Vulkan and external resources in reverse
ownership order.

A generation-bound context keeps its own generation across reset and
resubmission; a foreign generation returns `stale_generation` rather than
re-arming that context. Reset releases only its own generation-bound resources.
Preserve context isolation, registration/use/unregister races, failed-import fd
ownership, timeline waits, non-coherent visibility and reset/loss negatives for
the changed resource path. Cross-transport completion tests cover memfd, cdev and
guest vfio-user where the milestone requires them; an arithmetic-only change
does not require a new external-handle matrix.

The neutral contracts live in
[`vulkan_arguments.h`](../../../../contracts/plugin/backend/v1/include/metaflux/backend/vulkan_arguments.h)
and [`vulkan_memory.h`](../../../../contracts/plugin/backend/v1/include/metaflux/backend/vulkan_memory.h).
Preserve their versioned packed layout, target digest, generation and handle
ownership without leaking Vulkan types. CUDA stream/default/PTDS semantics stay
in provider/runtime translation; compose
[$cuda-driver-abi-compatibility](../../cuda-driver-abi-compatibility/SKILL.md) skill
and [$runtime-contracts-registry](../../runtime-contracts-registry/SKILL.md) skill
when changing that translation. Neutral ABI fields remain owned by the latter;
external transport import mechanics belong to the matching transport skill.

Primary sources:

- [Vulkan memory](https://docs.vulkan.org/spec/latest/chapters/memory.html)
- [Vulkan synchronization](https://docs.vulkan.org/spec/latest/chapters/synchronization.html)
