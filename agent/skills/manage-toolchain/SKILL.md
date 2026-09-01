---
name: manage-toolchain
description: Pin and expose MetaFlux repository tool versions while keeping Nix limited to tool materialization. Use for flake.lock, development shells, compiler epochs, SDK/header/tool manifests, mirror routing, or review of Nix scope. Do not use to own product build, test, packaging, release-evidence, source-snapshot, or host-GC semantics.
---

# Manage Toolchain

## Inputs

- Read [`toolchains/README.md`](../../../toolchains/README.md) before changing a
  version, source identity, patch, SDK, header set, or executable tool.
- Read the [Ubuntu 20.04 target SDK guide](references/ubuntu-20.04-target-sdk.md)
  whenever constructing, consuming, diagnosing, or qualifying the provider
  sysroot, complete target SDK, generic LLVM closure, or generic Linux product
  path. In particular, treat a `GLIBC_2.32` or newer result as a target-
  consumption failure, not as evidence that the declared SDK is absent.
- Inspect the affected manifest, `flake.lock`, `flake.nix`, and only the Nix
  toolchain or shell files that materialize that declaration.
- Read the active task only to learn which fixed tool it requires. A milestone
  is never the canonical definition of the repository toolchain.

## Routing

- Startup resolution is Nix-first. Before any repository executable or
  tool/version/capability probe, enter the Git-aware environment with
  `nix develop . --command ...`. Do not inspect ambient `PATH`, use
  `which`/`command -v`, or run a host executable to decide whether the Nix
   declaration is needed. Host `git` and `nix` are the only bootstrap
   executables; repository file APIs may read tracked text directly.
- A newly required repeatable tool is added to the narrow repository Nix
  declaration before use. This skill owns proving a Nix
  provision/materialization gap; after that proof it composes
  `manage-host-privilege`, which alone owns host package mapping, privilege,
  installation, and authorization. The installed host copy is a local
  prerequisite and does not become Nix-owned declared identity or
  repeatable/release evidence.
- Git owns source identity and history.
- CMake and Ninja own configure, build, install, and build-directory behavior.
- CTest and repository scripts own tests and qualification.
- `packaging/` owns package construction; Git and owning test harnesses retain
  accepted outcomes, while the invoking work unit cleans temporary artifacts.
- Nix owns only locked input resolution, exact tool materialization, and the
  development shell that exposes those tools. It does not wrap or duplicate the
  owners above, encode their commands or policies, archive evidence, snapshot
  project source, govern Agent execution, install host software, or configure
  host GC. `nix develop . --command TOOL ...` is an environment-entry
  boundary; `TOOL` and its owning repository workflow retain all semantics.
- A fixed tool is not permanently frozen. Its current version, source, patches,
  hashes, and exposure are explicit and reproducibly stable for the repository
  revision. Later change uses this skill to update the canonical manifest and
  lock, then revalidates the affected consumers without ambient drift.
- Entering `nix develop .#release` proves only that the release tools are
  available. It does not select the Ubuntu 20.04 target SDK, target triple,
  unwrapped compiler, startup objects, target linker, or generic CMake package
  roots; the CMake-owned target entry point must select those explicitly.
- decision-0009 is a hard compatibility floor for generic Linux artifacts: materialize
  and expose the Ubuntu 20.04 target SDK with glibc 2.31. The host distribution,
  host glibc, and a development-shell compiler wrapper must never raise or
  redefine that floor.
- When downloading any declared tool or immutable input, prefer a mirror selected
  for the current execution environment's configured timezone before trying an
  adjacent-timezone mirror or canonical upstream.
- Materialize framework clients as exact, named, on-demand profiles. Keep each
  profile's Python, framework wheel, CUDA user-space closure, and immutable
  digests internally consistent; never mix dependencies across profiles or add
  them to the default, provider, runtime, or release shell. A framework profile
  is a test client, not evidence for product capability or generic Linux ABI.
