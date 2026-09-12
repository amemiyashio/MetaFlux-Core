# Debugging and Versioning

Start from the actual emission/translation boundary in
[target paths](target-lowering.md). CPU failures may occur while generating or
parsing the LLVM-dialect text before any pass runs. Capture that first failing
module and its KIR input rather than searching for a nonexistent shared pass.

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

First distinguish a compiler failure from spawn, IPC, deadline or cleanup
failure. The latter composes
[$compiler-worker-isolation](../../compiler-worker-isolation/SKILL.md) skill and
starts in `compiler_worker_client.cpp`; repeating passes does not diagnose a
truncated response or unreaped child. A reproducible invalid/incorrect emitted
module stays here with the owning target skill.

## Epoch and cache discipline

Compiler epoch 1 is defined only by
[`toolchains/compiler-epoch-1.json`](../../../../toolchains/compiler-epoch-1.json).
Use its pinned LLVM/MLIR version and installed APIs, not a moving online version.
This skill supplies compiler epoch/patchset, KIR schema and canonical pipeline
meaning. The backend supplies target/features, FP/argument policy, helper/backend
ABI and PGO identity. Compose
[$compiler-artifact-cache](../../compiler-artifact-cache/SKILL.md) skill for key
encoding, tier selection, atomic publication, integrity, quota/pins and corruption
recovery; its [identity guide](../../compiler-artifact-cache/references/identity-and-modes.md)
traces the concrete CPU preparation path and current lookup costs.

A changed semantic/target/ABI input must cause an incompatible-entry miss.
Warm-JIT and AOT lookup invoke no compiler framework, and resident execution
does not repeat cache key generation or lookup. An interpreter result provides
no PIC-ELF execution proof. Compose the target skill for loader integrity,
helper closure and warm resources; Vulkan portable SPIR-V identity and
driver/device-bound residency stay distinct. These responsibilities do not turn
artifact keys into verification-result caches.

MLIR bytecode and LLVM IR are epoch-local debugging/intermediate artifacts. Do
not promise cross-version durability for them.

Primary sources:

- [MLIR Pass Management](https://mlir.llvm.org/docs/PassManagement/)
- [MLIR bytecode format](https://mlir.llvm.org/docs/BytecodeFormat/)
