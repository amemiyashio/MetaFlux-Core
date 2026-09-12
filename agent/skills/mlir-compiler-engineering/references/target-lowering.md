# Target Lowering

## Ownership rule

This reference owns MLIR framework mechanics: conversion targets and legality,
`TypeConverter` and materializations, pattern/pass structure, and deterministic
pipeline plumbing. The target backend owns target policy, the target-lowering
implementation and source placement, target environment and validation criteria,
and runtime integration. Any target-branch change composes this skill with the
corresponding backend skill; conversion review does not transfer backend
ownership to MLIR.

## CPU branch

```text
verified Kernel IR -> MlirEmitter LLVM-dialect text -> parse/verify MLIR
                   -> LLVM IR -> declared optimization -> PIC ELF -> cache -> loader
```

Start at `compile_kernel` and `MlirEmitter` in
[`compiler.cpp`](../../../../plugins/backend/cpu/compiler/src/compiler.cpp).
`operation_phases`/`segment_phase` split CTA semantics and scalar/vector regions;
`emit_vector_region` emits the actual vector operations. This branch directly
emits LLVM dialect, with the qualified O2 pipeline as the normal optimization
path. It has no required shared `arith/scf/gpu` conversion prefix.
For a missing opcode/type, follow emitter legality and parsed MLIR types to
`mlir::verify`, LLVM translation and the final entry ABI before changing passes.

Carry target triple, data layout, CPU name/canonical features, FP/overflow
policy, helper ABI, relocation/code model, optimization level, and cancellation
policy. Translation to LLVM IR is not the semantic oracle. Verify the resulting
object and loader boundary against the backend C ABI.

[$cpu-backend-performance](../../cpu-backend-performance/SKILL.md) skill owns CPU SIMT mapping, feature/codegen policy, the CPU
target-lowering implementation and its source placement, and object/runtime
qualification. This skill owns the MLIR conversion legality, type plumbing, and
pass mechanics used by that implementation.

Primary source: [MLIR LLVM IR target](https://mlir.llvm.org/docs/TargetLLVMIR/).

## Vulkan branch

```text
verified Kernel IR -> lower_kernel preflight/reflection -> emit_actual_spirv
                   -> actual MLIR source + target passes -> SPIR-V
                   -> validation/reflection cross-check -> pipeline cache
```

Start at `lower_kernel`, `emit_actual_spirv` and `actual_mlir_source` in
[`lowering.cpp`](../../../../plugins/backend/vulkan/compiler/src/lowering.cpp).
The emitter currently recognizes bounded complete kernel shapes and constructs
the selected MLIR module/pipeline using registered arith, func, GPU, math, memref,
SCF and SPIR-V dialects. A semantic preflight/reflection mapping is not proof
that the actual emitter supports the kernel: inspect its accepted branch and
serialized output. A new family must reach that branch and execute on the
advertising device; do not add only a nominal opcode mapping.

Do not route this branch through LLVM IR. Materialize Vulkan/SPIR-V capabilities,
extensions, versions, limits, addressing/memory model, storage classes, builtins,
execution modes, scopes, and memory semantics from one exact target environment.
Validate packed BDA arguments and reflection before pipeline creation.

[$vulkan-spirv-compute](../../vulkan-spirv-compute/SKILL.md) skill owns the target environment, capability policy,
target-lowering implementation and its source placement, SPIR-V validation, and
Vulkan runtime integration. This skill owns the MLIR conversion legality, type
plumbing, and pass mechanics used by that implementation.

Primary source: [MLIR SPIR-V dialect](https://mlir.llvm.org/docs/Dialects/SPIR-V/).

## Sharing and evidence

Share `arith`, `scf`, `memref`, `vector` or `gpu` abstractions only where an actual
change calls for them and they preserve the neutral source contract. Define
input/output legality and target-independent assumptions for any new shared
pass. Do not create a shared architecture as an incidental prerequisite to a
bounded emitter fix.

For CPU, preserve interpreter/compiled corpus differential coverage and the
pipeline, PIC-ELF/readelf and SIMD/objdump gates applicable to the change. For
Vulkan, preserve exact-environment validation, reflection negatives and actual
device execution for affected advertised forms. A compiler artifact or cache
hit is not client execution evidence. Use minimized compiler checks while
implementing; the review/verification workflow selects formal covering gates
once per phase, retaining required gate dependencies and independent integration.
