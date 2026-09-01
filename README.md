# MetaFlux Core

MetaFlux Core is a low-overhead compatibility and execution substrate for
unmodified accelerator applications. The repository is currently in its
engineering-bootstrap phase; the buildable targets are boundary fixtures, not
functional CUDA or NVML providers.

Agents start at [`AGENTS.md`](AGENTS.md) before making any change.

## Bootstrap

Enable the repository-local Git gate once per worktree:

```sh
git config core.hooksPath .githooks
```

Nix pins and materializes repository tools. CMake owns the project build graph,
Ninja executes it, CTest owns test execution, and Clang/LLD compile and link
userspace code. Invoke each owning command directly through the development
shell:

```sh
nix develop . --command cmake --preset dev
nix develop . --command cmake --build --preset dev
nix develop . --command ctest --preset dev
```

The Linux kernel modules introduced by milestone-0.1.1.0 / `v0.1.1` are built separately by
the target kernel's Kbuild environment.

MetaFlux project Agent context starts at [`agent/README.md`](agent/README.md),
including project memory, progress, plans, validated experience, and work
records. Architecture records remain authoritative under `docs/architecture/`,
and ABI/UAPI definitions remain authoritative under `contracts/`. The directory
taxonomy and dependency map are in
[`docs/architecture/repo-layout.md`](docs/architecture/repo-layout.md).
