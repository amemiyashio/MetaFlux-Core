---
id: work-item-1.0.0.3
delivery: 1.0.0.3
milestone: milestone-1.0.0.0
status: Queued
area: release.stable
depends_on: [milestone-0.1.1.0, milestone-0.1.2.0, milestone-0.1.3.0]
updated: 2026-08-30
---

# Stable Compatibility and Release

## Outcome

Define the exact public compatibility commitment represented by `v1.0.0` and
ship one reproducible release after the Intel and physical NVIDIA gates close.

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
  accepted Intel, physical NVIDIA, compatibility, and provenance evidence.

## Exit Gate

The stable compatibility manifest is explicit, every declared prior-release
upgrade path passes, work-item-1.0.0.1 and work-item-1.0.0.2 are complete, and one verified Git revision
reproduces the signed `v1.0.0` artifacts without Nix runtime dependencies or
vendor-file replacement.
