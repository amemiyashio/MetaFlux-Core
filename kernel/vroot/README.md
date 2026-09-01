# Software PCI Root

Planned home of the default-off experimental `metaflux_vroot.ko` presentation
module. It exposes only validated config-space identity and lifecycle; execution
continues through canonical MetaFlux queues and nodes.

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

This model does not claim the kernel `pci_host_bridge`, sysfs/uevent, module
signing, or Linux 6.12/6.18 qualification gates. Those remain the next
kernel-owned integration stage.

A synthetic vendor identity (for example `identity=nvidia`) is a presentation
disguise governed by decision-0008 and qualified in work-item-0.1.2.4. It must select
only `metaflux_pci` in kernel space before device publication, is never eligible
for vendor-driver matching, and never implements vendor-private RM/UVM behavior.
Failed deterministic pre-bind selection publishes no device; a failed MetaFlux
probe never falls through to vendor matching. Release promotion additionally
requires the registration and legal review recorded in the milestone-0.1.2.0 decisions.
