---
status: Current
updated: 2026-08-27
---

# Durable Constraints

Canonical sources are [M0001](../plan/M0001-core-foundation/plan.md) and
[M0001-W01](../plan/M0001-core-foundation/work/W01-build-toolchain.md).

- Target Linux x86_64 and glibc. Kernel work uses the target kernel's Kbuild.
- Application-side providers and client fast path use C17 and keep LLVM/MLIR,
  Python, systemd, and the C++ runtime out of the provider closure.
- Runtime services, compiler code, scheduler, and execution backends use C++20.
- Cross-component plugin boundaries use versioned C ABIs. Encoded/shared/UAPI
  records follow the narrower rules in the [contracts index](../../contracts/README.md).
- Compiler epoch 1 is defined only by
  [`toolchains/compiler-epoch-1.json`](../../toolchains/compiler-epoch-1.json):
  LLVM/Clang/MLIR/LLD 22.1.8 and its pinned Nix inputs.
- Nix is the authoritative development and package environment; CMake describes
  targets and Ninja executes the build graph.
- Rust and handwritten assembly are outside compiler epoch 1 unless a later
  measured decision explicitly changes that boundary.
- The runtime and compiler core remain ecosystem-neutral. Compatibility plugins
  do not depend on concrete execution backends.
- Mutable provider state belongs to one negotiated shared view, not per-DSO
  globals. Statically embedded fast-path code remains stateless.
- Generic packages do not overwrite vendor-owned libraries or device nodes and
  do not require Nix store paths at runtime.
- Performance budgets are acceptance gates, not aspirations; canonical values
  and measurement rules live in
  [M0001](../plan/M0001-core-foundation/plan.md).

When a task would relax one of these constraints, create or update a canonical
architecture decision before implementation.
