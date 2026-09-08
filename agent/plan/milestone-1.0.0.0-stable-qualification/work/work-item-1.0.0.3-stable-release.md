---
id: work-item-1.0.0.3
delivery: 1.0.0.3
milestone: milestone-1.0.0.0
status: Queued
area: release.stable
depends_on: [milestone-0.1.1.0, milestone-0.1.2.0, milestone-0.1.3.0, milestone-0.2.0.0]
updated: 2026-09-08
---

# Stable Compatibility and Release

## Outcome

Define the exact public compatibility commitment represented by `v1.0.0` and
ship one reproducible release after the v0.2 compatibility foundation closes.

## Work

- [ ] Close and publish the exact stable public API, ABI, CLI, configuration,
  package, and persisted-state surface plus its upgrade and deprecation policy.
- [ ] Prove that unlisted provider internals, third-party version namespaces,
  experimental transports, and private vendor behavior are not implied stable.
- [ ] Run cumulative milestone-0.1.0.0-milestone-0.1.3.0 regression and the approved generic release,
  installation, upgrade, coexistence, removal, ABI, and provenance matrices.
- [ ] Reproduce DEB, RPM, and tar artifacts byte-for-byte from one clean Git
  revision and declared tool identities.
- [ ] Bind the release notes, manifests, package versions, and Git tag to the
  accepted compatibility and provenance evidence. Intel and physical NVIDIA
  qualification remain milestone-2.0.0.0 scope.

## Exit Gate

The stable compatibility manifest is explicit, every declared prior-release
upgrade path passes, milestone-0.2.0.0 is complete, and one verified Git
revision reproduces the signed `v1.0.0` artifacts without Nix runtime
dependencies or vendor-file replacement.
