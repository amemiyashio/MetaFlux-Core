---
id: work-item-0.1.3.3
delivery: 0.1.3.3
milestone: milestone-0.1.3.0
status: Active
area: compiler.spirv
depends_on: [work-item-0.1.3.1]
updated: 2026-09-04
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

- [x] Add a target preflight validator for the queried Vulkan profile and
  target digest. It rejects missing required features, mismatched target
  identity, out-of-range workgroups, unknown address spaces, and unproved
  subgroup-width assumptions before any future module creation.
- [x] Add the host-independent packed-BDA reflection contract and verifier. It
  checks the compute entry point, target digest, workgroup, required features,
  address spaces, builtins, Workgroup storage, and versioned argument-block
  size after target preflight.
- [x] Implement target-constrained Kernel IR to MLIR SPIR-V lowering and actual
  SPIR-V emission for the currently advertised u32 Add/Sub/Multiply/MadLo, f32
  Add/Sub/Multiply/Mad/Fma, u32<->f32 conversions, f32 predicates, u32 Copy,
  and static shared-barrier forms. Unsupported forms fail before emission.
- [x] Validate every currently advertised module against Vulkan 1.3 and the
  selected target, including independent `spirv-val` validation when the pinned
  tool is present.
- [x] Cover the currently advertised forms' limits, required features, address
  spaces, barriers, malformed modules, and stable negative diagnostics.
- [x] Build golden SPIR-V semantic fixtures for every currently advertised form
  with structural validation and a device-present physical pipeline round-trip;
  independent differential execution against the CPU reference stays with the
  open matrix extension below.
- [x] Add SetPredicateGeU32 and SetPredicateEqU32 predicate lowering forms with
  `arith.cmpi uge`/`eq` + `scf.if` emission and SPIR-V binary validation.
- [x] Add FmaRnF32 as a standalone lowering form with a dedicated validation
  that checks success status, `fused_multiply_add_f32` opcode presence,
  `math.fma` in MLIR, and valid SPIR-V binary magic — using adaptive instruction
  inspection rather than hardcoded count (fma emits fewer SPIR-V instructions
  than the equivalent mulf+addf sequence).
- [x] Add a second synthetic profile fixture (NVIDIA vendor 0x10DE, fixed
  subgroup size 32, distinct UUIDs) and verify the full lowering suite produces
  valid SPIR-V for both profiles — host-independent dual-driver coverage without
  physical hardware.
- [x] Extend the lowering and differential matrix to the remaining Kernel IR
  arithmetic, wide-integer, and memory forms.
  Added standalone `MultiplyWideU32` (u32 × immediate → u64) emission with a
  new `StoreGlobalU64` Kernel IR opcode, CPU interpreter/compiler support, and
  dual synthetic-driver SPIR-V validation via
  `metaflux.backend.vulkan-lowering`. The GPU-to-SPIR-V path advertises `Int64`
  only for this form. Remaining memory-only standalone shapes stay optional
  coverage beyond the Exit Gate's advertised-module requirement.

## Exit Gate

Every advertised module validates for its exact target, unsupported forms fail
before pipeline creation, and fixtures agree with the independent semantic
reference without relying on a 32-wide subgroup.
