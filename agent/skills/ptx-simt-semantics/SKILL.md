---
name: ptx-simt-semantics
description: Specify or review PTX 9.x subset parsing, Kernel IR semantics, state spaces, predication, divergent SIMT control flow, CTA barriers, atomics, memory ordering, and interpreter-oracle tests. Use for M0001 PTX semantic work. Do not use for MLIR pass implementation, CUDA ELF ABI, or target-runtime tuning.
---

# PTX SIMT Semantics

## Inputs

- The active milestone/work item, selected PTX ISA version, and exact corpus and
  instruction/capability manifest.
- Kernel IR schema/verifier rules plus parser, interpreter, native-reference,
  and differential-test artifacts affected by the task.
- Target constraints only where they determine whether a semantic form can be
  advertised; a target limitation must not silently redefine PTX.

The manifest is a product contract. An opcode name alone is not coverage: type,
width, vector form, state space, qualifiers, scope, and modifiers all matter.

## Routing

- Use [PTX subset](references/ptx-subset.md) for grammar and advertised-form
  coverage.
- Use [SIMT control](references/simt-control.md) for predicates, divergence,
  reconvergence, CTA scheduling, and barriers.
- Use [memory model](references/memory-model.md) for state spaces, addresses,
  atomics, fences, and ordering.
- Use [oracle testing](references/oracle-testing.md) for independent semantic
  evidence and diagnostics.
- Route dialect/pass mechanics to `$mlir-compiler-engineering`, CPU code shape
  and tuning to `$cpu-backend-performance`, and Vulkan capability mapping to
  `$vulkan-spirv-compute`.

## Workflow

1. Freeze the selected PTX version and enumerate supported forms in a manifest
   across syntax, types, widths, spaces, scopes, modifiers, and required target
   capabilities.
2. Parse losslessly enough to issue source-located diagnostics, then normalize
   into Kernel IR operations whose verifier states every semantic precondition.
3. Define lane, warp, CTA, grid, predicate, special-register, and address-space
   behavior independently of any optimized backend.
4. Specify divergent control with explicit active masks and reconvergence rules.
   Specify barrier participation and deadlock/error behavior for invalid paths.
5. Define memory access, alignment, visibility, atomicity, scope, and ordering;
   reject forms whose semantics cannot be represented exactly.
6. Implement or update the interpreter before optimized lowering. Keep it simple
   enough to act as an oracle, not as a copy of the MLIR/LLVM implementation.
7. Add one positive and one precise negative fixture for every advertised form,
   then differential and randomized cases for interactions.

## Output

Return or implement:

- A PTX form/capability matrix and Kernel IR semantic contract.
- State-transition descriptions for control flow, barriers, memory, and errors.
- Stable source-located diagnostics for malformed and unsupported forms.
- Oracle and differential-test evidence, including any semantic exclusions a
  backend must report rather than approximate.

## Verification

- Prove every advertised form has parser, verifier, interpreter, lowering, and
  differential coverage before promotion.
- Compare scalar reference, interpreter, CPU JIT/AOT, and available native or
  Vulkan executions without sharing the same implementation logic as the oracle.
- Exercise divergent branches, partial predicates, loop backedges, barrier
  participation, boundary addresses, alignment, overflow, atomic contention,
  and scope/order pairs.
- Reject unknown opcodes, unknown modifiers, malformed declarations, unsupported
  state spaces, and unsupported semantics with deterministic diagnostics.
- Keep the exact corpus decision open until its canonical manifest and evidence
  satisfy M0001-W03; a draft list is not a frozen compatibility claim.
