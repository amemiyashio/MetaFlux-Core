# Software PCI Root

Planned home of the default-off experimental `metaflux_vroot.ko` presentation
module. It exposes only validated config-space identity and lifecycle; execution
continues through canonical MetaFlux queues and nodes.

An optional NVIDIA identity must select only `metaflux_pci` in kernel space before
device publication. Failed deterministic pre-bind selection publishes no device;
a failed MetaFlux probe never falls through to vendor matching.
