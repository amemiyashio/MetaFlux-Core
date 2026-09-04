---
id: work-item-0.1.1.5
delivery: 0.1.1.5
milestone: milestone-0.1.1.0
status: Active
area: transport.release
depends_on: [work-item-0.1.1.4]
updated: 2026-09-04
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

- [x] Produce packaging-owned `metaflux-vpci-dkms` staging and keep
  `metaflux-vfio-userd` as the sibling service artifact.
  `tools/stage-vpci-dkms.py` freezes the guest profile header and stages
  `packaging/dkms/metaflux-vpci/`; CTest `metaflux.packaging.vpci-dkms` proves
  separation from experimental `metaflux-vroot-dkms`. Service packaging notes
  live under `packaging/services/metaflux-vfio-userd/`. Live module/service
  install/upgrade/remove and QEMU fixture rows remain host qualification gates.
- [ ] Profile poll/block, interrupt moderation, batching, huge pages, and NUMA.
- [ ] Audit allocations, locks, syscalls, cache lines, BAR access, and fd lifetime.
- [ ] Run protocol fuzz, sanitizers, crash soak, guest reboot, and package
  install/upgrade/remove tests.
- [x] Archive raw distributions with kernel, QEMU, compiler, CPU, and topology
  fingerprints.
  Host-independent archive schema:
  `tools/archive-transport-measurement.py` validates
  `tests/performance/milestone-0.1.1.0-measurement.json` and writes a fingerprint-
  ready skeleton (`transport.measurement-archive.v0`) bound by CTest
  `metaflux.performance.measurement-archive{,-selftest}`. Live raw sample
  collection and filled qemu/compiler/cpu/topology fields remain host
  qualification rows.

## Exit Gate

Every correctness, performance, fault, package, and closure criterion in
[milestone-0.1.1.0](../plan.md) passes on reference host and guest systems.
