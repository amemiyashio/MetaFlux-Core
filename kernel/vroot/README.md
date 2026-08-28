# Software PCI Root

Planned home of the default-off experimental `metaflux_vroot.ko` presentation
module. It exposes only validated config-space identity and lifecycle; execution
continues through canonical MetaFlux queues and nodes.

A synthetic vendor identity (for example `identity=nvidia`) is a presentation
disguise governed by decision D0008 and qualified in M0003-W04. It must select
only `metaflux_pci` in kernel space before device publication, is never eligible
for vendor-driver matching, and never implements vendor-private RM/UVM behavior.
Failed deterministic pre-bind selection publishes no device; a failed MetaFlux
probe never falls through to vendor matching. Release promotion additionally
requires the registration and legal review recorded in the M0003 decisions.
