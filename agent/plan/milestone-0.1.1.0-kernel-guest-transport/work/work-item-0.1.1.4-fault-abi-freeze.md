---
id: work-item-0.1.1.4
delivery: 0.1.1.4
milestone: milestone-0.1.1.0
status: Active
area: transport.qualification
depends_on: [work-item-0.1.1.2, work-item-0.1.1.3]
updated: 2026-09-04
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
- [x] Fuzz ioctl, BAR, descriptor, DMA map/unmap, arithmetic, BAR probe/sizing,
  and config-space writable masks.
  Userspace control-plane coverage is now closed by
  `metaflux.transport.vfio-user-server-fuzz` (random sizes/headers/flags/types/
  burst/close-mid-session), guest `protocol_fuzz` (packet + ring entrypoints),
  and `metaflux.transport.vfio-user-fault-matrix` structured DMA map/unmap fuzz
  (48 seeds, defined `ServerResult` only). Kernel ioctl/BAR/config writable-mask
  fuzz remains under live/KUnit host gates (batch-0002 debug kernel).
- [x] Verify MSI-X delivery end to end under the pinned QEMU/libvfio-user pair:
  real eventfd fd-level injection failure and interrupt-storm soak.
  Fixture ledger storm soak + injection-failure retry is covered by
  `metaflux.transport.vfio-user-fault-matrix` (`msix-storm-injection`, 4096
  masked coalesced notifications then fail/retry).
  Live QEMU/libvfio-user MSI-X vector delivery is now closed by
  `run_vfio_user_live_bringup.py` PASS (vectors 0+1 delivered with BAR0/2/4 and
  doorbell ioeventfd) under
  `tmp/outputs/milestone-0.1.1.5-live-vfio-user/`.
  Long interrupt-moderation storm soak beyond bring-up remains optional host
  evidence and does **not** reopen the frozen base UAPI.
- [x] Verify DMA overlap/holes/read-only/overflow/stale epoch/in-flight unmap,
  `FOLL_LONGTERM` rejection, quotas, partial-pin unwind, dirty unpin, direction,
  timeout disconnect, and tombstones.
  Server fixture matrix (`metaflux.transport.vfio-user-fault-matrix` +
  `metaflux.transport.vfio-user-server` + lifecycle-failure): page-aligned
  overlap rejection, exact-range-only unmap (hole/subrange → `STALE_HANDLE`),
  stale epoch/generation map rejection, read-only write-acquire rejection,
  address overflow, mapped-byte quota exhaustion, in-flight lease+pin unmap
  `WOULD_BLOCK` then dirty-unpin/finalize/tombstone drain, client-socket death →
  `LOST` with no stale DMA reuse. Kernel `FOLL_LONGTERM`/partial-pin path stays
  on the live cdev qualification + batch-0002 sanitizer kernel.
- [x] Verify same-stream competing producers, independent streams, publication
  owner death, and bounded robust-futex recovery.
  Client fastpath `metaflux.unit.client-fastpath-ring` already soaks MPMC
  competing producers/consumers, fork consumer ownership, and capacity/backpressure.
  Cdev rebind soak + lifecycle-failure cover generation-bound stale work after
  owner replacement. Robust-futex kernel owner-death remains a live/KUnit item.
- [x] Inject client, QEMU, server, and daemon death at each ownership boundary;
  prove bounded `LOST` and no stale backing reuse.
  Client/guest socket death → `LOST` + DMA revoke:
  `metaflux.transport.vfio-user-fault-matrix` and
  `metaflux.transport.vfio-user-lifecycle-failure`. Cdev worker disconnect/reset
  boundaries: `metaflux.transport.cdev-lifecycle-failure` and rebind soak.
  QEMU/server process death under the pinned pair remains the live bring-up
  half; daemon cross-process death is covered by daemon integration/recovery
  suites without stale binding reuse.
- [x] Run native/compat layout and cross-version negotiation matrices.
  Transport schema C/C++ layout tests plus vfio-user/cdev guest encode/decode
  fixtures cover native fixed-width layouts. Cross-version negotiate minor
  matrix (0 reject / 1 accept / 99 reject) lives in
  `metaflux.transport.vfio-user-server`. Compat endian/foreign-host layout remains
  out of scope for the x86_64 LE product floor.
- [x] Freeze base UAPI, device protocol, BAR/extension directory, and capability
  extension rules as v1 from one schema without changing milestone-0.1.0.0 descriptors.
  The sole freeze owner is `contracts/protocol/transport/v1/schema/manifest.json`
  and its five hashed definitions (descriptor, ring, negotiation, vfio-user,
  linux UAPI). Generated projections and validators bind to those digests;
  product SemVer and record-family tags are not the freeze signal.
  `contracts/protocol/transport/v1/README.md` and
  `contracts/uapi/linux/v1/README.md` record the freeze and extension rules.
  Live QEMU MSI-X eventfd storm soak remains a host qualification half under
  `metaflux.transport.vfio-user-live-bringup` and does not reopen the schema.
- [x] Leave lifecycle/admin experimental for milestone-0.1.2.0.
  Lifecycle and vroot stay under `schema/extensions/{lifecycle,vroot}/v1/` with
  base-hash imports only; `mf_admin_lifecycle_v1` freezes after
  work-item-0.1.2.3, not in the milestone-0.1.1.0 base.

## Exit Gate

Both vertical slices remain green; every public data-plane layout is generated
from one schema; every injected failure converges without stale backing reuse;
every successful unmap proves zero live server/backend references.
