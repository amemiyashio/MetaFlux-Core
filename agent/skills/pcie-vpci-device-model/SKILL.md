---
name: pcie-vpci-device-model
description: Design or review PCI or vPCI Type-0 configuration space, domain and BDF identity, VID/DID/class, BAR0/BAR2/BAR4, MSI-X, enumeration, host bridges, guest driver binding, reset, and hotplug. Use for M0002 guest PCI or M0003 vroot presentation. Do not use for PCIe electrical/link training or excluded SR-IOV, ATS, PASID, PRI, P2P, or AER features.
---

# PCIe and vPCI Device Model

## Inputs

- The active M0002/M0003 work item and whether the target is the static QEMU
  guest function or the default-off bare-metal software root.
- Exact config image/capability list, BDF allocation policy, VID/DID/class,
  BAR/region contract, MSI-X vectors, driver binding policy, and lifecycle state.
- `lspci`, sysfs, config-access, BAR, IRQ, reset, add/remove, and concurrent-use
  evidence for the target kernel/QEMU matrix.

Do not merge the two profiles: the M0002 guest function has BARs/MSI-X, while the
initial M0003 vroot fixture deliberately advertises no BAR, IRQ, PM, PCIe, or FLR
capability.

## Routing

- Use [config and enumeration](references/config-and-enumeration.md) for Type-0
  images, identity, writable masks, BDF/domain, and enumeration behavior.
- Use [BARs, MSI-X, and ordering](references/bars-msix-ordering.md) for the guest
  BAR0/2/4 profile, vectors, access widths, and notification ordering.
- Use [host bridge and binding](references/host-bridge-binding.md) for QEMU guest
  attachment, software `pci_host_bridge`, pre-bind, sysfs, and uevents.
- Use [reset and hotplug](references/reset-hotplug.md) for PCI-visible lifecycle
  sequencing and stale-access behavior.
- Route vfio-user wire/DMA transport to `$gpu-virtualization-vfio-user`, kernel
  cdev/UAPI to `$linux-device-driver-uapi`, and authoritative generation state
  to `$device-lifecycle-resilience`.

## Workflow

1. Select the guest or vroot profile and enumerate every implemented config
   byte, capability, writable mask, side effect, reset value, and unsupported
   feature before coding callbacks.
2. Define identity: canonical UUID, enumeration-domain-local BDF, CI versus
   release VID/DID, class/revision, subsystem fields, and driver-binding policy.
3. Build a preallocated Type-0 config image with width/offset validation and
   serialized presence/add/remove. Config callbacks perform no allocation,
   sleep, RPC, or userspace access.
4. For the guest profile, define BAR sizing/probing, mmap regions, access widths,
   MSI-X table/PBA, vector masking, ioeventfd/irqfd, and DMA/MMIO ordering.
5. For vroot, define domain/bus/devfn allocation, host bridge ownership,
   scan/rescan/remove, sysfs/uevent behavior, and exclusive MetaFlux pre-bind.
   Do not claim unimplemented capabilities to satisfy a probing tool.
6. Sequence reset/remove/re-add with the lifecycle authority so stale config,
   BAR, IRQ, cdev, and userspace handles cannot attach to a new generation.
7. Fuzz config accesses and stress enumeration/lifecycle concurrently on every
   supported kernel/QEMU pair.

## Output

Return or implement:

- A byte/capability/writable-mask config specification for the selected profile.
- A BAR/MSI-X/access/ordering table where those capabilities exist.
- Enumeration, binding, reset, remove, and re-add state transitions tied to UUID,
  BDF, generation, and driver ownership.
- Qualification evidence from `lspci`, sysfs, config fuzzing, IRQ tracing, and
  lifecycle stress without overstating experimental vroot promotion.

## Verification

- Test config reads/writes for every offset, width, boundary, writable bit,
  reserved byte, sizing probe, absent function, and concurrent remove.
- Verify guest BAR sizes, region protections, typed doorbell writes, MSI-X
  table/PBA masking, vector routing, and no per-command interrupt.
- Verify UUID and generation across domains while treating BDF as stable only
  within its enumeration domain; cross-check CUDA/NVML/PCI/sysfs/cdev identity.
- On vroot, run `lspci -Dnn`, sysfs/uevent/binding tests and the planned 1,000
  add/remove plus load/unload concurrent-use cycles on Linux 6.12 and 6.18.
- Confirm no excluded PCIe capability is advertised and no vendor-owned driver,
  node, or function is displaced.
