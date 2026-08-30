# Kernel PCI Driver

`metaflux_pci.ko` is the common driver for the M0110 static guest MetaFlux PCI
function. Its first stage validates CI VID/DID `0x4D46:0x0001`, class `0x120000`,
and the generated BAR profile (BAR0 64 KiB, BAR2 4 KiB, BAR4 4 KiB), maps BAR0 and
the BAR2 doorbell page, and reserves exactly two MSI-X vectors. BAR4 remains
owned by the PCI MSI-X capability; no table or interrupt handler is installed by
this stage. The driver does not implement NVIDIA RM/UVM behavior.

Build against the exact target kernel tree with:

```sh
make -C /lib/modules/$(uname -r)/build M=$PWD/kernel/pci modules
```

The ring, DMA, ioeventfd, interrupt-arm, and lifecycle paths remain separate
W0113 work. A missing or differently sized BAR profile is rejected before the
device is enabled, and remove releases vectors, mappings, regions, and the PCI
device in reverse order.
