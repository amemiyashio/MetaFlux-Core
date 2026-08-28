# Target Lowering

## Ownership rule

This reference owns MLIR framework mechanics: conversion targets and legality,
`TypeConverter` and materializations, pattern/pass structure, and deterministic
pipeline plumbing. The target backend owns target policy, the target-lowering
implementation and source placement, target environment and validation criteria,
and runtime integration. Any target-branch change composes this skill with the
corresponding backend skill; conversion review does not transfer backend
ownership to MLIR.

## Shared prefix

Kernel IR may lower through shared `arith`, `scf`, `memref`, `vector`, and `gpu`
abstractions only where their semantics preserve the source contract. Record
each pass's input/output legality and target-independent assumptions.

## CPU branch

```text
Kernel IR -> shared MLIR -> SIMT-to-loop/SIMD -> LLVM dialect
          -> LLVM IR -> PIC ELF -> epoch cache -> loader
```

Carry target triple, data layout, CPU name/canonical features, FP/overflow
policy, helper ABI, relocation/code model, optimization level, and cancellation
policy. Translation to LLVM IR is not the semantic oracle. Verify the resulting
object and loader boundary against the backend C ABI.

`$cpu-backend-performance` owns CPU SIMT mapping, feature/codegen policy, the CPU
target-lowering implementation and its source placement, and object/runtime
qualification. This skill owns the MLIR conversion legality, type plumbing, and
pass mechanics used by that implementation.

Primary source: [MLIR LLVM IR target](https://mlir.llvm.org/docs/TargetLLVMIR/).

## Vulkan branch

```text
Kernel IR -> shared MLIR -> SPIR-V dialect with spirv.target_env
          -> SPIR-V -> validation/reflection -> pipeline cache
```

Do not route this branch through LLVM IR. Materialize Vulkan/SPIR-V capabilities,
extensions, versions, limits, addressing/memory model, storage classes, builtins,
execution modes, scopes, and memory semantics from one exact target environment.
Validate packed BDA arguments and reflection before pipeline creation.

`$vulkan-spirv-compute` owns the target environment, capability policy,
target-lowering implementation and its source placement, SPIR-V validation, and
Vulkan runtime integration. This skill owns the MLIR conversion legality, type
plumbing, and pass mechanics used by that implementation.

Primary source: [MLIR SPIR-V dialect](https://mlir.llvm.org/docs/Dialects/SPIR-V/).
