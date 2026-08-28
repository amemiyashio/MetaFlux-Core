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

## Stream/event mapping

- One logical context fixes CPU or Vulkan and its transport before visible
  resource success.
- Preserve stream FIFO. Encode cross-stream event waits as explicit timeline
  dependencies; carry legacy-default/PTDS rules from the CUDA provider contract.
- Use Synchronization2 stage/access masks for copy, host, shader, transfer, and
  external visibility. A semaphore orders only the declared dependencies and
  does not replace non-coherent flush/invalidate.
- Complete backend work before publishing the M0002 completion timeline with the
  required release relationship.
- Queue batching may combine submissions only when all observable dependencies,
  errors, and completion points remain equivalent.

Unregister and device loss reject new use, remove address lookup, wait or isolate
in-flight references, then destroy Vulkan and external resources in reverse
ownership order.

Primary sources:

- [Vulkan memory](https://docs.vulkan.org/spec/latest/chapters/memory.html)
- [Vulkan synchronization](https://docs.vulkan.org/spec/latest/chapters/synchronization.html)
