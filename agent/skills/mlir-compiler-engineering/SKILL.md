---
name: mlir-compiler-engineering
description: Design or review Kernel IR to MLIR boundaries, ODS operations, verifiers, Dialect Conversion, TypeConverter use, pass pipelines, LLVM or SPIR-V target lowering, compiler epochs, caches, diagnostics, and reproducers. Use for M0001 or M0004 compiler engineering. Do not use to redefine PTX semantics or own target runtime behavior.
---

# MLIR Compiler Engineering

## Inputs

- The active milestone/work item, compiler epoch descriptor, Kernel IR schema,
  and the exact source and target semantic contracts.
- A minimal input module/reproducer, current pass pipeline, target triple or
  SPIR-V target environment, helper/backend ABI, and cache-key definition.
- Existing verifier, conversion legality, diagnostics, and differential evidence
  for the affected operation family.

Do not persist an MLIR API assumption without checking the pinned epoch. Online
latest documentation is design guidance, not proof of LLVM/MLIR 22.1.8 behavior.

## Routing

- Use [Kernel IR boundary](references/kernel-ir-boundary.md) for ownership,
  operation invariants, ODS, and verifier placement.
- Use [dialect conversion](references/dialect-conversion.md) for legality,
  `TypeConverter`, materialization, and conversion patterns.
- Use [target lowering](references/target-lowering.md) for CPU/LLVM and
  Vulkan/SPIR-V pipeline separation.
- Use [debugging and versioning](references/debugging-and-versioning.md) for pass
  diagnostics, crash reproducers, epochs, artifacts, and cache identity.
- Route source meaning to `$ptx-simt-semantics`, CPU target policy to
  `$cpu-backend-performance`, and Vulkan target/runtime constraints to
  `$vulkan-spirv-compute`.

## Workflow

1. State the semantic contract at the Kernel IR boundary and identify which
   invariants are structural, locally verifiable, region-wide, or target-specific.
2. Define operations/types/attributes declaratively where practical; add custom
   verification only for relationships ODS cannot express clearly.
3. Write the conversion target first: legal, dynamically legal, and illegal
   dialects/operations. Define type conversion and all source/target
   materializations before patterns.
4. Decompose lowering into named passes with one responsibility, explicit
   prerequisites, preserved analyses, stable diagnostics, and deterministic
   ordering.
5. Keep CPU and Vulkan target branches separate after shared canonicalization.
   CPU lowers to LLVM/PIC ELF; Vulkan lowers to the MLIR SPIR-V dialect with an
   exact target environment and never through LLVM IR.
6. Add verifier, pass, conversion, and end-to-end tests including deliberately
   illegal IR. Capture a minimal reproducer for crashes or nondeterminism.
7. Namespace every artifact/cache entry by compiler epoch, schema, pipeline,
   target, helper/backend ABI, semantic policy, and content. Treat MLIR bytecode
   and LLVM IR as epoch-local intermediates, not durable cross-version formats.

## Output

Return or implement:

- A boundary/invariant table and affected operation/type definitions.
- An ordered pass pipeline with legality and type-conversion contracts.
- Target-specific lowering and cache-key changes, with explicit unsupported
  diagnostics.
- Minimal reproducers plus verifier, conversion, differential, and artifact
  evidence tied to the pinned compiler epoch.

## Verification

- Run operation/verifier tests, pass-unit tests, conversion tests, and full
  pipeline tests for both valid and invalid modules in scope.
- Enable verifier-after-pass and diagnostic/reproducer facilities in debug or CI
  qualification; inspect IR at the first divergent pass, not only final output.
- For CPU, verify LLVM translation, target data layout, PIC relocation policy,
  helper ABI, object loading, and interpreter/JIT/AOT agreement.
- For Vulkan, verify `spirv.target_env`, SPIR-V serialization, `spirv-val`,
  reflection, enabled features/limits, and no LLVM-IR detour.
- Confirm warm cache loading invokes no compiler framework and that an epoch,
  pipeline, target, or ABI change causes a cache miss.
