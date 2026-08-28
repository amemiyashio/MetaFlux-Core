# Debugging and Versioning

## First-divergence workflow

1. Reduce to the smallest Kernel IR/MLIR module and exact command/pipeline.
2. Record compiler epoch, build identity, target, features, environment digest,
   and random/concurrency seed.
3. Verify after each pass and capture IR before and after the first failing or
   semantically divergent pass.
4. Enable pass timing/statistics only after correctness reproduction is stable.
5. Generate a local crash reproducer with the pipeline and required resources;
   remove paths or environment details that are not repository evidence.
6. Add the minimized case at the narrowest test layer and an end-to-end
   differential case where the bug crossed layers.

Use stable diagnostic codes and source locations. Avoid tests that depend on
entire printer wording or incidental operation order unless canonical order is
the contract.

## Epoch and cache discipline

Compiler epoch 1 is defined only by `toolchains/compiler-epoch-1.json`. Cache
identity includes the epoch and patchset, Kernel IR schema, canonical pipeline,
target triple/environment and features, FP/argument semantics, helper/backend
ABI, PGO identity, and content hash. Atomic publication, integrity validation,
corruption removal, and incompatible-entry misses are required.

MLIR bytecode and LLVM IR are epoch-local debugging/intermediate artifacts. Do
not promise cross-version durability for them.

Primary sources:

- [MLIR Pass Management](https://mlir.llvm.org/docs/PassManagement/)
- [MLIR bytecode format](https://mlir.llvm.org/docs/BytecodeFormat/)
