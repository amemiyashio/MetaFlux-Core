---
name: ptx-simt-semantics
description: Implement or review PTX forms and canonical Kernel IR meaning with source diagnostics, exact representation legality and independent semantic oracles. Use for a new instruction/type/shape family or semantic defect; backend execution and MLIR mechanics have separate owners.
---

# PTX SIMT Semantics

For an implementation request, implement the semantic form needed by the client.
For analysis, review or benchmarking, report the representation and evidence in
that requested mode without expanding the subset. Begin at `kSupportedForms` and `Parser` in
[`parser.cpp`](../../../plugins/compat/cuda/compiler/ptx/src/parser.cpp), then
`operation_contract` and `verify_kernel` in
[`kernel_ir.cpp`](../../../compiler/core/src/kernel_ir.cpp).

Read the existing `manifest/capabilities.json`, `manifest/forms.jsonl` and
`corpus/index.json` under that frontend package root for the affected form. They bind
the PTX 9.0/sm_70 contract and exact fixture digests; do not reconstruct a new
matrix or reopen the completed initial-corpus decision on every task.

| Task | Read only the relevant guide |
| --- | --- |
| New operator/type/shape family or a missing serialized representation | [Semantic family](references/semantic-family.md) |
| Grammar, declaration, qualifier, diagnostic or support promotion | [PTX subset](references/ptx-subset.md) |
| Predicates, branches, reconvergence, CTA participation or barriers | [SIMT control](references/simt-control.md) |
| Address space, alignment, atomicity, scope or ordering | [Memory model](references/memory-model.md) |
| Exact FP/integer answers, allowed outcomes or differential evidence | [Oracle testing](references/oracle-testing.md) |

Carry parser normalization, canonical representation, verifier and oracle
together. An opcode name is not coverage: type, width, vector form, space,
qualifier, scope, modifier and target preconditions define the accepted form.
Preserve source locations for diagnostics while canonical content excludes
incidental spelling. Unsupported syntax and semantics have stable errors.

For a shape family, represent the actual dimensions/strides/control needed by
the admitted inputs. Generality comes from that representation and execution,
not another fixed-shape artifact. New rejection rows alone add no operation.
Preserve the current single-assignment, address-provenance and full-CTA barrier
invariants; target limitations never silently redefine PTX meaning.

Define expected results independently of optimized lowering and the interpreter.
Deterministic cases use exact values or a justified FP oracle; concurrency cases
use derived allowed and forbidden outcomes. Random scheduling is sampling, not
proof of a weak-memory outcome set. Undefined behavior has no upstream oracle.

Compose [$cpu-backend-performance](../cpu-backend-performance/SKILL.md) skill for
interpreter/CPU execution and [$mlir-compiler-engineering](../mlir-compiler-engineering/SKILL.md) skill
for conversion mechanics. Compose [$vulkan-spirv-compute](../vulkan-spirv-compute/SKILL.md) skill
when a Vulkan mapping is in scope. This skill owns meaning and oracle answers.

Promote a form only with parser, verifier, interpreter, lowering and differential
coverage for every backend advertising it. Select applicable interaction/negative
cases from the guides; full profile acceptance follows the active Exit Gate.
Hand source/representation changes and exact oracle evidence to
[$review](../review/SKILL.md) skill; neither a draft list nor a green unrelated
corpus establishes the new behavior.
