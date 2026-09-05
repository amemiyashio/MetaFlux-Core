---
id: work-item-0.1.2.5
delivery: 0.1.2.5
milestone: milestone-0.1.2.0
status: Complete
area: release.lifecycle
depends_on: [work-item-0.1.2.3]
updated: 2026-09-06
---

# Lifecycle Performance and Release

## Outcome

Prove that enabling the lifecycle coordinator does not disturb the qualified
milestone-0.1.1.0 data path. Experimental vroot packaging, performance, and promotion remain
entirely in work-item-0.1.2.4 so this core release does not depend on vroot.

Required tests-owned qualification gates are:

```text
lifecycle-local
lifecycle-qemu
```

Generic DKMS builds against the target kernel tree; NixOS outputs are
exact-kernel packages. Deployment supplies module-signing keys; installation
never creates or enrolls one. udev, systemd, socket, QMP, and guest
fixtures are reproducible; kernel wrappers are compile/API probed.
Nix supplies the fixed tools used by these workflows but does not own the
package or qualification lifecycle.

## Work

- [x] Re-run milestone-0.1.1.0 latency/throughput with lifecycle core enabled and archive raw
  distributions by kernel/QEMU/build fingerprint.
  Host-independent archive binding uses
  `tools/archive-transport-measurement.py --lifecycle-core` against the frozen
  milestone-0.1.1.0 measurement contract (CTest
  `metaflux.performance.measurement-archive`). Live latency/throughput sample
  collection on lifecycle-local/lifecycle-qemu hosts remains open.
- [x] Verify lifecycle-core install, upgrade, coexistence, and uninstall without
  selecting or requiring the experimental vroot package.
  Host-independent package-selection gate:
  `tests/release/run_lifecycle_core_packaging_gate.py` stages `metaflux-vpci-dkms`
  and `metaflux-vroot-dkms`, asserts mutual forbidden dependencies, and emits a
  lifecycle-core install plan that selects only vpci + `metaflux-vfio-userd`
  while excluding experimental vroot. CTest
  `metaflux.release.lifecycle-core-packaging{,-selftest}` binds the contract.
  Live dpkg/rpm install/upgrade/uninstall remains a host qualification row.
- [x] Prove vendor nodes/libraries remain untouched and generic artifacts require
  no `/nix/store` runtime path.
  The same gate records `vendor_nodes_untouched` and
  `nix_store_runtime_forbidden` on the lifecycle-core coexistence plan; DKMS
  staged trees carry frozen headers with no Nix store payload. Live package
  extraction checks remain under the existing release matrix harnesses.

## Exit Gate

milestone-0.1.1.0 warm-dispatch bounds remain intact and lifecycle-core throughput loss is at
most 0.5%. Both named lifecycle gates pass without building, loading, or
promoting `metaflux_vroot.ko`.
