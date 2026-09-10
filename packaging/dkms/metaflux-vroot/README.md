# metaflux-vroot DKMS

Experimental, default-off bare-metal vPCI presentation package. It ships only
`metaflux_vroot.ko` source and a frozen generated Type-0 profile header.

## Ownership

- Package name: `metaflux-vroot` (DKMS module package `metaflux-vroot-dkms`)
- Presentation only; never enters launch, copy, event, or metrics steady state
- Independent of milestone-0.1.1.0 `metaflux-vpci-dkms`
- Lifecycle core must install and run without this package

## Staging

```sh
nix develop . --command python3 packaging/dkms/stage-vroot-dkms.py \
  --output-dir /path/to/metaflux-vroot-0.1.0
```

The stager validates the vroot extension import closure, freezes
`generated_profile.h`, copies `kernel/vroot/metaflux_vroot_main.c`, and writes
`package-metadata.json` with content hashes. Target hosts build through the
kernel Kbuild tree; they do not need repository Python or contracts.

## Install posture

`AUTOINSTALL=no`. Operators load the module explicitly after install. Module
parameters (`function_count`, `domain`, `bus`) remain the only runtime controls.
Module signing and Secure Boot remain open packaging decisions before release
promotion.
