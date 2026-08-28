# MetaFlux Core

MetaFlux Core is a low-overhead compatibility and execution substrate for
unmodified accelerator applications. The repository is currently in its
engineering-bootstrap phase; the buildable targets are boundary fixtures, not
functional CUDA or NVML providers.

## Bootstrap

The authoritative development environment is Nix. CMake describes project
targets, Ninja executes the build graph, and Clang/LLD compile and link userspace
code.

```sh
nix develop path:.
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

The Linux kernel modules introduced in v0.2 are built separately by the target
kernel's Kbuild environment.

MetaFlux project Agent context starts at [`agent/README.md`](agent/README.md),
including project memory, progress, plans, validated experience, and work
records. Architecture records remain authoritative under `docs/architecture/`,
and ABI/UAPI definitions remain authoritative under `contracts/`.
