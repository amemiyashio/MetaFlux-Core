---
id: work-item-0.1.2.5
delivery: 0.1.2.5
milestone: milestone-0.1.2.0
status: Queued
area: release.lifecycle
depends_on: [work-item-0.1.2.3]
updated: 2026-08-30
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

- [ ] Re-run milestone-0.1.1.0 latency/throughput with lifecycle core enabled and archive raw
  distributions by kernel/QEMU/build fingerprint.
- [ ] Verify lifecycle-core install, upgrade, coexistence, and uninstall without
  selecting or requiring the experimental vroot package.
- [ ] Prove vendor nodes/libraries remain untouched and generic artifacts require
  no `/nix/store` runtime path.

## Exit Gate

milestone-0.1.1.0 warm-dispatch bounds remain intact and lifecycle-core throughput loss is at
most 0.5%. Both named lifecycle gates pass without building, loading, or
promoting `metaflux_vroot.ko`.
