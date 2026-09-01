# Software PCI Root

Home of the default-off experimental `metaflux_vroot.ko` presentation module.
The Kbuild module creates one isolated software `pci_host_bridge`, exposes only
validated config-space identity, and removes the root before module unload;
execution continues through canonical MetaFlux queues and nodes.

The host-independent `metaflux_vroot_model` is now the executable contract for
the first vroot slice. It allocates at most eight functions in one domain and
bus, assigns stable devfns within that enumeration domain, and owns a
preallocated 256-byte Type-0 image per function. The model exposes the CI
identity (`0x4d46:0x0001`, class `0x120000`), no BAR or interrupt capability,
read-only config bytes, and explicit `present`, `matching_enabled`, `bound`,
`online`, and `quarantined` boundaries.

The required ordering is `add -> prepare_driver -> enable_matching -> probe ->
online`. A probe failure removes config presence and quarantines the function;
it never enables a vendor match. Rescan can retry only while logical presence
remains, and remove clears logical presence so a later add receives a new
generation. The model uses fixed storage and no allocation, sleep, RPC, or
userspace access, making it suitable for callback and config-access tests.

The Kbuild module consumes a kernel-compatible projection of the same profile.
Its `function_count` parameter controls logical functions visible to a
subsequent PCI rescan, while the root bus `remove` path handles disappearance.
Config access uses fixed storage, a spinlock, and the generated writable mask;
no allocation, sleep, RPC, or userspace access occurs in the callback.

The presentation driver is intentionally named `metaflux_vroot`: the canonical
vroot profile has no BAR or IRQ capability, so it must not bind the static guest
`metaflux_pci` resource driver. The synthetic MetaFlux identity is not a vendor
identity, and no vendor driver is matched by this module.

A synthetic vendor identity (for example `identity=nvidia`) is a presentation
disguise governed by decision-0008 and qualified in work-item-0.1.2.4. It must select
only `metaflux_pci` in kernel space before device publication, is never eligible
for vendor-driver matching, and never implements vendor-private RM/UVM behavior.
Failed deterministic pre-bind selection publishes no device; a failed MetaFlux
probe never falls through to vendor matching. Release promotion additionally
requires the registration and legal review recorded in the milestone-0.1.2.0 decisions.