- Route compiler semantics, provider ABI, runtime behavior, and target tuning to
  their domain skills. A tool version change does not transfer those decisions
  to this skill.
- Route every `sudo`, `su`, root-helper, host package, and privileged driver
  operation to `manage-host-privilege`. This skill neither defines nor executes
  privilege commands and never owns credentials or persistent grants.

## Workflow

1. Change the canonical manifest first. Keep only portable version, upstream
   identity, patch digest, target, and immutable input information. Exclude
   `/nix/store` paths, NARs of local outputs, qualification counts, task-local
   profiles, mirror observations, and build results.
2. Update `flake.lock` or the narrow Nix tool derivation only as needed to
   materialize the declared version. Keep project packages, CMake option maps,
   CTest invocations, packaging, and evidence generation out of Nix.
3. For a generic Linux compile or link, expose and use the pinned unwrapped
   Clang, its matching resource directory, the Ubuntu 20.04 sysroot, the target
   SDK GCC layout, and the target `ld.lld`. Require the target triple,
   `--sysroot`, and external GCC-toolchain selection explicitly. Reject a
   command when a host wrapper injects host startup objects, headers, libraries,
   loader paths, or a newer glibc symbol version.
4. Apply that same target tuple to release-side executables that make a generic
   compatibility claim, including activation launchers and acceptance fixtures.
   A host-built helper does not qualify a glibc-2.31 package even when the
   package payload itself is clean.
5. For network acquisition, try a mirror in the environment's configured
   timezone and then an adjacent timezone. Verify the canonical signature or
   frozen digest; the mirror is a transfer route, not dependency identity.
6. Use the Git flake entry point (`nix develop .` or a tool output under `.`).
   Never use `path:.`; it ignores Git's source boundary and can copy generated
   trees into the Nix store before evaluation.
7. Run every version probe and owning project command inside the declared
   development shell, for example
   `nix develop . --command cmake --preset development` or
   `nix develop . --command ctest --preset development`. Nix supplies the
   executable closure; CMake, CTest, packaging, and qualification retain command
   semantics and evidence ownership.
   A decision-0032-installed host prerequisite is invoked by exact absolute path from
   inside this entry environment; record it as local host state, never as a
   substitute for a declared repeatable tool closure.
8. Before accepting a generic artifact or release fixture, have its owning
   workflow verify the system loader, absence of RPATH/RUNPATH and Nix store
   strings, allowed `DT_NEEDED` closure, and a highest referenced glibc symbol
   no newer than `GLIBC_2.31`. Execute it in the frozen Ubuntu 20.04 row; a
   successful build or a host-only run is not compatibility evidence.
9. Remove task-owned temporary downloads and failed materialization work at the
   Iteration or integration boundary. Host Nix-store retention and GC remain operator concerns;
   do not add project GC roots, timers, thresholds, or store paths as identity.

## Output

- A small, portable manifest and matching locked materialization.
- A development shell or named tool output exposing the requested exact tools.
- An explicit Ubuntu 20.04/glibc 2.31 target tuple for every generic Linux
  consumer, without implicit host compiler-wrapper inputs.
- Concise links from Agent memory or tasks to the canonical toolchain record.
- No product derivation, duplicated build graph, source snapshot, qualification
  archive, task-specific mirror output, or GC policy.

## Verification

```sh
nix flake show .
nix develop . --command clang --version
nix develop . --command cmake --version
nix develop . --command ninja --version
nix develop .#release --command rpmbuild --version
nix develop . --command python3 tools/check-agent-state.py .
```

Then run the narrow CMake/CTest or other owner-specific gate affected by the
tool change. For a generic Linux artifact, additionally inspect its interpreter,
`DT_NEEDED`, RPATH/RUNPATH, embedded paths, and glibc symbol ceiling, then run
the Ubuntu 20.04 release row. Version probes prove provisioning only; they do
not prove product behavior or glibc-floor compatibility.
