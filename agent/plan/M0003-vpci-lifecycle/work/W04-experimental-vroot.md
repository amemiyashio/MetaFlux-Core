---
id: M0003-W04
milestone: M0003
status: Queued
area: kernel.vroot
depends_on: [M0003-W03]
updated: 2026-08-28
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

The canonical vroot profile lives under
`contracts/protocol/transport/v1/schema/extensions/vroot/v1/`: its manifest
imports the frozen M0002 base and qualified lifecycle-extension manifests by
content hash and references only vroot-owned definitions. Neither imported
manifest points back to vroot or changes bytes. Generated config images, writable
masks, and fixtures are projections of this profile. The direct import list has no
duplicates; an identical base tuple reached transitively through lifecycle is
de-duplicated in the validated import closure, while any path/version/hash conflict
fails qualification. Promotion consumes the M0003-W03-qualified lifecycle hash.

`identity=nvidia` is default-off. Before making config presence observable, the
kernel verifies that `metaflux_pci` is registered, preallocates required state,
and keeps matching disabled. Linux scan calls `device_add()` and may expose an
unbound `pci_dev`/uevent before `pci_bus_add_device()` enables matching, so the
kernel stages the driver override after scan but before that match-enable point.
Registry `ONLINE` and canonical nodes commit only after successful MetaFlux probe.
Every re-add repeats this sequence. A failed MetaFlux probe is quarantined and
removed while matching remains excluded, never allowed to fall through to vendor
matching. Guest images use the equivalent early-boot binding policy before vendor
modules load.

Config callbacks allocate nothing, do not sleep, make no RPC/userspace access,
and touch only a documented writable-field whitelist. Sysfs `remove` removes the
current `pci_dev`; rescan may rediscover it only while logical presence is true.

Canonical nodes remain the M0002-owned `/dev/metafluxctl` and `/dev/metafluxN`;
vroot adds no functional UVM node or base ioctl/mmap contract. Host-wide udev never
creates NVIDIA aliases. An explicit launcher may expose only aliases backed by a
named existing MetaFlux contract in a dedicated mount namespace after proving that
no vendor node is hidden; it never creates `/dev/nvidia-uvm*`, replaces a vendor
node, or changes a vendor-owned node.

## Work

- [ ] Implement bridge/config/present/allocation state with unwind-safe
  load/unload and one static plus dynamic add/remove function.
- [ ] Generate config images, writable masks, and fixtures from the independent
  vroot extension manifest; verify both imported manifest hashes remain frozen.
- [ ] Implement kernel-side pre-bind, sysfs/uevent, remove/rescan, and canonical
  node/namespace policy.
- [ ] Trace config presence, scan-time `device_add`/uevent, match enable,
  override, MetaFlux probe, registry/node commit, quarantine, and removal as
  distinct boundaries; assert zero vendor-driver probe attempts.
- [ ] Fuzz config offset, width, writable masks, and init-failure cleanup.
- [ ] Run 1,000 add/remove and load/unload cycles under concurrent `lspci`,
  rescan, open, mmap, and submit on Linux 6.12 and 6.18.
- [ ] Produce `packages.x86_64-linux.metaflux-vroot-dkms`,
  `packages.x86_64-linux.metaflux-vroot-launcher`, and
  `checks.x86_64-linux.baremetal-vpci`; qualify signing, install/upgrade,
  namespace isolation, coexistence, and uninstall for this separate package. The
  launcher and check depend on the vroot package, never the M0002 base
  `metaflux-vpci-dkms` output.
- [ ] Measure vroot/config/sysfs/`lspci` and 1 Hz `nvidia-smi` overhead separately
  from lifecycle core, including proof that launch never enters
  `metaflux_vroot.ko`.

## Exit Gate

`lspci -Dnn`, sysfs, driver, UUID/local BDF, uevents, and nodes remain consistent;
callbacks meet the no-sleep/no-allocation/no-RPC rule; no duplicate function,
false capability, vendor-driver match, or lifecycle defect appears. A transient
scan-visible unbound function never owns nodes or registry `ONLINE`, and failed
MetaFlux probe is removed without fallthrough. This promotes only the separate
experimental package and never delays the lifecycle core. Vroot throughput loss
is at most 0.5%, 1 Hz `nvidia-smi` remains inside the M0001 compute-impact budget,
and the three named package/check outputs pass their release gates.
