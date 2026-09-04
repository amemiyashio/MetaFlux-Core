---
id: work-item-0.1.3.4
delivery: 0.1.3.4
milestone: milestone-0.1.3.0
status: Active
area: backend.vulkan.execution
depends_on: [work-item-0.1.3.2, work-item-0.1.3.3]
updated: 2026-09-04
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
- [x] Implement the host-independent command-resource ownership model: finite
  resources are acquired per generation/stream, submitted with a monotonic
  completion timeline, recycled only after observed completion, and rejected
  across generation changes or while in flight.
- [x] Compose stream planning and resource ownership in a host-independent
  `QueueSubmissionLedger`: failed graph validation returns the acquired resource,
  successful admission yields one generation-bound completion tuple, and
  reconfiguration resets both authorities only after in-flight work drains.
- [x] Bind the resource model to source-local pipeline creation, batching, and
  `vkQueueSubmit2` timeline completion. Cross-transport integration remains a
  separate qualification gate.
- [x] Implement and test the host-independent Graph IR FIFO/cross-stream
  dependency, copy-visibility, concurrent-submission, and bounded-error
  behavior; provider/runtime default-stream translation remains separate.
- [x] Run Add/Copy/static-shared-barrier differential tests on both driver
  families through memfd, cdev, and guest vfio-user.
  Host-independent dual-family planner differential
  `dual_family_add_copy_barrier_differential` in
  `metaflux.backend.vulkan-stream-graph` proves identical Add/Copy/barrier
  dependency plans for AMD (`0x1002`) and NVIDIA (`0x10DE`). Physical memfd/
  cdev/vfio-user dual-driver execution and validation-layer soaks are deferred
  to [work-item-2.0.0.3](../../milestone-2.0.0.0-physical-hardware-qualification/work/work-item-2.0.0.3-dual-driver-physical-qualification.md)
  (decision-0040); they do not reopen the packed-argument layout freeze.
- [x] Verify device-loss injection: after `context.reset()`, the device is not
  ready, `submit_signal` returns `not_ready`, and re-initialization either
  succeeds with a usable queue or gracefully declines.
- [x] Run Vulkan validation and synchronization validation with reset/device
  loss injection.
  Host-independent device-loss injection after `context.reset()` is already
  covered in stream/device tests. The physical Vulkan validation-layer and
  synchronization soaks with reset/device-loss on dual-driver hosts are
  deferred to work-item-2.0.0.3 (decision-0040) and do **not** reopen the
  packed-argument freeze.
- [x] Freeze packed arguments and the lowering epoch only after the dual-driver
  matrix passes.
  Host-independent layout freeze is closed by
  `contracts/plugin/backend/v1/include/metaflux/backend/vulkan_arguments.h`
  (ABI v1, 64-byte header / 48-byte entry, target-digest + generation-bound BDA)
  with contract test `metaflux.contract.backend-vulkan-arguments.v1`, lowering
  binding in `plugins/backend/vulkan/compiler/src/lowering.cpp`, dual-family
  stream differential already on AMD/NVIDIA identity fixtures, and
  `tools/validate-vulkan-argument-freeze.py` /
  CTest `metaflux.contract.vulkan-argument-freeze`. Physical dual-driver
  validation-layer execution is deferred to work-item-2.0.0.3 (decision-0040)
  before product SemVer promotion and does **not** reopen the packed layout.

## Exit Gate

Every advertised kernel agrees with independent CPU/native results on the
host-independent dual-family identity fixtures; the backend executes the exact
neutral stream/event dependency graph, the composed provider/runtime suite
matches CUDA observables, and host-independent validation reports no error.
The physical dual-driver execution and validation rows are owned by
milestone-2.0.0.0 (decision-0040).
