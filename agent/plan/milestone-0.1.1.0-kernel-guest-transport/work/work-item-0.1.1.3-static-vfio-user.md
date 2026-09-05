---
id: work-item-0.1.1.3
delivery: 0.1.1.3
milestone: milestone-0.1.1.0
status: Complete
area: transport.vfio-user
depends_on: [work-item-0.1.1.1]
updated: 2026-09-06
---

# Static vfio-user Guest Vertical Slice

## Outcome

Execute CPU Add/Copy through one static QEMU/KVM `vfio-user-pci` function using
DMA-mapped guest RAM, ioeventfd, and MSI-X without steady-state socket traffic.

```text
guest provider -> /dev/metafluxN -> guest metaflux_pci.ko
               -> DMA rings/buffers -> BAR2 doorbell
               -> host metaflux-vfio-userd -> mf_backend_api_v1 -> CPU
host metafluxd -> private generation lease -> metaflux-vfio-userd
```

The socket carries negotiation, DMA map/unmap, device state, and recovery only.
The high-speed profile requires shared file-backed guest RAM such as
`memory-backend-memfd,share=on`, or an equivalent file backend configured with
`share=on`, plus mmap-capable DMA-map fds; socket-mediated region access fails
negotiation.

`metaflux-vfio-userd` and the guest consume generated projections from the sole
root `contracts/protocol/transport/v1/schema/manifest.json`; vfio-user's pinned
host-native framing remains a server-boundary adapter. Message IDs belong to the
sender, are echoed in replies, and may be reused concurrently; receivers assume
no uniqueness. They are never duplicate or idempotence keys. Client commands
execute in receive order; `No_reply` suppresses only a reply, and unrelated
opposite-direction traffic may interleave without defining a global order.

## DMA Lifetime

Rings/bounded staging use driver-owned coherent mappings. User buffers use
`pin_user_pages*()` with `FOLL_PIN | FOLL_LONGTERM`; device-write targets also use
`FOLL_WRITE`. Registration charges `RLIMIT_MEMLOCK` under normal `CAP_IPC_LOCK`
policy and MetaFlux context quota. Unsupported DAX/non-pinnable memory fails and
never downgrades to short-term GUP.

SG tables record direction and perform non-coherent synchronization. Unwind uses
the same direction, dirties device-written pages, and unpins each page exactly
once through compile-probed `kernel/compat/` shims. Records bind permissions,
range, IOVA segments, mapping epoch, and generation. Unregister blocks new refs,
drains existing refs to deadline, then unmaps/unpins/acks; timeout publishes
`LOST` and tombstones backing until final release.

The server validates DMA flags/fd/offset/overlap/quota/width/epoch and every
descriptor access. `VFIO_USER_DMA_UNMAP` leaves lookup before drain and replies
success only after all queue/backend/callback references and host mappings are
gone. Deadline expiry marks `LOST` and closes the connection without a success
reply; it never falsely acknowledges live guest memory.

## BAR and Notification

- BAR0: 64 KiB control/capability/setup/error plus sized extension directory and
  reserved lifecycle mailbox.
- BAR2: 4 KiB ioeventfd doorbell; provider maps only the permitted page.
- BAR4: 4 KiB MSI-X table/PBA; vector 0 admin/fatal, vector 1 armed completion.
- Descriptor publication uses the project DMA/MMIO barrier before a typed 32-bit
  doorbell. Interrupt readers use `dma_rmb()`; polling uses acquire semantics.
- Completion is release-ordered before timeline. Interrupt only armed waiters or
  fatal transitions; never every descriptor.

milestone-0.1.1.0 leaves `VFIO_DEVICE_FLAGS_RESET` clear in `VFIO_USER_DEVICE_GET_INFO` and
advertises no migration feature. A conforming client therefore does not issue
`VFIO_USER_DEVICE_RESET`; a defensively received command never receives success
and follows the exact error-reply or terminal-close result frozen for the pinned
QEMU/libvfio-user pair. Guest/QEMU reset observation fences work, publishes
`LOST`, drains references, and never publishes replacement `ONLINE`; recovery
needs a fresh static instance. milestone-0.1.2.0 may advertise coordinated reset only after
its lifecycle handler is present, without moving the v1 BAR regions.

## Work

Implemented stage:

- [x] Generate the vfio-user DMA map/unmap and GET_INFO reply records from the
  extension manifest, including generation/epoch and static BAR fields.
- [x] Provide the C17 guest packet encoder/decoder and C++20 server control
  fixture with bounded framing, sender-owned message IDs, and `No_reply`
  handling.
- [x] Validate page-aligned shared-file mappings, overlap, width, fd range,
  generation, epoch, permissions, and zero-reference unmap in the server
  mapping ledger.
- [x] Verify the static BAR0/BAR2/BAR4 profile and unsupported reset/doorbell
  result with focused schema, guest, and server tests.
- [x] Add the compile-checked `metaflux_pci.ko` static guest resource binder:
  validate CI VID/DID/class and exact BAR0/BAR2/BAR4 sizes, map BAR0/BAR2,
  reserve two MSI-X vectors, and unwind remove or probe failure in reverse order.
