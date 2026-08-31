# Character-Device Transport

Local transport over `/dev/metafluxN` mappings and the `/dev/metafluxctl` worker
broker. Setup, registration, teardown, and blocking waits are cold operations;
active queues use shared descriptors and timelines directly.

The userspace halves are split per the transport halves convention (D0010). The
kernel counterpart under `kernel/core/` maps one submission and one completion
ring back-to-back from the generated UAPI queue record. It advertises queue mmap,
eventfd association, registered-memory, and the worker-broker bits.
`MEMORY_ALLOC` provisions one
generation-bound, page-aligned driver payload arena per data-file owner; the
returned `offset` is the `MF_UAPI_MMAP_PAYLOAD_V0` mapping offset and `fd` remains
`-1` because the arena is driver-owned. Closing the owner marks the arena
offline; existing VMAs and an in-flight allocation retain a tombstone until
their final references close.

`MEMORY_REGISTER` accepts up to four caller-owned ranges, each up to 64 MiB,
with a 256 MiB aggregate quota per module generation. The C17 client passes the
virtual address in `offset`, the exact byte count in `byte_count`, `fd=-1`, and
`READ`/`WRITE` direction flags. The kernel charges the current process's
memlock quota, pins full pages with `pin_user_pages_fast()` using
`FOLL_LONGTERM` and optional `FOLL_WRITE`, and builds an independent SG table
per slot. Each returned kind-`REGISTERED` memory record has a unique
generation-bound handle (`3` through `6`); `mf_cdev_memory_close_v0` issues the
matching unregister request, while owner close revokes every slot owned by the
file. A fifth slot or an aggregate quota overflow returns `-EBUSY`. The kernel
maps each SG table with one direction derived from the READ/WRITE flags through
the data cdev's DMA device, and every unregister, owner-close, module-exit, and
map-failure path unmaps before SG teardown, dirty-unpin for device-written
pages, and memlock release. This stage does not expose backend memory import or
in-flight worker references, and it does not qualify a physical GPU DMA master;
those remain W0112/W0114 work.

Closing the queue owner or worker lease transitions the current generation to an
offline tombstone before waking waiters. Existing queue VMAs remain mapped until
their final close, but new negotiation, queue creation, allocation, and worker
lease attempts cannot reuse that generation; daemon-controlled replacement is a
separate lifecycle step. The kernel queue reference graph keeps root, owner,
lease, VMA, and active wait/poll references distinct, so owner close cannot free
backing still observed by an in-flight operation.

Eventfds are caller-owned descriptors. A queue or worker lease may attach one
complete pair (or no eventfds) for a generation; the kernel retains `eventfd_ctx` references and returns
the supplied descriptor numbers unchanged. A second owner is rejected with
`-EBUSY`, and every owner close releases the references before the file object is
freed. Eventfd attachment is a notification primitive only; descriptors and
payload remain shared-memory fast-path data.

`CdevWorkerSession` is the C++ worker-side activation wrapper for this lease.
It opens `/dev/metafluxctl`, submits the generation- and `registry_view_id`-bound
`MF_UAPI_IOCTL_WORKER_LEASE`, validates the returned identity, lease, exact
paired-ring size, queue IDs, and ring metadata, then maps the queue through the
leased control fd. `close()` unmaps the rings before closing the fd, so the
kernel release revokes the lease. The wrapper deliberately does not map the
payload arena: that data-plane mapping and the daemon object-table ranges stay
owned by their respective clients. `ENOENT`/`ENODEV`/`ENOTTY` and unsupported
ioctls map to `MF_SHARED_NOT_SUPPORTED`; stale, busy, permission, resource,
and malformed responses remain distinct statuses.

The C++ worker can register a `metaflux::runtime::lifecycle::Mirror` with the
M0120 coordinator. Quiesce stops ordinary queue consumption, the lifecycle
drain consumes only already-published descriptors, and a committed generation
is the sole value accepted by the worker. Requests observed while the mirror is
`LOST` complete with `MF_SHARED_DEVICE_LOST`; descriptors for a retired
generation complete with `MF_SHARED_STALE_HANDLE`. When a pending asynchronous
backend operation observes transport loss, the mirror asks the generation-bound
backend binding to cancel its queue only if `MF_BACKEND_CAP_CANCELLATION` and
`cancel_queue` are both advertised. A successful cancellation converts the
pending operation to `MF_SHARED_DEVICE_LOST` through the normal completion path;
lease and memory references remain held through completion-ring backpressure.
Backends without the capability retain their existing event/lease contract until
the backend reports completion. Reset and Remove quiesce also reject when a
pending operation cannot be cancelled; when cancellation is supported, drain
first completes the pending operation before inspecting the submission ring.
This mirror changes no ring, ioctl, mmap, or BAR record.

