# Control and Data Plane Ownership

| Field | Value |
| --- | --- |
| Status | Proposed |
| Source plans | 0002.1 and 0002.2 |

`metafluxd` is the sole registry, policy, generation, and worker-lease authority.
Exactly one generation-bound data-plane worker owns a backend instance and
consumes its queues. The local worker may be embedded in `metafluxd`; the guest
worker is `metaflux-vfio-userd`. This deployment difference does not change the
ownership contract.

For local cdev, `/dev/metafluxctl` brokers privileged worker registration,
exclusive lease binding, queue attachment, drain, revocation, and worker-death
publication. The guest worker receives the equivalent lease over the private
`metafluxd`/`metaflux-vfio-userd` control channel, so guest service does not
require a host kernel module merely for brokering. `/dev/metafluxN` owns
application-side negotiation, mappings, waits, and memory registration. No
control path carries steady-state launch descriptors or tensor payloads.

Once established, the fast path is:

```text
provider or guest mapping -> shared queue -> leased worker -> backend C ABI
```

Kernel components mirror authoritative state and protect references. They may
publish a one-way local `LOST` condition, but only `metafluxd` may authorize and
commit a replacement `ONLINE` generation.

The v0.1 memfd transport has no doorbell primitive. Its cold operations (setup,
registration, teardown, and blocking waits) may syscall freely, and one
active-queue dispatch may perform at most one wake syscall. Whether a leased
worker spins, adapts, or sleeps is a worker-side implementation freedom, never
an application-visible contract. The zero-syscall steady-state obligation
begins with M0002 and applies only to transports that expose a doorbell or
mapped wake primitive: local cdev and guest vfio-user BAR2.

The MetaFlux device protocol is always little-endian. Host-native vfio-user
framing is converted at the `metaflux-vfio-userd` adapter boundary and never
reinterpreted as a MetaFlux record.
