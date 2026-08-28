# vfio-user Transport

Guest transport adapter between QEMU/libvfio-user framing and the common MetaFlux
device protocol. `metaflux-vfio-userd` owns the leased guest backend instance;
steady-state descriptors and data stay in DMA-mapped guest RAM.

The adapter converts host-native vfio-user fields at its boundary. MetaFlux BAR,
ring, and DMA records remain explicitly little-endian. A DMA unmap succeeds only
after all worker/backend references drain; timeout closes the connection without
false success and retains isolated tombstones until final release.
