---
id: work-item-0.2.0.2
delivery: 0.2.0.2
milestone: milestone-0.2.0.0
status: Active
area: compiler-cpu
depends_on: [work-item-0.2.0.1]
updated: 2026-09-08
---

# Kernel IR and MLIR CPU Pipeline

## Outcome

The pinned CUDA/PyTorch profile becomes a versioned ecosystem-neutral request,
the daemon/compiler worker validates it into canonical Kernel IR, and the
backend-owned MLIR/LLVM pipeline executes the declared eager corpus through the
stock daemon with results bit-exact against the torch CPU reference.

## Current Implementation

The CUDA provider retains deferred client fatbins, parses cubin ELF symbol
names, resolves selected framework kernels, and contains a profile-specific
semantic router for elementwise, reduction, concatenation, layout, comparison,
and range operations. That router performs tensor arithmetic in the
application-side provider over host copy buffers; deferred modules explicitly
skip daemon artifact registration and module launch.

No checked-in real-client corpus manifest or CTest gate proves the reported
operator sweep. This is an implementation prototype, not daemon/CPU-backend
integration or qualification.

## Work

- [ ] Define the exact versioned framework-kernel request selected by
  decision-0044. It carries neutral operation identity, typed arguments,
  launch geometry, memory references, profile identity, and semantic options;
  CUDA, PyTorch, MLIR, and target-specific types remain outside the wire form.
- [ ] Validate and canonicalize each accepted request into versioned Kernel IR
  inside the daemon/compiler worker. Unsupported profile operations fail before
  compilation, and MLIR bytecode is never the durable or cross-process format.
- [ ] Close the library-backed operator boundary, including matmul behavior when
  vendored cuBLAS execution remains excluded.
- [ ] Move tensor arithmetic and result materialization out of the CUDA provider
  and lower verified Kernel IR through an explicitly legal MLIR conversion to
  the CPU backend's LLVM dialect, LLVM IR, and validated PIC ELF path.
- [ ] Add a versioned real-client corpus manifest and checked-in gate covering
  baseline artifact intake and eager add, module volume, repeated function
  resolution, cold/warm cache behavior, supported operations, exact
  inputs/outputs, and classified unsupported operations.
- [ ] If the route expands PTX or Kernel IR, advance parser, verifier, semantic
  oracle, interpreter, lowering, differential corpus, compiler epoch, and cache
  identity together. Bind request schema, canonical pipeline, target triple,
  CPU features, FP policy, helper/backend ABI, and content hash into the cache.
- [ ] Prove that every accepted corpus operation produces a daemon submission
  and a CPU-backend completion rather than a provider-local result.

## Exit Gate

The request-schema and library-boundary decisions are closed; provider-local
tensor execution is absent; the real-client gate reaches `complete`; every
accepted request verifies as canonical Kernel IR and lowers through the
backend-owned MLIR/LLVM path; the versioned corpus passes bit-exact through the
neutral protocol, stock daemon, and CPU backend from a named revision; cold/warm
cache and many-kernel tests pass; and every unsupported operation returns its
declared error without wrong data.
