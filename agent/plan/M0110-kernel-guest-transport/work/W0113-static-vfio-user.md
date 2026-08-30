---
id: W0113
delivery: 0.1.1.3
milestone: M0110
status: Active
area: transport.vfio-user
depends_on: [W0111]
updated: 2026-08-30
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

M0110 leaves `VFIO_DEVICE_FLAGS_RESET` clear in `VFIO_USER_DEVICE_GET_INFO` and
advertises no migration feature. A conforming client therefore does not issue
`VFIO_USER_DEVICE_RESET`; a defensively received command never receives success
and follows the exact error-reply or terminal-close result frozen for the pinned
QEMU/libvfio-user pair. Guest/QEMU reset observation fences work, publishes
`LOST`, drains references, and never publishes replacement `ONLINE`; recovery
needs a fresh static instance. M0120 may advertise coordinated reset only after
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

- [ ] Implement static guest `metaflux_pci.ko`, BAR0/BAR2/BAR4, and MSI-X with
  pinned libvfio-user.
- [ ] Require shared file-backed guest RAM and implement mapping epochs,
  registration, drain, validation, long-term accounting, direction, dirty-unpin,
  and no-success-before-drain unmap.
- [ ] Implement guest SPSC rings, BAR2 publication, timeline polling, armed waits,
  and multi-stream submission.
- [ ] Generate guest/server config, BAR, capability, and UAPI fixtures from the
  root manifest; reject any handwritten competing layout.
- [ ] Test concurrent message-ID reuse, `No_reply` followed by a replied command,
  and opposite-direction interleaving without deduplication or global ordering.
- [ ] Freeze the unsupported-reset result for each pinned pair, execute CPU
  Add/Copy before another backend, and treat observed reset as terminal loss.

## Exit Gate

Guest PCI/sysfs/cdev/CUDA/NVML identity agrees and Add/Copy passes. Tracing proves
active commands cause no vfio-user socket message, QEMU main-loop write, provider
syscall/allocation, or per-command interrupt. Generated bytes match the root
manifest, message concurrency fixtures pass, and no advertised bit promises reset
or migration.
