# Character-Device Transport

Local transport over `/dev/metafluxN` mappings and the `/dev/metafluxctl` worker
broker. Setup, registration, teardown, and blocking waits are cold operations;
active queues use shared descriptors and timelines directly.

The userspace halves are split per the transport halves convention (D0010). The
kernel counterpart under `kernel/core/` maps one submission and one completion
ring back-to-back from the generated UAPI queue record. It advertises queue mmap,
eventfd association, and the worker-broker bit. `MEMORY_ALLOC` provisions one
generation-bound, page-aligned driver payload arena per data-file owner; the
returned `offset` is the `MF_UAPI_MMAP_PAYLOAD_V0` mapping offset and `fd` remains
`-1` because the arena is driver-owned. Closing the owner marks the arena
offline; existing VMAs retain a tombstone until their final VMA close. Import and
long-term user-page registration are deliberately separate `MEMORY_REGISTER`
work and still return `-EOPNOTSUPP`.

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
