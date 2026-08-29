---
name: manage-toolchain
description: Pin and expose MetaFlux repository tool versions while keeping Nix limited to tool materialization. Use for flake.lock, development shells, compiler epochs, SDK/header/tool manifests, mirror routing, or review of Nix scope. Do not use to own product build, test, packaging, release-evidence, source-snapshot, or host-GC semantics.
---

# Manage Toolchain

## Inputs

- Read [`toolchains/README.md`](../../../toolchains/README.md) before changing a
  version, source identity, patch, SDK, header set, or executable tool.
- Inspect the affected manifest, `flake.lock`, `flake.nix`, and only the Nix
  toolchain or shell files that materialize that declaration.
- Read the active task only to learn which fixed tool it requires. A milestone
  is never the canonical definition of the repository toolchain.

## Routing

- Git owns source identity and history.
- CMake and Ninja own configure, build, install, and build-directory behavior.
- CTest and repository scripts own tests and qualification.
- `packaging/` owns package construction; session records own compact outcomes
  and cleanup of session-owned temporary artifacts.
- Nix owns only locked input resolution, exact tool materialization, and the
  development shell that exposes those tools. It does not wrap or duplicate the
  owners above, archive evidence, snapshot project source, or configure host GC.
- When downloading any declared tool or immutable input, prefer a mirror selected
  for the current execution environment's configured timezone before trying an
  adjacent-timezone mirror or canonical upstream.
- Route compiler semantics, provider ABI, runtime behavior, and target tuning to
  their domain skills. A tool version change does not transfer those decisions
  to this skill.

## Workflow

1. Change the canonical manifest first. Keep only portable version, upstream
   identity, patch digest, target, and immutable input information. Exclude
   `/nix/store` paths, NARs of local outputs, qualification counts, task-local
   profiles, mirror observations, and build results.
2. Update `flake.lock` or the narrow Nix tool derivation only as needed to
   materialize the declared version. Keep project packages, CMake option maps,
   CTest invocations, packaging, and evidence generation out of Nix.
3. For network acquisition, try a mirror in the environment's configured
   timezone and then an adjacent timezone. Verify the canonical signature or
   frozen digest; the mirror is a transfer route, not dependency identity.
4. Use the Git flake entry point (`nix develop .` or a tool output under `.`).
   Never use `path:.`; it ignores Git's source boundary and can copy generated
   trees into the Nix store before evaluation.
5. Run a version probe inside the development shell. Run project configure,
   build, tests, packaging, or qualification directly through their owning
   tools after leaving Nix orchestration out of the command.
6. Remove task-owned temporary downloads and failed materialization work at the
   session boundary. Host Nix-store retention and GC remain operator concerns;
   do not add project GC roots, timers, thresholds, or store paths as identity.

## Output

- A small, portable manifest and matching locked materialization.
- A development shell or named tool output exposing the requested exact tools.
- Concise links from Agent memory or tasks to the canonical toolchain record.
- No product derivation, duplicated build graph, source snapshot, qualification
  archive, task-specific mirror output, or GC policy.

## Verification

```sh
nix flake show .
nix develop . --command clang --version
nix develop . --command cmake --version
nix develop . --command ninja --version
python3 tools/check-agent-records.py .
```

Then run the narrow CMake/CTest or other owner-specific gate affected by the
tool change. Version probes prove provisioning only; they do not prove product
behavior.
