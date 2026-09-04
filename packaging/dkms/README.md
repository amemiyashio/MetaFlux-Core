# DKMS Packaging

Generic external-module source packages build through the target kernel's
Kbuild tree, carry compile-probed compatibility shims, and do not claim one
module binary works across kernel configurations.

## Packages

| Package tree | Artifact | Role |
| --- | --- | --- |
| `metaflux-vpci/` | `metaflux-vpci-dkms` | milestone-0.1.1.0 static guest `metaflux_pci.ko` |
| `metaflux-vroot/` | `metaflux-vroot-dkms` | experimental bare-metal `metaflux_vroot.ko` presentation |

`metaflux-vpci-dkms` is staged by `tools/stage-vpci-dkms.py` from the combined
guest PCI profile (transport BAR/MSI-X + vroot CI identity). It is the base
guest transport module package and forbids depending on experimental vroot.

`metaflux-vroot-dkms` is staged by `tools/stage-vroot-dkms.py`, which freezes the
generated Type-0 profile header so target hosts need no repository Python or
contract trees. The package is default-off (`AUTOINSTALL=no`), presentation
only, and must never become a dependency of lifecycle-core release rows. The
packaging-owned launcher lives under `packaging/vroot-launcher/` and depends on
vroot metadata only—never on `metaflux-vpci-dkms`. Tests own the
`baremetal-vpci` gate under `tests/release/`. Service packaging notes for
`metaflux-vfio-userd` live under `packaging/services/metaflux-vfio-userd/`.
