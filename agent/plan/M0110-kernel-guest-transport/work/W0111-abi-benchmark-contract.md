---
id: W0111
delivery: 0.1.1.1
milestone: M0110
status: Queued
area: transport.contracts
depends_on: [M0100]
updated: 2026-08-30
---

# ABI Fixtures and Benchmark Contract

## Outcome

Define the candidate transport envelope and reproducible measurement contract
used unchanged by the local cdev and static guest vertical slices.

## Contract Ownership

| Path | Ownership |
| --- | --- |
| `contracts/plugin/backend/v1/` | M0100 backend C function table |
| `contracts/protocol/client/v1/` | M0100 provider/client negotiation |
| `contracts/protocol/device/v1/` | 64-byte command/completion/timeline bytes |
| `contracts/protocol/transport/v1/` | Transport negotiation and extensions |
| `contracts/shared/device/v1/` | mmap-visible registry/state/metrics |
| `contracts/uapi/linux/v1/` | Linux cdev, mmap, and worker-broker UAPI |

Directory ownership does not imply stability. Negotiation, ioctl, BAR, and DMA
start at ABI `0.x`; the inherited M0100 descriptor is already frozen. v1 freezes
only after both Add/Copy slices, native/compat layouts, teardown/fault tests, and
reproducible performance measurements pass.

Negotiation selects the highest common minor then intersects optional features.
Required extensions reject when absent; every extension has type and byte size.
The base record includes magic, major/minor/size, UUID/generation, feature and
required-extension bits, profile, descriptor versions, queue/ring limits, DMA
width/alignment/coherency, MSI-X/notification modes, and registered-region/byte/
in-flight limits.

Public records are little-endian, fixed-width, sized, explicitly padded/aligned,
and zero reserved bytes. They contain no pointer, `size_t`, native enum, language
atomic, or kernel object. Native, compat, C, C++, and kernel fixtures assert every
size, alignment, offset, and reserved byte.

vfio-user host-native framing is adapted only at the server boundary. MetaFlux
BAR, DMA, and descriptor bytes always use explicit little-endian accessors and
golden-byte/forced-byte-swap tests.

## Linux UAPI Boundary

`/dev/metafluxN` provides application-side negotiation/capabilities, context and
session lifecycle, queue mmap/arm/wait, memory allocate/register/import/export,
eventfd association, and blocking wait.

For local cdev, `/dev/metafluxctl` provides privileged broker negotiation, worker
capability registration, one authority-issued UUID/generation lease binding,
queue arena and kick/completion eventfd attachment, start/drain/revoke/detach/
unregister, and worker-death/fatal reporting. It validates credentials, daemon
incarnation, generation, capabilities, and exclusivity. It cannot create registry
identity, override policy, publish a replacement generation, carry descriptors,
compile, or carry tensors.

Unknown ioctls return `-ENOTTY`; address/length arithmetic checks overflow. No
ioctl synchronously starts a daemon or compiler RPC. Lifecycle/admin stays `0.x`
until M0120 qualification.

## Work

- [ ] Define ABI 0.x capability, ioctl, BAR, and DMA layouts while importing the
  M0100 descriptor unchanged.
- [ ] Define local/guest buffer lifetimes and worker register/lease/attach/drain/
  revoke/death/credential rules.
- [ ] Pin Linux, QEMU, libvfio-user, guest memory, and image inputs.
- [ ] Generate all language/layout assertions and byte fixtures.
- [ ] Define timestamps, queue state, poll/block mode, affinity, NUMA, warm-up,
  samples, and direct baselines.

## Exit Gate

Clean builds produce byte-identical fixtures and both local and guest harnesses
negotiate ABI 0.x using one descriptor schema and one measurement definition.
