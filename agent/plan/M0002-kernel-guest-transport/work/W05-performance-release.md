---
id: M0002-W05
milestone: M0002
status: Queued
area: transport.release
depends_on: [M0002-W04]
updated: 2026-08-29
---

# Transport Performance and Release

## Packaging and Qualification Owners

```text
packaging: metaflux-vpci-dkms, metaflux-vfio-userd
test fixtures: qemu-vfio-user, guest-test-image
qualification gates: kernel-6_12, kernel-6_18, cdev-transport, qemu-vfio-user
```

Pin one QEMU/libvfio-user pair for CI and test the newest supported stable pair
separately. Generic modules build through the exact target
`/lib/modules/$(uname -r)/build` tree; one binary is never claimed across kernels.
Generic artifacts have no required Nix store path. Reproducible inputs generate
udev/systemd/socket policy, QEMU fixture, and guest image. Compile/API probes, not
only `LINUX_VERSION_CODE`, select 6.12/6.18 compatibility wrappers.

`packaging/` owns the artifacts and `tests/` owns the fixtures and qualification
gates. Nix provides their fixed tools without constructing either workflow.

## Measurement Contract

`client enqueue` begins at provider entry before descriptor construction and ends
after publication/required doorbell. `warm dispatch` ends when the backend
acquire-load accepts it. Completion is separate. Archive busy and
empty-to-nonempty distributions independently.

## Work

- [ ] Profile poll/block, interrupt moderation, batching, huge pages, and NUMA.
- [ ] Audit allocations, locks, syscalls, cache lines, BAR access, and fd lifetime.
- [ ] Run protocol fuzz, sanitizers, crash soak, guest reboot, and package
  install/upgrade/remove tests.
- [ ] Archive raw distributions with kernel, QEMU, compiler, CPU, and topology
  fingerprints.

## Exit Gate

Every correctness, performance, fault, package, and closure criterion in
[M0002](../plan.md) passes on reference host and guest systems.
