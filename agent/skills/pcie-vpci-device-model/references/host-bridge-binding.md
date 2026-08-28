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

Before `pci_bus_add_device()` exposes a function under an optional synthetic
identity:

1. verify `metaflux_pci` is registered;
2. stage an exclusive driver override/pre-bind policy;
3. publish only if vendor matching cannot race or take ownership;
4. quarantine and remove a failed MetaFlux probe;
5. repeat the full sequence on every re-add.

Config callbacks allocate nothing, do not sleep, make no RPC/userspace access,
and touch only the predeclared image/masks. Sysfs remove destroys the current
`pci_dev`; rescan may rediscover it only while logical presence remains true.

Canonical nodes remain MetaFlux-owned. Namespace aliases never replace or hide a
vendor-owned node.
