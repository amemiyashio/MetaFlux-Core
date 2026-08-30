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
offline; existing VMAs retain a tombstone until their final VMA close.

`MEMORY_REGISTER` accepts one caller-owned range up to 64 MiB. The C17 client
passes the virtual address in `offset`, the exact byte count in `byte_count`,
`fd=-1`, and `READ`/`WRITE` direction flags. The kernel charges the current
process's memlock quota, pins full pages with `pin_user_pages_fast()` using
`FOLL_LONGTERM` and optional `FOLL_WRITE`, and builds an SG table. A returned
kind-`REGISTERED` memory record is generation-bound; `mf_cdev_memory_close_v0`
issues the matching unregister request, while owner close also revokes the
registration. SG teardown precedes dirty-unpin for device-written pages and
releases the memlock charge. This stage does not expose backend `dma_map_sg`
or in-flight device references; those remain W0112/W0114 qualification work.

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

The C++ worker can register a `metaflux::runtime::lifecycle::Mirror` with the
M0120 coordinator. Quiesce stops ordinary queue consumption, the lifecycle
drain consumes only already-published descriptors, and a committed generation
is the sole value accepted by the worker. Requests observed while the mirror is
`LOST` complete with `MF_SHARED_DEVICE_LOST`; descriptors for a retired
generation complete with `MF_SHARED_STALE_HANDLE`. This mirror changes no ring,
ioctl, mmap, or BAR record.

The worker also exposes an explicit `CdevBackendBinding` for the worker-side
`mf_backend_api_v1` copy call. A bound API must advertise `MF_BACKEND_CAP_COPY`,
provide a table large enough to contain `copy`, and carry nonzero instance,
queue, and payload-memory handles. A payload COPY is translated to one
`mf_backend_copy_v1` record with the same memory handle at checked base-relative
offsets; backend status is mapped to the shared status vocabulary. An invalid
or unsupported bound API returns `MF_SHARED_NOT_SUPPORTED` rather than silently
falling back. With no binding, the fixture keeps its local `memmove` path. The
binding is synchronous at this stage; backend memory import, DMA mapping,
in-flight reference draining, and CPU Add/Copy production wiring remain open.
