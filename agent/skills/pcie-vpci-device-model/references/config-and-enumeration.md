# Configuration and Enumeration

## Canonical ownership

The milestone-0.1.1.0 guest config/BAR profile is declared by the root manifest at
`contracts/protocol/transport/v1/schema/manifest.json`. The milestone-0.1.2.0 vroot profile
is declared by
`contracts/protocol/transport/v1/schema/extensions/vroot/v1/manifest.json`, which
imports the frozen milestone-0.1.1.0 base and lifecycle-extension manifests by content hash
without changing either. The import closure de-duplicates an identical base tuple
reached directly and through lifecycle; duplicate direct entries, cycles, and
path/version/hash conflicts fail validation. Generated config images, writable masks, BAR tables,
and byte fixtures are consumed by QEMU/server and kernel adapters; callback-local
copies are not authoritative.

## Profile matrix

| Property | milestone-0.1.1.0 guest function | Initial milestone-0.1.2.0 vroot fixture |
| --- | --- | --- |
| Function type | QEMU `vfio-user-pci` Type-0 | Software-hosted Type-0 `pci_dev` |
| Identity | CI `0x4D46:0x0001`, class `0x120000` | Same CI identity; optional default-off presentation policy |
| Config size | Per pinned QEMU/profile contract | Preallocated 256-byte Type-0 image |
| BAR/IRQ | BAR0, BAR2, BAR4/MSI-X | None initially |
| Reset/hotplug | Static cold-plug, unadvertised observed reset is terminal in milestone-0.1.1.0 | Dynamic add/remove/re-add through milestone-0.1.2.0 lifecycle |

Release VID/DID is an open decision and requires the repository's registration
or deployment-supplied process. Synthetic vendor identity is presentation only,
never a claim to a vendor-private ABI.

## Config checklist

- Specify reset value and writable mask for every byte. Unimplemented/reserved
  fields read as the architecture requires and ignore/reject writes consistently.
- Validate aligned and unaligned accesses exactly as the host bridge/QEMU
  contract permits; reject cross-boundary and unsupported widths safely.
- Implement command/status, class/revision, header type, subsystem fields,
  interrupt pin/line, BAR probes, capability pointer/list, and extended config
  only when the profile advertises them.
- Keep BDF allocation unique within one enumeration domain and persistent only
  to the plan's stated scope. UUID remains cross-domain identity.
- Serialize config presence, scan/add/remove, matching enable, driver bind, and
  registry publication. Linux PCI scanning calls `device_add()` before
  `pci_bus_add_device()`, so an unbound `pci_dev` may be visible before matching
  is enabled. It must remain quarantined, never registry-online, and unable to
  match any vendor driver; an absent function still never answers config reads.

Primary source: [Linux PCI driver documentation](https://docs.kernel.org/driver-api/pci/index.html).
