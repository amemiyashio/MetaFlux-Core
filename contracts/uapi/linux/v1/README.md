# Linux UAPI v1

The candidate ABI `0.x` cdev and worker-broker records are defined in
[`schema/uapi.json`](schema/uapi.json), imported by the milestone-0.1.1.0 base manifest, and
generated with the transport projection. Structures use fixed-width types,
explicit padding, sized inputs, and reserved-zero validation. The public
include wrapper is [`include/metaflux/uapi/transport.h`](include/metaflux/uapi/transport.h);
the generated header is installed alongside it. Compatibility and ioctl
qualification remain work-item-0.1.1.2/work-item-0.1.1.4 work before a `v1` freeze.

`MF_UAPI_IOCTL_MEMORY_REGISTER` uses the existing fixed-width memory record for
one long-term user range. The input `offset` is the user virtual address and
`byte_count` is the exact byte range; `handle` must be zero, `fd` must be `-1`,
and `flags` must contain `MF_UAPI_MEMORY_REGISTER_FLAG_READ_V0` (device reads
the range) and/or `MF_UAPI_MEMORY_REGISTER_FLAG_WRITE_V0` (device writes the
range). The kernel pins full pages with `FOLL_PIN | FOLL_LONGTERM` and adds
`FOLL_WRITE` for a device-write range, then builds and direction-maps an SG
table through the transport DMA device. The returned handle and current
generation identify the registration. The current projection permits one range
per data-file owner, bounded to 64 MiB. An unregister uses the returned handle
and generation with all range fields zero; owner close performs the same
revocation implicitly. Both paths remove the live lookup before DMA unmap, SG
teardown, dirty-unpin of device-written pages, and memlock-charge release. A
map failure is unpublished and unwound in the same order. The fixed UAPI still
does not expose backend memory import or in-flight device references; physical
GPU DMA and lifecycle qualification remain outside this stage.

`MF_UAPI_IOCTL_MEMORY_QUERY` uses the same memory record on a negotiated worker
lease fd. The input keeps `handle`, `generation`, `byte_count`, `alignment`,
and `offset` zero, uses `fd=-1`, and requires reserved-zero bytes. The kernel
returns the currently online data-owner payload handle, generation, exact
page-aligned byte count, `MF_UAPI_MMAP_PAYLOAD_V0` offset, and `fd=-1`.
The query is generation-bound and returns `-EAGAIN` while no payload is online;
it never transfers ownership or exposes the data fd. A subsequent mmap still
rechecks the online state and exact length under the cdev lock.
