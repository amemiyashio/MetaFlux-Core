# Configuration and Enumeration

## Profile matrix

| Property | M0002 guest function | Initial M0003 vroot fixture |
| --- | --- | --- |
| Function type | QEMU `vfio-user-pci` Type-0 | Software-hosted Type-0 `pci_dev` |
| Identity | CI `0x4D46:0x0001`, class `0x120000` | Same CI identity; optional default-off presentation policy |
| Config size | Per pinned QEMU/profile contract | Preallocated 256-byte Type-0 image |
| BAR/IRQ | BAR0, BAR2, BAR4/MSI-X | None initially |
| Reset/hotplug | Static cold-plug, terminal reset in M0002 | Dynamic add/remove/re-add through M0003 lifecycle |

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
- Serialize presence changes with scan/add/remove so no half-created function is
  discoverable and no absent function responds as present.

Primary source: [Linux PCI driver documentation](https://docs.kernel.org/driver-api/pci/index.html).
