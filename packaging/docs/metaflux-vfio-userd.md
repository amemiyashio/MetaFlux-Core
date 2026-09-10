# metaflux-vfio-userd packaging

Service packaging notes for the production vfio-user daemon binary built from
`services/metaflux-vfio-userd/`.

## Artifact

- Binary name: `metaflux-vfio-userd`
- CMake install component: `Daemon`
- Sibling of `metaflux-vpci-dkms` for milestone-0.1.1.0 guest transport release
  rows; not a dependency of experimental `metaflux-vroot-dkms`

## Policy

- Does not install vendor CUDA/NVML libraries
- Does not create `/dev/nvidia*` nodes
- Consumes the frozen guest PCI profile and transport base contracts at build
  time; runtime policy remains the vfio-user server and QMP adapters

Live package install/upgrade/remove and QEMU fixture rows remain under
`tests/release/` host qualification gates.
