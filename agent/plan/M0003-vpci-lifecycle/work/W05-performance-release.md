---
id: M0003-W05
milestone: M0003
status: Queued
area: release.lifecycle
depends_on: [M0003-W03]
updated: 2026-08-27
---

# Lifecycle Performance and Release

## Outcome

Prove that enabling the lifecycle coordinator does not disturb the qualified
M0002 data path; report vroot overhead separately for its experimental promotion.

Required outputs or equivalent checks are:

```text
packages.x86_64-linux.metaflux-vpci-dkms
packages.x86_64-linux.metaflux-vpci-launcher
checks.x86_64-linux.baremetal-vpci
checks.x86_64-linux.lifecycle-local
checks.x86_64-linux.lifecycle-qemu
```

Generic DKMS builds against the target kernel tree; NixOS outputs are
exact-kernel packages. Deployment supplies module-signing keys; installation
never creates or enrolls one. udev, systemd, namespace, socket, QMP, and guest
fixtures are reproducible; kernel wrappers are compile/API probed.

## Work

- [ ] Re-run M0002 latency/throughput with lifecycle core enabled and archive raw
  distributions by kernel/QEMU/build fingerprint.
- [ ] Separately measure vroot, config/sysfs/`lspci`, and 1 Hz `nvidia-smi`.
- [ ] Verify install, upgrade, coexistence, signing, namespace, and uninstall.
- [ ] Prove vendor nodes/libraries remain untouched and generic artifacts require
  no `/nix/store` runtime path.

## Exit Gate

M0002 warm-dispatch bounds remain intact; lifecycle-core throughput loss is at
most 0.5%; vroot loss is also at most 0.5% before its separate promotion; 1 Hz
`nvidia-smi` stays inside the M0001 compute-impact budget; steady-state launch
never enters `metaflux_vroot.ko`.
