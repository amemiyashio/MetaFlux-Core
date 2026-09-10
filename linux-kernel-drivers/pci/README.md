# Kernel PCI Driver

`metaflux_pci.ko` is the common driver for the milestone-0.1.1.0 static guest MetaFlux PCI
function. Its first stage validates the CI Type-0 identity and BAR profile generated
by `tools/generate-pci-guest-profile.py` (composing the root transport vfio-user
profile with the vroot identity lock: VID/DID `0x4D46:0x0001`, class `0x120000`,
BAR0 64 KiB, BAR2 4 KiB, BAR4 4 KiB, two MSI-X vectors). The Kbuild rule emits
`generated/include/metaflux/pci/generated_guest_profile.h` before compiling
`metaflux_pci_main.c`. The driver maps BAR0 and the BAR2 doorbell page and
reserves exactly two MSI-X vectors. BAR4 remains owned by the PCI MSI-X
capability; the driver registers one bounded IRQ handler per vector and records
delivered notifications without exposing a new UAPI. The driver does not
implement NVIDIA RM/UVM behavior.

Build against the exact target kernel tree with:

```sh
make -C /lib/modules/$(uname -r)/build M=$PWD/linux-kernel-drivers/pci modules
```

The ring, DMA, ioeventfd, interrupt-arm state machine, and lifecycle paths remain
separate work-item-0.1.1.3 work. A missing or differently sized BAR profile is rejected before the
device is enabled, and remove releases vectors, mappings, regions, and the PCI
device in reverse order. Probe establishes a 64-bit coherent DMA mask when
available and falls back to a 32-bit mask before any BAR mapping or MSI-X
allocation; a device that supports neither width is rejected. The selected
width remains private until the transport negotiation path exposes a matching
generated capability field.
