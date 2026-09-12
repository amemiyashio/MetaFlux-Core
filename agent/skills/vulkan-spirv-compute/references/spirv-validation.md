# SPIR-V Validation and Reflection

## Actual emission boundary

Start at `lower_kernel` and `emit_actual_spirv` in
[`lowering.cpp`](../../../../plugins/backend/vulkan/compiler/src/lowering.cpp).
The former can map semantic forms and reflection before the latter accepts an
actual bounded kernel shape. Follow `actual_mlir_source`, target-environment
attachment, the pass pipeline and SPIR-V serialization. A successful preflight
map with rejected actual emission is still unsupported execution.

For a requested family, extend the real emission branch with its representation
legality and precise failure diagnostic, then execute the serialized kernel on
each advertising device row. Compose
[$mlir-compiler-engineering](../../mlir-compiler-engineering/SKILL.md) skill for
conversion mechanics and [$ptx-simt-semantics](../../ptx-simt-semantics/SKILL.md) skill
for independent expected semantics. Do not use a generated SPIR-V file or compile
counter as the completion criterion for a client operation.

## Required mapping checks

- CTA to Workgroup and thread/block/grid values to the matching Vulkan builtins.
- Static shared storage to Workgroup storage with valid layout and limits.
- Barriers to explicit execution scope, memory scope, and memory semantics.
- Atomics to supported storage class, width, operation, scope, and semantics.
- Generation-bound pointers to validated buffer device addresses.
- Versioned packed scalar/pointer arguments with deterministic offsets,
  alignment, width, endianness, and reflection metadata.
- FP operations/modes to the selected capability policy without silent weakening.

## Validation pipeline

1. MLIR conversion proves target legality against the explicit target environment.
2. Serialize canonical SPIR-V and run the pinned SPIR-V validator for the exact
   Vulkan environment.
3. Reflect entry point, execution modes, descriptor/BDA interface, workgroup
   memory, builtins, capabilities/extensions, and packed argument layout.
4. Cross-check reflection against Kernel IR metadata, runtime enabled features,
   device limits, and cache key before shader-module creation.
5. Emit a stable source-related capability diagnostic for unsupported semantics;
   never defer a known mismatch to opaque pipeline creation failure.

Negative fixtures cover malformed modules, target-version mismatch, undeclared
capabilities/extensions, invalid storage classes, excessive workgroup/storage
limits, unsupported scope/order/FP, BDA alignment/range, reflection mismatch, and
any subgroup-size assumption.

Choose negatives for the changed interface and preserve the existing exact
validator/device gates once per formal phase. Required physical rows need
device/driver identity and real submission/completion; an unavailable-device
skip or software ICD is separate diagnostic evidence. Reuse unchanged
capability/reflection fixtures rather than rebuilding a new matrix by default.

Primary source: [Vulkan SPIR-V environment](https://docs.vulkan.org/spec/latest/appendices/spirvenv.html).
