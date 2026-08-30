# Linux UAPI v1

The candidate ABI `0.x` cdev and worker-broker records are defined in
[`schema/uapi.json`](schema/uapi.json), imported by the M0110 base manifest, and
generated with the transport projection. Structures use fixed-width types,
explicit padding, sized inputs, and reserved-zero validation. The public
include wrapper is [`include/metaflux/uapi/transport.h`](include/metaflux/uapi/transport.h);
the generated header is installed alongside it. Compatibility and ioctl
qualification remain W0112/W0114 work before a `v1` freeze.

`MF_UAPI_IOCTL_MEMORY_REGISTER` uses the existing fixed-width memory record for
one long-term user range. The input `offset` is the user virtual address and
`byte_count` is the exact byte range; `handle` must be zero, `fd` must be `-1`,
and `flags` must contain `MF_UAPI_MEMORY_REGISTER_FLAG_READ_V0` (device reads
the range) and/or `MF_UAPI_MEMORY_REGISTER_FLAG_WRITE_V0` (device writes the
range). The kernel pins full pages with `FOLL_PIN | FOLL_LONGTERM` and adds
`FOLL_WRITE` for a device-write range, then builds an SG table. The returned
handle and current generation identify the registration. The current projection
permits one range per data-file owner, bounded to 64 MiB. An unregister uses the
returned handle and generation with all range fields zero; owner close performs
the same revocation implicitly. Both paths remove the live lookup before SG
teardown, dirty-unpin device-written pages, and memlock-charge release. No
backend DMA mapping or in-flight device-reference ABI is claimed until the
lifecycle and device-reference gates are qualified.
