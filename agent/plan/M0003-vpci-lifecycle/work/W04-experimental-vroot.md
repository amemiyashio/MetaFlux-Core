---
id: M0003-W04
milestone: M0003
status: Queued
area: kernel.vroot
depends_on: [M0003-W01]
updated: 2026-08-27
---

# Experimental Bare-Metal vPCI Presentation

## Outcome

Provide a default-off `metaflux_vroot.ko` software `pci_host_bridge` for
presentation only. It owns isolated domain/bus allocation, preallocated Type-0
256-byte config images, present bits, writable masks, serialized scan/add/remove,
uevents, and compile-probed Linux 6.12/6.18 compatibility shims. It never enters
launch, copy, event, or metrics steady state.

The first fixture exposes class `0x120000`, CI identity `0x4D46:0x0001`, and no
BAR, IRQ, PM, PCIe, or FLR capability; execution continues through
`/dev/metafluxN`. UUID survives restart/mapping; BDF is stable only inside its
enumeration domain.

`identity=nvidia` is default-off. Before `pci_bus_add_device()` exposes a
function, the kernel stages its driver override to `metaflux_pci`, verifies that
driver is registered, and aborts publication unless exclusive pre-bind selection
is guaranteed. Every re-add repeats this sequence. A failed MetaFlux probe is
quarantined and removed, never allowed to fall through to vendor matching. Guest
images use the equivalent early-boot binding policy before vendor modules load.

Config callbacks allocate nothing, do not sleep, make no RPC/userspace access,
and touch only a documented writable-field whitelist. Sysfs `remove` removes the
current `pci_dev`; rescan may rediscover it only while logical presence is true.

Canonical nodes are `/dev/metafluxctl`, `/dev/metaflux-uvm`, and
`/dev/metafluxN`; the UVM node does not claim NVIDIA UVM ioctl compatibility.
Host-wide udev never creates NVIDIA aliases. An explicit launcher may expose only
required `/dev/nvidia*` aliases in a dedicated mount namespace after proving that
no vendor node is hidden; it never replaces or changes a vendor-owned node.

## Work

- [ ] Implement bridge/config/present/allocation state with unwind-safe
  load/unload and one static plus dynamic add/remove function.
- [ ] Implement kernel-side pre-bind, sysfs/uevent, remove/rescan, and canonical
  node/namespace policy.
- [ ] Fuzz config offset, width, writable masks, and init-failure cleanup.
- [ ] Run 1,000 add/remove and load/unload cycles under concurrent `lspci`,
  rescan, open, mmap, and submit on Linux 6.12 and 6.18.

## Exit Gate

`lspci -Dnn`, sysfs, driver, UUID/local BDF, uevents, and nodes remain consistent;
callbacks meet the no-sleep/no-allocation/no-RPC rule; no duplicate function,
false capability, or lifecycle defect appears. This promotes only the separate
experimental package and never delays the lifecycle core.
