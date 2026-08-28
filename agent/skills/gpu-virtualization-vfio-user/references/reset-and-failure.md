# Reset and Failure

## M0002 static contract

`VFIO_USER_DEVICE_RESET`, guest/QEMU reset, server disconnect, or fatal transport
failure stops admission, fences queues, revokes the worker lease, invalidates DMA
lookups, drains/isolate references, publishes `LOST`, and leaves the static
instance terminal. It never publishes a replacement `ONLINE` generation.

## Failure sequence

1. Atomically reject new control requests, mappings, and submissions.
2. Mark the connection/device lost and snapshot the failure cause.
3. Revoke the exclusive worker lease and prevent old completions from publishing.
4. Remove DMA records from lookup and drain or tombstone all references.
5. Signal the guest's fatal/admin path where transport still permits it.
6. Close protocol resources and expose deterministic errors to guest userspace.

Duplicate reset/disconnect is idempotent. A late socket reply, ioeventfd write,
MSI-X signal, backend callback, or QEMU reconnect cannot revive the instance.

## M0003 boundary

Coordinated reset/recovery belongs to the lifecycle authority. It may stage a
new never-reused generation, but only after all transport owners have retired the
old one and only the authority may commit registry visibility. The vfio-user
server mirrors committed state; it does not allocate generations locally.

Inject failure before/after feature negotiation, region setup, DMA publication,
worker lease, queue activation, completion, unmap drain, and reset commit. Assert
one terminal state, no false success, no leaked fd/map/ref, and no stale work in a
replacement generation.
