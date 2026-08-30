---
id: W0114
delivery: 0.1.1.4
milestone: M0110
status: Queued
area: transport.qualification
depends_on: [W0112, W0113]
updated: 2026-08-30
---

# Fault Qualification and Data-Plane v1 Freeze

## Work

- [ ] Fuzz ioctl, BAR, descriptor, DMA map/unmap, arithmetic, BAR probe/sizing,
  and config-space writable masks.
- [ ] Verify MSI-X mask/unmask, arm state, coalescing, fd/injection failure, and
  interrupt-storm prevention.
- [ ] Verify DMA overlap/holes/read-only/overflow/stale epoch/in-flight unmap,
  `FOLL_LONGTERM` rejection, quotas, partial-pin unwind, dirty unpin, direction,
  timeout disconnect, and tombstones.
- [ ] Verify same-stream competing producers, independent streams, publication
  owner death, and bounded robust-futex recovery.
- [ ] Inject client, QEMU, server, and daemon death at each ownership boundary;
  prove bounded `LOST` and no stale backing reuse.
- [ ] Run native/compat layout and cross-version negotiation matrices.
- [ ] Freeze base UAPI, device protocol, BAR/extension directory, and capability
  extension rules as v1 from one schema without changing M0100 descriptors.
- [ ] Leave lifecycle/admin experimental for M0120.

## Exit Gate

Both vertical slices remain green; every public data-plane layout is generated
from one schema; every injected failure converges without stale backing reuse;
every successful unmap proves zero live server/backend references.
