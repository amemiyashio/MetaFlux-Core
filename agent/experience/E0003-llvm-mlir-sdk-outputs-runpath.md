---
id: E0003
status: Validated
validated: 2026-08-27
applies_to: MetaFlux compiler epoch toolchain package
---

# LLVM/MLIR SDK Outputs and RUNPATH

## Observation

A toolchain bundle that contains compiler binaries is not necessarily a usable
LLVM/MLIR SDK. Headers and CMake package files may live in development outputs,
libraries in separate library outputs, and a standalone Clang-built consumer may
need an explicit runtime search path outside a Nix development shell.

## Validated practice

- Compose the toolchain from LLVM/MLIR package, library, and development outputs.
- Include development/runtime outputs for libffi, libxml2, and zlib used by the
  exported SDK configuration.
- Link `/include`, `/lib`, `/lib/cmake`, `/bin`, and `/share` with collision
  detection enabled.
- Wrap the bundled Clang drivers with the selected GCC runtime RUNPATH.
- Qualify `LLVMConfig.cmake`, `MLIRConfig.cmake`, headers, required LLVM targets,
  an MLIR consumer link/run, the resulting RUNPATH, and a plain C compiler probe.

The implementation and executable qualification are in
[`nix/toolchains/default.nix`](../../nix/toolchains/default.nix) and the
[`toolchain-sdk` check](../../nix/checks/default.nix).

## Boundary

This RUNPATH is appropriate for the Nix-native compiler SDK output. It does not
relax the rule that provider DSOs exclude LLVM/MLIR and that generic release
artifacts have no required Nix store RUNPATH. See
[packaging ownership](../../packaging/README.md).

## Evidence and revalidation

`nix flake check path:.` passed and the `toolchain`, `runtime`, `provider`, and
`daemon` packages built at checkpoint
[P20260827-001](../progress/checkpoints/2026/P20260827-001-engineering-bootstrap-baseline.md).
Revalidate on every compiler epoch or Nix output-layout change.

