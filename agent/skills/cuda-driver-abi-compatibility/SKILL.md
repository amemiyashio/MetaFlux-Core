---
name: cuda-driver-abi-compatibility
description: Implement or review libcuda.so compatibility for CUDA Driver symbols, ELF version aliases, cuGetProcAddress, contexts, modules, memory, streams, events, and CUDA error behavior. Use for provider ABI work, the stock PyTorch CUDA profile, or CUDA-visible lifecycle behavior. Do not use for PTX semantics, compiler lowering, NVML telemetry, or backend execution policy.
---

# CUDA Driver ABI Compatibility

## Implementation Focus

For an implementation request, use the shared
[implementation guidance](../review/references/implementation-guidance.md).
Select the affected inputs and obligations below; broad qualification lists
do not make every invocation a new inventory or full-suite run.

Trace the stock client's actual call through symbol resolution, descriptor
admission, fresh/reused handles, module identity and daemon completion. Implement
the missing translation or lifecycle behavior with its real client consumer.
Before removing a fallback, prove reachability across admitted configurations,
not just corpus entries; follow client-side retries after a typed rejection.
A fixed-profile rejection repair does not add supported shapes.

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
- Compose `$runtime-contracts-registry` when process-view membership, ordering,
  `registry_view_id`, or provider freeze rules change, and
  `$device-lifecycle-resilience` when reset/loss/replacement generation changes.
  This skill owns the CUDA-visible mapping, handle lifetime, and error outcome;
  it consumes rather than redefines those shared contracts.

## Workflow

1. Freeze the target header/driver matrix and map each requested API to its
   canonical declaration, aliases, minimum API version, and implementation
   status.
2. Prove the ELF surface before behavior: SONAME, symbol names, symbol versions,
   visibility, aliases, calling convention, data layouts, and dependencies.
3. Define initialization and registry-view acquisition so loading the DSO alone
   creates no state and the first real call is lazy, reentrant, and fail-safe.
4. Classify every observable behavior as normative for the pinned header/spec,
   observed on a named target driver family/build, or MetaFlux-strengthened where
   CUDA leaves behavior undefined or unspecified. Never present an observation or
   strengthening as a CUDA compatibility guarantee.
5. Specify each handle class with owner, generation, valid transitions,
   concurrency rule, destruction behavior, stale-handle outcome, and behavior
   classification.
6. Trace synchronous and asynchronous error delivery through copies, launches,
   events, and synchronization points. Preserve normative CUDA observation points;
   qualify selection/order among multiple pending errors on named driver builds
   when the specification does not fix it.
7. Keep the provider C17-only and route execution through the ecosystem-neutral
   client protocol. Do not introduce LLVM/MLIR, a backend dependency, or a C++
   object across the provider boundary.
8. Build positive, negative, short-buffer, version-mismatch, repeated-lifecycle,
   and concurrent tests from the manifest rather than hand-picking symbols.

## Output

Select the applicable outputs for the requested task:

- A versioned symbol/alias/status matrix and any manifest changes.
- An object-lifecycle and error-semantics table for affected APIs, with each row
  labeled normative, observed on a named driver build, or MetaFlux-strengthened.
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
  destroy, concurrent init/shutdown, fault injection, and every generated stub;
  verify each expected result carries its behavior classification and evidence.
- Report planned gates as planned. Do not claim provider compatibility from a
  fixture-only or compile-only result.
