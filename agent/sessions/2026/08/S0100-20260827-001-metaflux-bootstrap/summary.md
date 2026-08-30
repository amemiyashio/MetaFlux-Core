# MetaFlux Bootstrap Session

This reconstructed project record summarizes the repository work and verified
outcomes that established MetaFlux Core as a performance-first, seamless
compatibility and execution layer. The intended application experience remains Wine-like:
existing CUDA or management clients keep familiar interfaces while MetaFlux
selects local, virtualized, heterogeneous, or distributed execution behind them.

The technology policy uses C17 for application-facing providers, C++20 for the
core, stable C ABI contracts, and Clang/LLVM/MLIR 22 with LLD. CMake and Ninja
remain the build system because LLVM and MLIR are CMake-native, while Nix pins
and composes the environment. Rust was removed from the planned implementation
language set. AOT and JIT share lowering and cache infrastructure.

Planning produced M0100 for the v0.1 core and M0110 through M0130 for the split
v0.2 transport, lifecycle and vPCI presentation, and Vulkan backend work. The
monorepo separates ecosystem compatibility plugins, including CUDA and a future
ROCm family, from ecosystem-neutral runtime, compiler IR, contracts, transports,
kernel interfaces, services, and execution backends. These are plans and
qualified fixtures, not claims that the product vertical slices are complete.

The architecture review led to finer build roles, explicit provider export
surfaces, a versioned backend C API, separate PTX frontend ownership, one shared
compiler epoch descriptor, package-specific Nix source sets, enforced LLD,
release LTO, and SDK and closure checks.

Final visible verification reported 14/14 development tests, 14/14 ASan tests,
9/9 provider-preset tests, 5/5 CUDA-only tests, and 5/5 NVML-only tests. The Nix
flake gate passed, and the `runtime`, `provider`, `daemon`, and `toolchain`
packages built. Provider export and dependency qualification plus installed
runtime and toolchain SDK consumers also passed.

After the project records were integrated, the standalone validator passed and
the final Nix flake gate built the independent `agent-records` check.

The repository-local Agent area now contains MetaFlux plans, project memory,
validated engineering experience, progress, templates, and date-partitioned
project work records. The stdlib-only validator checks those records.

There are no Git revision anchors because the repository had no commit at the
time of reconstruction. See the [event log](events.jsonl), [fidelity notes](notes.md),
and [retained verification](outputs/0001.txt). The maintained roadmap is under
[Agent plans](../../../../plan/README.md).
