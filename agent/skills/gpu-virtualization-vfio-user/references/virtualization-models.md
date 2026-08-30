# Virtualization Models

## Selection matrix

| Model | Resource owner | Guest interface | Hardware dependency | MetaFlux role |
| --- | --- | --- | --- | --- |
| VFIO passthrough | Host VFIO/IOMMU assigns a physical function | Physical device and vendor ABI | IOMMU plus assignable hardware | Comparison only; it does not present the software CPU backend |
| Mediated device | Parent driver multiplexes a physical device through VFIO | Vendor/parent-defined mediated function | Supporting physical parent driver | Comparison only; not the M0110 transport |
| SR-IOV | PF creates hardware VFs | Hardware PCI virtual functions | SR-IOV-capable device and PF policy | Explicitly excluded from M0110-M0130 |
| vfio-user | Userspace server implements a VFIO device over a Unix socket | QEMU `vfio-user-pci` function | Shared memory plus QEMU/KVM; no physical GPU required | Selected static guest transport for M0110 |

MetaFlux chooses vfio-user because a userspace server can present one software
compute function backed by the ecosystem-neutral CPU backend while QEMU handles
guest PCI integration. The high-speed data plane uses shared guest RAM and
eventfds; the protocol socket remains control plane.

This choice does not emulate NVIDIA RM/UVM, expose a physical GPU, or make vPCI
the product identity. CUDA/NVML stay application-facing compatibility plugins;
the persistent MetaFlux UUID is canonical identity.

## Re-evaluation triggers

Revisit the model only if a milestone decision changes required hardware
isolation, migration, multi-tenant scheduling, physical-device ownership, or
guest deployment. Compare isolation, reset granularity, DMA ownership, kernel
surface, deployment dependencies, performance, and qualification burden before
changing the architecture.

Primary sources:

- [Linux VFIO](https://docs.kernel.org/driver-api/vfio.html)
- [VFIO mediated devices](https://docs.kernel.org/6.9/driver-api/vfio-mediated-device.html)
- [PCI SR-IOV how-to](https://docs.kernel.org/6.0/PCI/pci-iov-howto.html)
- [QEMU vfio-user device](https://www.qemu.org/docs/master/system/devices/vfio-user.html)
