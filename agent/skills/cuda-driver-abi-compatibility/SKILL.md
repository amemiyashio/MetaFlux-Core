---
name: cuda-driver-abi-compatibility
description: Design or review libcuda.so compatibility for CUDA Driver symbols, ELF version aliases, cuGetProcAddress, contexts, modules, memory, streams, events, and CUDA error behavior. Use for M0001 CUDA provider ABI work. Do not use for PTX semantics, compiler lowering, NVML telemetry, or backend execution policy.
---

# CUDA Driver ABI Compatibility

## Inputs

- The active milestone/work item and the exact CUDA header manifest or target
  release set being qualified.
- The provider ABI manifest, generated exports, version script, object tables,
  and the unmodified acceptance application relevant to the change.
- The current client protocol and registry-view contracts when behavior crosses
  the provider boundary.

Do not infer a CUDA surface from memory. Treat pinned headers and the repository
manifest as the versioned source of truth.

## Routing

- Use [symbols and versioning](references/symbols-and-versioning.md) for exports,
  aliases, typed stubs, and `cuGetProcAddress`.
- Use [object semantics](references/object-semantics.md) for contexts, handles,
  memory, modules, streams, events, teardown, and errors.
- Use [qualification](references/qualification.md) for ABI and application
  evidence.
- Route PTX meaning to `$ptx-simt-semantics`, lowering mechanics to
  `$mlir-compiler-engineering`, CPU execution to `$cpu-backend-performance`, and
  telemetry or `nvidia-smi` behavior to `$nvml-telemetry-compatibility`.

## Workflow

1. Freeze the target header/driver matrix and map each requested API to its
   canonical declaration, aliases, minimum API version, and implementation
   status.
2. Prove the ELF surface before behavior: SONAME, symbol names, symbol versions,
   visibility, aliases, calling convention, data layouts, and dependencies.
3. Define initialization and registry-view acquisition so loading the DSO alone
   creates no state and the first real call is lazy, reentrant, and fail-safe.
4. Specify each handle class with owner, generation, valid transitions,
   concurrency rule, destruction behavior, and stale-handle error.
5. Trace synchronous and asynchronous error delivery through copies, launches,
   events, and synchronization points. Preserve CUDA-observable ordering rather
   than leaking internal transport errors.
6. Keep the provider C17-only and route execution through the ecosystem-neutral
   client protocol. Do not introduce LLVM/MLIR, a backend dependency, or a C++
   object across the provider boundary.
7. Build positive, negative, short-buffer, version-mismatch, repeated-lifecycle,
   and concurrent tests from the manifest rather than hand-picking symbols.

## Output

Return or implement:

- A versioned symbol/alias/status matrix and any manifest changes.
- An object-lifecycle and error-semantics table for affected APIs.
- A bounded implementation slice naming provider, protocol, and test owners.
- Qualification evidence with exact target headers, commands, and observed
  behavior; list unsupported calls and their typed CUDA errors explicitly.

## Verification

- Compare generated exports and versions with the pinned manifest and target
  headers; inspect the DSO with `readelf` or equivalent ELF tools.
- Verify C/C++ layout probes, dependency closure, constructor-free loading, and
  simultaneous CUDA/NVML loading.
- Run the unmodified Add/Copy path across interpreter, cold JIT, warm JIT, and
  AOT when the owning workstream is active.
- Exercise nulls, short buffers, invalid ordinals, stale handles, duplicate
  destroy, concurrent init/shutdown, fault injection, and every generated stub.
- Report planned gates as planned. Do not claim provider compatibility from a
  fixture-only or compile-only result.
