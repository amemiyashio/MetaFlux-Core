# SPIR-V Validation and Reflection

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

Primary source: [Vulkan SPIR-V environment](https://docs.vulkan.org/spec/latest/appendices/spirvenv.html).
