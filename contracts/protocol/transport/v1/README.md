# Transport Protocol v1

The candidate milestone-0.1.1.0 transport envelope is authored by
[`schema/manifest.json`](schema/manifest.json) and its hashed definitions. The
same little-endian records are carried by memfd, cdev, vfio-user, and later
network transports without leaking provider or backend types. Generated C/C++
projections are emitted into the build tree; this directory contains no second
hand-maintained layout. The candidate remains ABI `0.x` until work-item-0.1.1.4 evidence
freezes the extension namespace as `v1`.

The vfio-user fixture uses `MF_VFIO_USER_MESSAGE_NEGOTIATE_V0` as the
transport-level capability exchange. A request carries only the supported
major/minor candidate and required or optional feature bits; identity,
generation, queue, DMA, and limit fields are zero. The server returns the
selected minor and feature set together with its registry identity and
published limits. Unsupported versions or required features use the common
`mf_transport_completion_v0` status reply, while a successful negotiation
returns the canonical `mf_transport_negotiate_v0` record. No file descriptor
is accepted on this control message.

milestone-0.1.2.0 lifecycle semantics live in the one-way extension at
`schema/extensions/lifecycle/v1/`. Its manifest imports this base by content
hash and the lifecycle model owns generation-candidate, epoch-retirement,
tombstone, and provider-view rules without changing the milestone-0.1.1.0 root.
