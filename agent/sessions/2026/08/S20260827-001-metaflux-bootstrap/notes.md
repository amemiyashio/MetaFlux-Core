# Project Work Notes

This is a reconstructed, MetaFlux-Core-only work record with date-level
precision. It summarizes repository decisions and evidence; it is not a
conversation transcript or a source of personal memory.

Known gaps:

- Exact command and project-event timestamps were unavailable, so event
  timestamps are null and sequence numbers preserve order.
- Some early patch operations and terminal output survived only as repository
  outcomes. Those events are marked `omitted` with an evidence-gap reason.
- Work was delegated across architecture, ABI/provider, Nix/build, and planning
  scopes. The final filesystem and verification are observable, while exact
  individual attribution is incomplete.
- Git had been initialized but had no commit, so `base_revision` and
  `final_revision` are null.

The authoritative latest verification is [output 0001](outputs/0001.txt). The
final summary uses the 14/14 and focused-provider test matrix.

## Recorded comparisons and decisions

- C17 was selected for providers and the client fast path to keep the
  application-side ABI and dependency closure small. C++20 remains appropriate
  for the daemon, compiler, scheduler, and backends where ownership and compiler
  integration dominate; cross-component boundaries stay C ABIs.
- Handwritten assembly was deferred because no measured compiler-codegen failure
  justified its maintenance and portability cost.
- Clang/LLVM/MLIR/LLD 22 form one compiler epoch so AOT and JIT share lowering,
  cache identity, and generated-code behavior. A cache hit must not load the
  compiler stack or contact the compiler worker.
- CMake/Ninja was retained over Meson because LLVM and MLIR expose their native
  integration through CMake packages. Nix owns dependency and environment
  reproducibility rather than replacing the project build graph.
- CUDA, future ROCm-facing interfaces, and other ecosystems are compatibility
  plugins. CPU and Vulkan are execution backends. This keeps the meta runtime
  ecosystem-neutral and permits several frontends and targets without direct
  coupling.
- NVML presentation precedes vPCI because stock `nvidia-smi` consumes NVML;
  kernel, guest transport, and lifecycle work remain separate later milestones.

Canonical consequences and acceptance gates are in the
[milestone index](../../../../plan/README.md), not duplicated in this archive.

## Unresolved at close

- Final provider glibc floor and supported distribution matrix.
- Verified LLVM 22 downstream patchset and qualification corpus.
- CUDA/NVML header acquisition and ABI-manifest update process.
- Cache ownership, quota, eviction, and multi-user isolation policy.
- CPU worker topology, NUMA policy, and reference-host performance evidence.
- The first intentional Git commit; this session has no revision anchor.
