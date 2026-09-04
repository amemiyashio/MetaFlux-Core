# Transport Protocol v1

The milestone-0.1.1.0 transport envelope is authored by
[`schema/manifest.json`](schema/manifest.json) and its hashed definitions. The
same little-endian records are carried by memfd, cdev, vfio-user, and later
network transports without leaking provider or backend types. Generated C/C++
projections are emitted into the build tree; this directory contains no second
hand-maintained layout. work-item-0.1.1.4 freezes this directory as the `v1`
data-plane base: the five hashed base definitions (device descriptor, shared
ring, negotiation, vfio-user profile, linux UAPI) are the sole public wire and
UAPI layouts. The freeze is content-addressed by those digests; schema record
version tags such as `0.1`/`v0` name the record family, not an unfrozen
candidate. Product SemVer remains independent (`VERSION`).

Capability extension rules are permanent for the base: new transports or
backend-facing wire types add records only under
`schema/extensions/<name>/` with a manifest that imports this base by content
hash. They never rewrite the five base digests or the milestone-0.1.0.0
descriptor layout. Base transport feature bits live in `negotiation.json`;
UAPI feature bits live in `uapi.json`; backend capability padding stays
reserved-zero until an extension claims it through its own hashed document.

The vfio-user fixture uses `MF_VFIO_USER_MESSAGE_NEGOTIATE_V0` as the
transport-level capability exchange. A request carries only the supported
major/minor candidate and required or optional feature bits; identity,
generation, queue, DMA, and limit fields are zero. The server returns the
selected minor and feature set together with its registry identity and
published limits. Unsupported versions or required features use the common
`mf_transport_completion_v0` status reply, while a successful negotiation
returns the canonical `mf_transport_negotiate_v0` record. No file descriptor
is accepted on this control message.

milestone-0.1.2.0 lifecycle and experimental vroot assets live only under
`schema/extensions/lifecycle/v1/` and `schema/extensions/vroot/v1/`. Their
manifests import this base by content hash; they are not members of the v1
base closure and remain experimental until work-item-0.1.2.3 freezes
`mf_admin_lifecycle_v1`. The lifecycle model owns generation-candidate,
epoch-retirement, tombstone, and provider-view rules without changing the
milestone-0.1.1.0 root.
