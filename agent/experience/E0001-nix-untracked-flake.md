---
id: E0001
status: Validated
validated: 2026-08-27
applies_to: untracked Git worktree flakes
---

# Nix Flakes Before the First Commit

## Observation

When a flake is addressed through the Git worktree form, Nix evaluates the Git
snapshot and omits untracked files. In a repository with no first commit, a
normal `nix develop .` or `nix flake check .` may therefore report that
`flake.nix` is not tracked even though it exists on disk.

## Validated practice

Use an explicit path flake until the first commit exists:

```sh
nix develop path:.
nix flake check path:.
nix build path:.#runtime path:.#provider path:.#daemon path:.#toolchain
```

After the repository has a commit containing the flake inputs, the shorter Git
flake form may be used. Do not stage files merely to change Nix discovery without
understanding the resulting Git state.

## Evidence and revalidation

The path form was used for the 2026-08-27 bootstrap, where `nix flake check`
passed and all four packages built. The repository still had no commit and all
files were untracked at checkpoint
[P20260827-001](../progress/checkpoints/2026/P20260827-001-engineering-bootstrap-baseline.md).

Revalidate after the first commit and whenever the flake source model changes.
See the [bootstrap overview](../../README.md) and
[Nix filesets](../../nix/lib/source.nix).

