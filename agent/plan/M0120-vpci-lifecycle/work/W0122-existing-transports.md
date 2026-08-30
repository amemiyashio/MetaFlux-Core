---
id: W0122
delivery: 0.1.2.2
milestone: M0120
status: Queued
area: lifecycle.transports
depends_on: [W0121]
updated: 2026-08-30
---

# Lifecycle on Existing Transports

## Outcome

Apply one generation transaction to the M0100 memfd fallback, M0110 local cdev,
and M0110 guest vfio-user paths without changing the M0110 descriptor, base
ioctl/mmap UAPI, BAR layout, or steady-state ring.

`metaflux_core.ko`, `metaflux_pci.ko`, and `metaflux-vfio-userd` mirror the
committed lifecycle and own local reference safety. The guest worker uses the
private `metafluxd`/`metaflux-vfio-userd` control channel; `/dev/metafluxctl`
remains the local broker.

Admin reset, `VFIO_USER_DEVICE_RESET`, QMP remove/add, server/QEMU disconnect,
guest unload/reload, and daemon restart enter one request normalizer. No source
may publish a generation independently. Every add/reset stages an exclusive
worker lease before registry commit; drain/remove revokes new attachment before
retirement. Brokers reject a second binding, stale daemon incarnation, or
completion after revocation.

The cold-control QMP adapter correlates commands/events with lifecycle request
IDs. Remove publishes `QUIESCING` before `device_del` and waits for the matching
device-deleted event; QMP failure leaves a still-present function rejecting work
in `LOST`. Add stages server/socket and an unpublished generation before
`device_add`; BAR0 remains not-ready until commit. If QMP succeeds but commit
fails, the adapter removes the staged function, destroys staging, and leaves the
candidate consumed without advancing epoch because it never became a committed
generation. Partial success never restores a retired generation to `ONLINE`.

CUDA and NVML retain the M0100 provider-view epoch and shared
`registry_view_id`. Removal immediately makes old handles return `DEVICE_LOST`.
Later additions appear only after a later NVML zero-to-one init epoch or in a new
process, never as a new ordinal in an initialized CUDA process. Default
unfiltered membership/order is identical only when both providers captured the
same process-view revision; after NVML reinitializes while CUDA remains initialized,
their count/order may diverge. Common live incarnations match by
`(UUID, generation)`, never UUID/BDF alone. `CUDA_VISIBLE_DEVICES` affects CUDA
only.

## Work

- [ ] Implement coordinator transactions and bounded transport mirror callbacks.
- [ ] Exercise identical quiesce, drain, tombstone, `LOST`, and `ABSENT` paths on
  memfd, cdev, and guest vfio-user.
- [ ] Integrate every reset/disconnect/restart source and inject failure at each
  staging, commit, DMA, completion, and teardown step.
- [ ] Verify provider enumeration freeze before, during, and after replacement.

## Exit Gate

Local and guest CPU Add/Copy recover only through a new generation; old objects
return `DEVICE_LOST`; every failure reaches complete `ONLINE`, `LOST`, or
`ABSENT`; no transport bypasses the coordinator.
