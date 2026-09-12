---
name: ptx-simt-semantics
description: Specify, implement, or review PTX 9.x subset parsing, Kernel IR semantics, state spaces, predication, divergent SIMT control flow, CTA barriers, atomics, memory ordering, and deterministic or allowed-outcome semantic-oracle tests. Use whenever PTX source meaning or its oracle changes, including milestone-0.1.0.0 CPU and milestone-0.1.3.0 Vulkan paths. Do not use for CPU interpreter implementation, MLIR pass implementation, CUDA ELF ABI, or target-runtime tuning.
---

# PTX SIMT Semantics

## Implementation Focus

For an implementation request, use the shared
[implementation guidance](../main/references/implementation-guidance.md).
Select the affected inputs and obligations below; broad qualification lists
do not make every invocation a new inventory or full-suite run.

Choose the semantic form needed by the current client or backend capability.
Implement parser normalization, canonical representation and verifier/oracle
support together, composing with the backend owner for execution. Prefer a
bounded reusable form over another fixed-shape artifact when the assignment
requires shape generality. Keep unsupported forms explicit without treating
new rejection rows as newly implemented operations.

## Inputs

- The active milestone/work item, selected PTX ISA version, and exact corpus and
  instruction/capability manifest.
- Kernel IR schema/verifier rules plus parser, semantic-oracle, CPU interpreter,
  native-reference, and differential-test artifacts affected by the task.
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
- Route dialect/pass mechanics to `$mlir-compiler-engineering`, CPU interpreter
  implementation, runtime execution, code shape, and tuning to
  `$cpu-backend-performance`, and Vulkan capability mapping to
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
6. Define the semantic oracle independently of optimized lowering: exact expected
   values for deterministic cases, allowed and forbidden outcome sets for
   scheduler- or memory-sensitive cases, and explicit undefined or unsupported
   cases. Compose with `$cpu-backend-performance` for interpreter implementation;
   the interpreter executes oracle tests but does not define their answers.
7. Add one positive and one precise negative fixture for every advertised form,
   classify each result oracle, then add differential and randomized interaction
   cases. Random scheduling samples allowed behavior; it is not completeness
   evidence for a weak-memory outcome set.

## Output

Select the applicable outputs for the requested task:

- A PTX form/capability matrix and Kernel IR semantic contract.
- State-transition descriptions for control flow, barriers, memory, and errors.
- Stable source-located diagnostics for malformed and unsupported forms.
- Oracle and differential-test evidence with every case classified as
  deterministic, allowed-outcome-set, or undefined/unsupported, including any
  semantic exclusions a backend must report rather than approximate.

## Verification

- Prove every advertised form has parser, verifier, interpreter, lowering, and
  differential coverage before promotion.
- Compare deterministic scalar reference, interpreter, CPU JIT/AOT, and available
  native or Vulkan results byte-for-byte for integer cases and by the declared FP
  oracle. For scheduler- or memory-sensitive cases, require every result to be in
  the derived allowed set and assert forbidden outcomes separately.
- Keep reference models and executable implementations structurally independent.
  Back randomized litmus sampling with a normative model, exhaustive enumerator,
  or equivalent argument appropriate to the finite case.
- Exercise divergent branches, partial predicates, loop backedges, barrier
  participation, boundary addresses, alignment, overflow, atomic contention,
  and scope/order pairs.
- Reject unknown opcodes, unknown modifiers, malformed declarations, unsupported
  state spaces, and unsupported semantics with deterministic diagnostics.
- Keep the exact corpus decision open until its canonical manifest and evidence
  satisfy work-item-0.1.0.3; a draft list is not a frozen compatibility claim.
