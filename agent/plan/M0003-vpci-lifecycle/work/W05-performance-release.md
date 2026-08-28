---
id: M0003-W05
milestone: M0003
status: Queued
area: release.lifecycle
depends_on: [M0003-W03]
updated: 2026-08-28
---

# Lifecycle Performance and Release

## Outcome

Prove that enabling the lifecycle coordinator does not disturb the qualified
M0002 data path. Experimental vroot packaging, performance, and promotion remain
entirely in M0003-W04 so this core release does not depend on vroot.

Required outputs or equivalent checks are:

```text
checks.x86_64-linux.lifecycle-local
checks.x86_64-linux.lifecycle-qemu
```

Generic DKMS builds against the target kernel tree; NixOS outputs are
exact-kernel packages. Deployment supplies module-signing keys; installation
never creates or enrolls one. udev, systemd, socket, QMP, and guest
fixtures are reproducible; kernel wrappers are compile/API probed.

## Work

- [ ] Re-run M0002 latency/throughput with lifecycle core enabled and archive raw
  distributions by kernel/QEMU/build fingerprint.
- [ ] Verify lifecycle-core install, upgrade, coexistence, and uninstall without
  selecting or requiring the experimental vroot package.
- [ ] Prove vendor nodes/libraries remain untouched and generic artifacts require
  no `/nix/store` runtime path.

## Exit Gate

M0002 warm-dispatch bounds remain intact and lifecycle-core throughput loss is at
most 0.5%. Both named lifecycle checks pass without building, loading, or
promoting `metaflux_vroot.ko`.
