# Reset and Failure

## M0110 static contract

The M0110 `VFIO_USER_DEVICE_GET_INFO` reply leaves `VFIO_DEVICE_FLAGS_RESET`
clear, and migration is also unadvertised. A conforming client therefore does
not send `VFIO_USER_DEVICE_RESET` as a supported operation. If that command is
received defensively, the server never replies success: it returns the exact
unsupported result qualified for the pinned pair or closes on terminal failure.
Guest/QEMU reset observation, server disconnect, or fatal transport failure
stops admission, fences queues, revokes the worker lease, invalidates DMA
lookups, drains or isolates references, publishes `LOST`, and leaves the static
instance terminal. It never publishes a replacement `ONLINE` generation.

## Failure sequence

1. Atomically reject new control requests, mappings, and submissions.
2. Mark the connection/device lost and snapshot the failure cause.
3. Revoke the exclusive worker lease and prevent old completions from publishing.
4. Remove DMA records from lookup and drain or tombstone all references.
5. Signal the guest's fatal/admin path where transport still permits it.
6. Close protocol resources and expose deterministic errors to guest userspace.

Repeated reset/disconnect observation is idempotent. vfio-user message IDs are
not idempotence keys. A late socket reply, ioeventfd write,
MSI-X signal, backend callback, or QEMU reconnect cannot revive the instance.

## M0120 boundary

Coordinated reset/recovery belongs to the lifecycle authority. It reserves one
never-reused generation candidate before quiesce and stages every replacement
owner while the old generation remains current. Only the authority may atomically
retire old, advance epoch, and install the candidate `ONLINE`; the vfio-user
server mirrors committed state and never allocates or publishes generations
locally.

M0120 sets `VFIO_DEVICE_FLAGS_RESET` only when the coordinated reset handler is
registered and ready before `VFIO_USER_DEVICE_GET_INFO`. Once advertised, each
accepted nonduplicate reset command reserves one generation candidate, replies
success only after the replacement reaches committed `ONLINE`, and otherwise
mirrors the authority's exact terminal result. A pre-transaction failure returns
the pinned error reply while the old generation may remain `ONLINE` with epoch
unchanged; it does not close merely to manufacture `LOST`. A fault after the
atomic replacement transaction marks the committed candidate `LOST` before the
pinned error/close result. Adapter-local state or reset success is forbidden.

Inject failure before/after feature negotiation, region setup, DMA publication,
worker lease, queue activation, completion, unmap drain, and reset commit. Assert
one terminal state, no false success, no leaked fd/map/ref, and no stale work in a
replacement generation.
