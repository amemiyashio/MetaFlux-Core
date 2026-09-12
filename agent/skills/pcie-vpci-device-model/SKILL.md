---
name: pcie-vpci-device-model
description: Implement or review PCI or vPCI Type-0 configuration space, domain and BDF identity, VID/DID/class, BAR0/BAR2/BAR4, MSI-X, enumeration, host bridges, guest driver binding, reset, and hotplug. Use for milestone-0.1.1.0 guest PCI or milestone-0.1.2.0 vroot presentation. Do not use for PCIe electrical/link training or excluded SR-IOV, ATS, PASID, PRI, P2P, or AER features.
---

# PCIe and vPCI Device Model

Own config-space presentation, enumeration and binding. First select the static
QEMU guest function or default-off software vroot in the
[implementation path](references/implementation-path.md). Locate the exact
config access, bind, BAR/IRQ or removal callback that must change and implement
its observable behavior with the corresponding consumer.

Analysis/review requests stay read-only; implementation steps apply to requested
changes. Use the assignment and Exit Gate through [$main](../main/SKILL.md) skill.
Reuse the profile's generated config image and identity rules. Follow
[implementation guidance](../review/references/implementation-guidance.md);
repeated `lspci` enumeration does not implement a missing data-plane operation.

## Select The Work

| Task | Read before changing that boundary |
| --- | --- |
| Config bytes, access widths, writable masks or BDF | [Config and enumeration](references/config-and-enumeration.md) |
| Guest BAR0/2/4, MSI-X or doorbell ordering | [BARs and MSI-X](references/bars-msix-ordering.md) |
| Guest binding, vroot scan, override or sysfs | [Host bridge and binding](references/host-bridge-binding.md) |
| Reset/remove/re-add and stale access | [Reset and hotplug](references/reset-hotplug.md) |

The guest profile has BARs/MSI-X; the initial vroot fixture advertises no BAR,
IRQ, PM, PCIe or FLR capability. Never infer capability from the PCI name.
Use the profile's manifest/import closure, generated image and writable masks;
callbacks perform no allocation, sleep, RPC or userspace access.

UUID is persistent identity; a live incarnation also needs its generation.
BDF is enumeration-domain-local. Scan-time `device_add()` can expose an unbound
function before matching; quarantine it, enforce exclusive MetaFlux binding,
and publish registry `ONLINE` only after successful bind and authority commit.
Preserve excluded SR-IOV/ATS/PASID/PRI/P2P/AER boundaries.

## Compose At The Crossing

[$gpu-virtualization-vfio-user](../gpu-virtualization-vfio-user/SKILL.md) skill owns
wire and guest-DMA transport; [$linux-device-driver-uapi](../linux-device-driver-uapi/SKILL.md) skill
owns kernel UAPI. [$device-lifecycle-resilience](../device-lifecycle-resilience/SKILL.md) skill
owns reset/replacement state. Presentation mirrors committed identity and keeps
old-generation config, BAR and IRQ accesses isolated.

Return the selected profile, changed callback/consumer and exact access/binding
or lifecycle evidence to parent review. Final qualification retains that
profile's kernel/QEMU and cycle requirements without claiming other profiles.
