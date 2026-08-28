---
id: M0004-W04
milestone: M0004
status: Queued
area: backend.vulkan.execution
depends_on: [M0004-W02, M0004-W03]
updated: 2026-08-27
---

# Execution, Streams, and Events

## Outcome

Execute the advertised corpus through recycled command resources,
`vkQueueSubmit2`, and timeline semaphores while preserving observable CUDA
stream/event behavior on local and guest transports.

One MetaFlux stream remains FIFO. Event record publishes one backend timeline
value; cross-stream wait creates an explicit dependency; copy/launch visibility
uses Synchronization2 stage/access masks. Legacy default-stream and per-thread
default-stream behavior follow the CUDA provider contract rather than Vulkan
queue behavior. Batching is permitted only when observable dependencies remain
identical.

The leased worker owns the long-lived `VkInstance`/`VkDevice`, queue set, command
pools/buffers, any descriptors, and packed argument blocks. The local worker may
be embedded in `metafluxd`; the guest worker is `metaflux-vfio-userd`. Resources
are recycled without control RPC on an active queue. Unsupported kernels return
a capability diagnostic and never switch an established context to CPU.

## Work

- [ ] Implement pipeline creation, command recycling, batching, and
  `vkQueueSubmit2` timeline completion into the M0002 timeline.
- [ ] Implement FIFO, cross-stream events, PTDS/default stream, copy visibility,
  concurrent submission, and bounded error propagation.
- [ ] Run Add/Copy/static-shared-barrier differential tests on both driver
  families through memfd, cdev, and guest vfio-user.
- [ ] Run Vulkan validation and synchronization validation with reset/device
  loss injection.
- [ ] Freeze packed arguments and the lowering epoch only after the dual-driver
  matrix passes.

## Exit Gate

Every advertised kernel agrees with independent CPU/native results on both driver
families; stream/event/default-stream behavior matches CUDA observables; validation
reports no error.
