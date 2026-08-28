# MetaFlux vfio-user Service

Planned QEMU vfio-user PCI server and leased guest data-plane worker. One process
instance owns the backend instance and queue mappings for each accepted
generation, while `metafluxd` remains the registry, policy, and lease authority.
