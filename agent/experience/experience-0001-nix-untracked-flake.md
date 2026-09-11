---
id: experience-0001
status: Superseded
validated: 2026-08-27
applies_to: untracked Git worktree flakes
---

# Nix Flakes Before the First Commit

## Observation

When a flake is addressed through the Git worktree form, Nix evaluates the Git
snapshot and omits untracked files. In a repository with no first commit, normal
Git-worktree flake evaluation may therefore report that `flake.nix` is not
tracked even though it exists on disk.

## Historical practice and correction

The 2026-08-27 bootstrap used the explicit path-flake form before the first
commit. That workaround also caused Nix to act as a source snapshot and product
build orchestrator, so it is no longer a repository practice. Git owns source
identity and history; Nix only materializes pinned tools. Current commands use
the committed flake and invoke the owning build or test command directly, for
example `nix develop . --ignore-environment --keep HOME --keep USER --command ctest --preset dev`.

This record is superseded by the
[tool-provider boundary](../../toolchains/README.md#tool-provider-boundary-decision-0022).

## Evidence and revalidation

The path form was used for the 2026-08-27 pre-commit bootstrap, where the
then-current broad Nix check passed and four product packages built. That result
is recoverable from Git history and remains historical context, not current
build, test, package, or release evidence.

Revalidate tool availability when the committed flake or lock changes. See the
[bootstrap overview](../../README.md).