The worker also exposes an explicit `CdevBackendBinding` for worker-side
`mf_backend_api_v1` calls. A COPY binding must advertise
`MF_BACKEND_CAP_COPY`, provide a table large enough to contain `copy`, and carry
nonzero instance, queue, and payload-memory handles. It must also provide a
synchronous backend-operation lease pair. The worker acquires that lease
before resolving or dispatching a bound request and releases it after the
synchronous backend call returns; lease rejection is surfaced in the completion
record and never falls back to the local path. A payload COPY is translated to
one `mf_backend_copy_v1` record with checked base-relative offsets; backend
status is mapped to the shared status vocabulary. A binding may additionally
provide a complete `CdevBackendMemoryReference` for its payload-memory handle;
the worker retains that handle before dispatch and holds it through asynchronous
completion, cancellation, and completion-ring backpressure, then releases it
after publishing the terminal completion. Partial reference callbacks are
rejected as an invalid binding. With no binding, the fixture keeps its local
`memmove` path.

The worker also accepts `MF_RING_COPY_FLAG_REGION_ARGUMENT_BLOCK_V1` when the
binding supplies a `CdevCopyResolver`. The resolver owns daemon/object-table
semantics and maps the descriptor's region argument-block references to
independent destination and source backend memory handles, offsets, and a byte
count. Each returned handle carries a resolver-owned retain/release pair. The
worker validates both handles and callbacks, retains both references before
calling `mf_backend_api_v1.copy`, and releases them after synchronous completion
or after an asynchronous event is observed and its completion is published.
References remain held while the completion ring applies backpressure. Direct-
host COPY flags remain unsupported on this worker. This is the backend
import/reference seam; a resolver may use `CdevBackendMemoryImporter` after its
object-table generation and range checks to turn a registered caller-owned
range into a backend handle. The importer returns the complete reference pair,
and the worker-side reference callback remains the owner boundary for that
imported handle. Physical-device qualification remains open.

The region descriptor uses the argument-block object ID as `target_id`, its
generation in `arguments[0]`, and zero in `arguments[1..3]`; the resolver must
reject stale object-table generations. The cdev client exposes this encoding as
`mf_cdev_copy_region_descriptor_v0` and
`mf_cdev_submit_copy_region_v0`.

For the bounded LAUNCH subset, the binding additionally supplies a
`CdevLaunchResolver`. The resolver owns daemon/object-table semantics and maps
the cdev descriptor's generation, module ID/generation, and argument-block
ID/generation to a `CdevLaunchResolution`. It must place the already encoded
backend argument bytes in the worker payload arena; the worker checks the
payload range, primary-entry ID, 2D dimensions (`z == 1`), and reserved fields
before constructing `mf_backend_launch_v1` and calling `submit`. The same
operation lease covers resolver access and the submit call. The C client
helpers `mf_cdev_launch_descriptor_v0` and `mf_cdev_submit_launch_v0` encode
this primary-entry layout. Resolver failures and backend statuses remain
visible in the completion record, with no silent fallback. Nonzero completion
events and daemon generation replacement are still outside this synchronous
descriptor stage; the worker's asynchronous event contract extends the lease
until observed completion and snapshots the exact backend binding so a later
replacement cannot query or release the old operation through the new binding.
Production backend memory import remains open; the host-independent reference
lifetime contract, including direct payload-memory retain/release, and
capability-gated pending-operation cancellation are implemented by the worker
binding and lifecycle callbacks.

The W0114 bounded fault matrix now treats unknown COPY flags as malformed,
known direct-host flags as unsupported on this worker, and zero-length COPY as
invalid. A full completion ring returns backpressure without consuming the
submission; once a slot is released the request completes in FIFO order.
Malformed worker views, stale generations, and offline workers remain bounded
by a completion status. Kernel ioctl fuzzing, live DMA references, owner-death
injection, and native/compat qualification remain open exit-gate work.
