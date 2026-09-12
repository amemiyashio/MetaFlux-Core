# Kernel IR Boundary

Kernel IR is the ecosystem-neutral semantic handoff. PTX parsing and meaning are
owned before it; target selection and execution are owned after it. MLIR is an
epoch-local implementation framework inside compiler workers and does not cross
the provider, transport, or backend C ABI.

## Invariant placement

Current Kernel IR is a C++ representation, not a repository ODS dialect.
Begin at `operation_contract` and `verify_kernel` in
[`kernel_ir.cpp`](../../../../compiler/core/src/kernel_ir.cpp), the enums and
fields in [kernel_ir.hpp](../../../../compiler/core/include/metaflux/compiler/kernel_ir.hpp),
and the worker [encode/decode protocol](../../../../services/metafluxd/src/compiler_worker_protocol.cpp).
Compose [$compiler-worker-isolation](../../compiler-worker-isolation/SKILL.md) skill
when the private IPC representation or its bounds change; it owns transport
validation and process lifetime while KIR meaning remains with its semantic owner.
Trace a new type through all serializers, enum bounds and independent runtime
decoders; adding a parser spelling or MLIR type alone is incomplete. KIR source
meaning composes [$ptx-simt-semantics](../../ptx-simt-semantics/SKILL.md) skill.

- Put syntax/source-language checks in the frontend.
- Put current KIR local and cross-operation constraints in its canonical
  verifier. For an actual MLIR operation/dialect, put local type, attribute,
  region, successor and symbol invariants in ODS traits/constraints where
  declarative definitions are practical, or its operation verifier.
- Put cross-operation dominance, memory-space, capability and whole-module
  MLIR relationships in explicit verification passes.
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
