# metaflux-vpci DKMS

Milestone-0.1.1.0 static guest PCI driver package for `metaflux_pci.ko`. It ships
module source and a frozen generated guest profile header that locks CI Type-0
identity plus BAR0/BAR2/BAR4 and MSI-X vector counts.

## Ownership

- Package name: `metaflux-vpci` (DKMS module package `metaflux-vpci-dkms`)
- Base transport guest presentation for cdev/vfio-user qualification rows
- Independent of experimental `metaflux-vroot-dkms`
- Lifecycle-core and vfio-userd release rows may depend on this package; vroot
  launcher and baremetal-vpci must not

## Staging

```sh
nix develop . --command python3 tools/stage-vpci-dkms.py \
  --output-dir /path/to/metaflux-vpci-0.1.0
```

The stager composes the guest profile from the frozen transport base and vroot
identity lock, freezes `generated_guest_profile.h`, copies
`kernel/pci/metaflux_pci_main.c`, and writes `package-metadata.json`. Target
hosts build through the kernel Kbuild tree without repository Python.

## Install posture

`AUTOINSTALL=no`. Operators load the module explicitly after guest bring-up or
host qualification. Module signing and Secure Boot remain open packaging
decisions before release hardening.
