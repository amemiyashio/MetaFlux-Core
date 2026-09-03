---
id: work-item-0.1.1.4
delivery: 0.1.1.4
milestone: milestone-0.1.1.0
status: Active
area: transport.qualification
depends_on: [work-item-0.1.1.2, work-item-0.1.1.3]
updated: 2026-09-03
---

# Fault Qualification and Data-Plane v1 Freeze

## Work

- [x] Add a bounded cdev/vfio-user fault matrix for malformed descriptors and
  worker views, unsupported COPY flags, zero-length COPY, completion
  backpressure/FIFO retry, malformed packets, stale exact unmap, and DMA
  address overflow. Keep malformed input recoverable and prevent retired
  mapping reuse.
- [x] Verify the MSI-X notification ledger contract: mask/unmask delivery,
  armed-timeline gating with disarmed suppression, masked coalescing, injection
  failure with bounded retry, lost/stale-generation fencing, and timeline
  overflow handling.
- [x] Fuzz the vfio-user server control boundary: random byte sequences,
  malformed headers, invalid flags, payload mismatches, unknown message types,
  burst floods, and close-mid-session — all outcomes are defined `ServerResult`
  values with no crashes.
- [x] Verify cross-version negotiation matrix: minor=0 (rejected), minor=1
  (accepted), minor=99 (rejected) with correct completion status codes.
- [x] Verify DMA read-only mapping: acquire with READ permission succeeds,
  acquire with WRITE permission on a READ-only region is rejected.
- [ ] Fuzz ioctl, BAR, descriptor, DMA map/unmap, arithmetic, BAR probe/sizing,
  and config-space writable masks.
- [ ] Verify MSI-X delivery end to end under the pinned QEMU/libvfio-user pair:
  real eventfd fd-level injection failure and interrupt-storm soak.
- [ ] Verify DMA overlap/holes/read-only/overflow/stale epoch/in-flight unmap,
  `FOLL_LONGTERM` rejection, quotas, partial-pin unwind, dirty unpin, direction,
  timeout disconnect, and tombstones.
- [ ] Verify same-stream competing producers, independent streams, publication
  owner death, and bounded robust-futex recovery.
- [ ] Inject client, QEMU, server, and daemon death at each ownership boundary;
  prove bounded `LOST` and no stale backing reuse.
- [ ] Run native/compat layout and cross-version negotiation matrices.
- [ ] Freeze base UAPI, device protocol, BAR/extension directory, and capability
  extension rules as v1 from one schema without changing milestone-0.1.0.0 descriptors.
- [ ] Leave lifecycle/admin experimental for milestone-0.1.2.0.

## Exit Gate

Both vertical slices remain green; every public data-plane layout is generated
from one schema; every injected failure converges without stale backing reuse;
every successful unmap proves zero live server/backend references.
