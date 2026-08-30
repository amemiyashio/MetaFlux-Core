---
id: W0134
delivery: 0.1.3.4
milestone: M0130
status: Active
area: backend.vulkan.execution
depends_on: [W0132, W0133]
updated: 2026-08-31
---

# Execution, Streams, and Events

## Outcome

Execute the advertised corpus through recycled command resources,
`vkQueueSubmit2`, and timeline semaphores while preserving ecosystem-neutral Graph
IR dependency/event behavior on local and guest transports.

One MetaFlux stream remains FIFO. Event record publishes one backend timeline
value; cross-stream wait creates an explicit dependency; copy/launch visibility
uses Synchronization2 stage/access masks. The CUDA provider/runtime translates
legacy default-stream and per-thread-default behavior into explicit Graph IR edges
before this backend boundary; Vulkan sees no CUDA stream mode. Batching is
permitted only when observable dependencies remain identical.

The leased worker owns the long-lived `VkInstance`/`VkDevice`, queue set, command
pools/buffers, any descriptors, and packed argument blocks. The local worker may
be embedded in `metafluxd`; the guest worker is `metaflux-vfio-userd`. Resources
are recycled without control RPC on an active queue. Unsupported kernels return
a capability diagnostic and never switch an established context to CPU.

## Work

- [x] Add the host-independent stream/dependency planning layer. It preserves
  per-stream FIFO, adds the prior same-stream timeline edge, requires explicit
  cross-stream waits, validates copy/launch stage/access masks, and rejects
  stale or future dependencies before `vkQueueSubmit2` integration.
- [ ] Implement pipeline creation, command recycling, batching, and
  `vkQueueSubmit2` timeline completion into the M0110 timeline.
- [ ] Implement Graph IR FIFO/cross-stream dependencies, copy visibility,
  concurrent submission, and bounded error propagation; verify composed
  provider/runtime default-stream translation separately.
- [ ] Run Add/Copy/static-shared-barrier differential tests on both driver
  families through memfd, cdev, and guest vfio-user.
- [ ] Run Vulkan validation and synchronization validation with reset/device
  loss injection.
- [ ] Freeze packed arguments and the lowering epoch only after the dual-driver
  matrix passes.

## Exit Gate

Every advertised kernel agrees with independent CPU/native results on both driver
families; the backend executes the exact neutral stream/event dependency graph,
the composed provider/runtime suite matches CUDA observables, and validation
reports no error.