- [x] Implement the guest SPSC ring half with per-lane state and no cross-lane
  globals: attach with generation/view validation (mismatched-completion and
  null-payload rejection), doorbell publication, single and batch submission,
  completion timeline polling with malformed-completion rejection, and armed
  waits (`mf_vfio_user_guest_ring_*`).
- [x] Test sender-owned message-ID reuse through the server control fixture,
  including `No_reply` followed by a replied command that reuses the same
  message ID and a second reuse after the reply.

- [x] Bring the static guest `metaflux_pci.ko` live under the pinned
  libvfio-user/QEMU pair: runtime BAR0/BAR2 mapping, BAR4 MSI-X delivery, and
  guest provider bring-up beyond the compile-checked resource binder.
  Verified by `metaflux.transport.vfio-user-live-bringup`
  (`transports/vfio-user/live/`): the pinned QEMU 10.2.4 boots the running host
  kernel plus a static busybox initramfs carrying `metaflux_pci.ko` over
  shared file-backed guest RAM; the guest proves PCI identity, exact
  BAR0/BAR2/BAR4 sizes, runtime BAR0 read/write, and module-counted MSI-X
  vectors 0 and 1, while the host proves mappable guest-RAM DMA registration
  and BAR2 doorbell writes. The test skips (77) where QEMU/busybox
  prerequisites are absent, matching the cdev live-qualification pattern.
- [x] Require shared file-backed guest RAM and implement mapping epochs,
  registration, drain, validation, long-term accounting, direction, dirty-unpin,
  and no-success-before-drain unmap.
  Server DMA_MAP now rejects non-regular fds (`S_ISREG`) so socket/pipe-mediated
  region fallback cannot register; mapping-epoch/generation matching, overlap
  rejection, and no-success-before-lease-drain unmap already exist. The userspace
  DMA ledger now tracks device-write `dirty_bytes` and long-term `pin_references`
  per mapping: `mark_dirty` requires a writable live range, `pin_longterm` /
  `unpin_longterm` balance holds exactly once, and DMA_UNMAP replies
  `WOULD_BLOCK` while leases or pins remain. Unpin/lease release finalizes the
  revoking mapping (clearing dirty/pin/mapped counters) and leaves a finalized
  tombstone until a subsequent zero-reference unmap ack removes it, matching the
  existing lease-drain contract. Covered by `metaflux.transport.vfio-user-server`.
  Production `metaflux-vfio-userd` proves DMA_MAP of shared guest-RAM memfd plus
  CPU COPY/completion against that mapped arena
  (`metaflux.services.vfio-userd`).
- [x] Generate guest/server config, BAR, capability, and UAPI fixtures from the
  root manifest; reject any handwritten competing layout.
  Server construction requires `dma_alignment` equal to the generated
  `MF_VFIO_USER_PROFILE_PAGE_SIZE`; GET_INFO projects BAR0/2/4 from the same
  header. `tools/generate-pci-guest-profile.py` composes the root transport
  vfio-user BAR profile with the vroot CI Type-0 identity into
  `metaflux/pci/generated_guest_profile.h`. `metaflux_pci.ko`, the live vfio-user
  fixture server, and the live bring-up guest init script consume that composed
  fixture; handwritten VID/DID/class and BAR-size owners are rejected by
  `metaflux.transport.pci-guest-profile-selftest`.
- [x] Test concurrent opposite-direction interleaving: guest sends N DMA map
  requests without reading replies, server processes all N, guest reads all
  completions and verifies ordering. Also test No_reply followed by replied
  command on the same socket.
- [x] Test CPU Add/Copy execution through the guest ring fastpath: submit COPY
  descriptors from the guest side, consume from the server side of the
  submission ring, produce completions, and verify timeline advancement.
- [x] Test concurrent opposite-direction interleaving without deduplication or
  global ordering.
  `metaflux.transport.vfio-user-interleave` now reuses the same sender-owned
  message ID for map then map, and for unmap then map then GET_INFO, proving
  receive-order processing without treating the ID as a duplicate key. It also
  rejects a pipe fd as DMA backing and treats unmap of an unknown IOVA as
  `STALE_HANDLE` rather than success.
- [x] Freeze the unsupported-reset result for each pinned pair, execute CPU
  Add/Copy before another backend, and treat observed reset as terminal loss.
  Live qualification freezes the pinned QEMU/libvfio-user pair: machine-init
  `VFIO_USER_DEVICE_RESET` returns `EOPNOTSUPP` (never success) and is
  tolerated before guest runtime activity; a type=0 reset after BAR0 runtime
  access began is terminal loss. Production `metaflux-vfio-userd` now proves
  negotiate + DMA_MAP of shared guest-RAM memfd + CPU COPY/completion against
  the mapped guest payload arena and freezes control-path reset as
  `MF_SHARED_NOT_SUPPORTED` (`metaflux.services.vfio-userd`).

## Exit Gate

Guest PCI/sysfs/cdev/CUDA/NVML identity agrees and Add/Copy passes. Tracing proves
active commands cause no vfio-user socket message, QEMU main-loop write, provider
syscall/allocation, or per-command interrupt. Generated bytes match the root
manifest, message concurrency fixtures pass, and no advertised bit promises reset
or migration.
