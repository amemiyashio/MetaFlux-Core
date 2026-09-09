# MetaFlux Core

MetaFlux Core is a low-overhead compatibility and execution substrate for
unmodified accelerator applications. The compatibility layer already presents a
functional CUDA Driver and NVML ecosystem plus a bounded cuBLAS profile on top
of its own execution backends: the CPU backend runs unmodified PTX kernels
through interpreter, cold-JIT, warm-JIT, and AOT paths, and the Vulkan backend
executes SPIR-V compute on physical adapters. Measured evidence lives beside
each owning work item.

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

MetaFlux project Agent context starts at [`agent/README.md`](agent/README.md):
project memory, milestone plans, and validated experience. Architecture records
remain authoritative under `docs/architecture/`, and ABI/UAPI definitions
remain authoritative under `contracts/`. The directory taxonomy and dependency
map are in
[`docs/architecture/repo-layout.md`](docs/architecture/repo-layout.md).
Research-only upstream source pointers live under [`references/`](references/README.md)
as exact, on-demand submodule gitlinks; ordinary builds and qualification do
not materialize them.

Containers for release qualification run under podman through
`nix develop .#release`; `docker/` is the approved home for their build
contexts. Workspace scratch — CMake trees, debug-kernel overlays, measurement
dumps, and retained work directories — lives under [`tmp/`](tmp/README.md)
(decision-0042). Installed compiler and AOT caches stay at
`/var/cache/metaflux/compiler` and `/var/lib/metaflux/aot`. The numbers and
the recipe land in the owning plan record; generated files under `tmp/` are
never committed.
