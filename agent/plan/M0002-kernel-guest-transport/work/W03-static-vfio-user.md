---
id: M0002-W03
milestone: M0002
status: Queued
area: transport.vfio-user
depends_on: [M0002-W01]
updated: 2026-08-27
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

Guest/QEMU/`VFIO_USER_DEVICE_RESET` fences work, publishes `LOST`, drains refs,
and never publishes replacement `ONLINE`; recovery needs a fresh static instance.
M0003 adds coordinated generations without moving the v1 BAR regions.

## Work

- [ ] Implement static guest `metaflux_pci.ko`, BAR0/BAR2/BAR4, and MSI-X with
  pinned libvfio-user.
- [ ] Require shared file-backed guest RAM and implement mapping epochs,
  registration, drain, validation, long-term accounting, direction, dirty-unpin,
  and no-success-before-drain unmap.
- [ ] Implement guest SPSC rings, BAR2 publication, timeline polling, armed waits,
  and multi-stream submission.
- [ ] Execute CPU Add/Copy before another backend; treat reset as terminal loss.

## Exit Gate

Guest PCI/sysfs/cdev/CUDA/NVML identity agrees and Add/Copy passes. Tracing proves
active commands cause no vfio-user socket message, QEMU main-loop write, provider
syscall/allocation, or per-command interrupt.
