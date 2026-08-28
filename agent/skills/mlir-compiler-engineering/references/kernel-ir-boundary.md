# Kernel IR Boundary

Kernel IR is the ecosystem-neutral semantic handoff. PTX parsing and meaning are
owned before it; target selection and execution are owned after it. MLIR is an
epoch-local implementation framework inside compiler workers and does not cross
the provider, transport, or backend C ABI.

## Invariant placement

- Put syntax/source-language checks in the frontend.
- Put operation-local type, attribute, region, successor, and symbol invariants
  in ODS traits/constraints or the operation verifier.
- Put cross-operation dominance, memory-space, capability, and whole-module
  relationships in explicit verification passes.
- Put target feature/limit checks at target conversion, while preserving enough
  semantic identity for a precise source diagnostic.
- Never rely on an LLVM/SPIR-V verifier to be the first detector of malformed
  Kernel IR.

For each operation, define operands/results, types, attributes, regions,
side-effects, memory effects, speculatability, interfaces, canonical forms,
constant-folding rules, location policy, and unsupported cases. Canonicalization
must preserve FP, overflow, ordering, barrier, and divergence semantics; an
algebraic identity is not automatically legal.

Version the serialized Kernel IR schema and provide deterministic parse/print or
another canonical representation used in content hashing. The active milestone
decides durable artifact shape; MLIR bytecode is not that durable format.

Primary design reference: [MLIR documentation](https://mlir.llvm.org/docs/).
