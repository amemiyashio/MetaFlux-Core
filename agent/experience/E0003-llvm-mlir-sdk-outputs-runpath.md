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

The tool materialization implementation is in
[`nix/toolchains/default.nix`](../../nix/toolchains/default.nix). Executable
qualification belongs to the toolchain's version and standalone SDK-consumer
probes, not a project build or release check.

## Boundary

This RUNPATH is appropriate for the Nix-native compiler SDK output. It does not
relax the rule that provider DSOs exclude LLVM/MLIR and that generic release
artifacts have no required Nix store RUNPATH. See
[packaging ownership](../../packaging/README.md).

## Evidence and revalidation

The then-current broad Nix check passed and the toolchain plus three product
packages built at checkpoint
[P20260827-001](../progress/checkpoints/2026/P20260827-001-engineering-bootstrap-baseline.md).
The product-package portion is historical context and is not current product
build or release evidence. Revalidate the toolchain itself with version probes
and the LLVM/MLIR consumer check on every compiler epoch or Nix output-layout
change; revalidate products through their CMake, CTest, and packaging owners.
