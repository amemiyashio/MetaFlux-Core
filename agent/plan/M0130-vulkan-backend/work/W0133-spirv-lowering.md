---
id: W0133
delivery: 0.1.3.3
milestone: M0130
status: Queued
area: compiler.spirv
depends_on: [W0131]
updated: 2026-08-30
---

# Target-Constrained SPIR-V Lowering

## Outcome

Lower Kernel IR through shared MLIR arith/scf/memref/vector/gpu dialects into the
MLIR SPIR-V dialect with an explicit `spirv.target_env`; validate/reflection-check
the result against the exact logical-device configuration. LLVM IR is not an
intermediate for this path.

Required semantic mappings include CTA to Workgroup, `threadIdx` to
`LocalInvocationId`, block/grid values to their Vulkan builtins, static shared
memory to Workgroup storage, explicit execution/memory scopes and
semantics for barriers, validated generation-bound buffer device addresses, and
versioned packed scalar/pointer arguments. The PTX/Kernel IR manifest is the
source of truth: parser, verifier, CPU/native reference, lowering, and execution
tests must cover a complete type/qualifier form before it is advertised.

Warp-dependent operations, unverified subgroup vote/shuffle/reconvergence,
unsupported atomic scopes, and unsupported FP semantics fail explicitly before
pipeline creation. They never execute approximately or trigger per-kernel CPU
fallback inside a Vulkan context.

## Work

- [ ] Implement target-constrained Kernel IR to MLIR SPIR-V lowering and packed
  BDA reflection/verification.
- [ ] Validate every module against Vulkan 1.3 and the actually enabled target.
- [ ] Cover FP edges, limits, missing features, address spaces, barriers,
  malformed modules, and stable negative diagnostics.
- [ ] Build differential fixtures for every advertised semantic form.

## Exit Gate

Every advertised module validates for its exact target, unsupported forms fail
before pipeline creation, and fixtures agree with the independent semantic
reference without relying on a 32-wide subgroup.
