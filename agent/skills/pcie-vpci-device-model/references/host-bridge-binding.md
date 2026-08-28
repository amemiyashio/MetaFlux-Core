# Host Bridge and Binding

## QEMU guest profile

Attach one `vfio-user-pci` function per server socket using the pinned QEMU
configuration and shared file-backed guest memory. Guest early boot must bind the
function to `metaflux_pci` before any vendor driver can claim a synthetic identity.
Verify PCI/sysfs/cdev identity and that removal/reset produces deterministic
device loss to guest userspace.

## Bare-metal vroot profile

`metaflux_vroot.ko` is a default-off software `pci_host_bridge` for presentation,
not a physical endpoint and not a data-plane transport. It owns an isolated
domain/bus allocation, preallocated config images, presence bits, writable masks,
serialized scan/add/remove, sysfs, and uevents.

Linux PCI publication has distinct boundaries. Scanning creates the `pci_dev`
and calls `device_add()`, which can make the kobject/sysfs identity and uevent
observable. `pci_bus_add_device()` later enables matching and starts probe; it is
not the first visibility point. The vroot sequence is therefore:

1. before config presence, verify `metaflux_pci` is registered and preallocate
   every policy resource needed for exclusive selection;
2. expose config presence and scan while PCI matching remains disabled by the
   target kernel's normal scan/add sequence;
3. set and verify the exclusive `metaflux_pci` override before calling
   `pci_bus_add_device()` or otherwise enabling matching;
4. allow only `metaflux_pci` to probe, and commit registry `ONLINE` only after a
   successful bind plus lifecycle prepare/commit;
5. on override or probe failure, keep the function out of registry visibility,
   quarantine it, and remove it without ever enabling vendor matching;
6. repeat the full sequence on every re-add.

Tests must treat a scan-created, unbound `pci_dev` as possible transient external
visibility. Capture its uevent and prove it cannot bind a vendor driver, create
canonical device nodes, or be mistaken for committed MetaFlux registry state.

Version-sensitive source checks:

- [Linux v6.12 PCI scan/add](https://github.com/torvalds/linux/blob/v6.12/drivers/pci/probe.c)
  and [bus match enable](https://github.com/torvalds/linux/blob/v6.12/drivers/pci/bus.c);
- [Linux v6.18 PCI scan/add](https://github.com/torvalds/linux/blob/v6.18/drivers/pci/probe.c)
  and [bus match enable](https://github.com/torvalds/linux/blob/v6.18/drivers/pci/bus.c).

Config callbacks allocate nothing, do not sleep, make no RPC/userspace access,
and touch only the predeclared image/masks. Sysfs remove destroys the current
`pci_dev`; rescan may rediscover it only while logical presence remains true.

Canonical nodes remain MetaFlux-owned. Namespace aliases never replace or hide a
vendor-owned node.
