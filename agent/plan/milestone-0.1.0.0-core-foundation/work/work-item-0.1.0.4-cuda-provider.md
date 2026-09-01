---
id: work-item-0.1.0.4
delivery: 0.1.0.4
milestone: milestone-0.1.0.0
status: Complete
area: compat.cuda.driver
depends_on: [work-item-0.1.0.2, work-item-0.1.0.3]
updated: 2026-08-30
---

# CUDA Driver Provider

## Outcome

Run the unmodified Add/Copy acceptance application through a C17 CUDA Driver ABI
provider without compiler or C++ runtime dependencies in the application process.

## Required Surface

The generated manifest covers initialization/version, device count/ordinal/name/
UUID/attributes/memory, primary/basic contexts, device allocation/free, HtoD/
DtoH/DtoD copies, PTX module/function/launch, streams, events, error name/string,
`cuGetProcAddress`, required `_v2`/`_v3`, and per-thread-default-stream aliases.
Known but unimplemented target-version symbols receive correctly typed generated
stubs and explicit CUDA errors.

## Work

- [x] Generate export map, aliases, stubs, and `cuGetProcAddress` table from a
  checked-in ABI manifest.
- [x] Use hidden visibility and prove the exact symbol/version set and simultaneous
  CUDA/NVML loading.
- [x] Implement lazy reentrant initialization without constructors, threads,
  sockets, or allocation before the first real API call.
- [x] Implement device/context/module/function/memory/stream/event object tables.
- [x] Route launch, copy, event, and synchronization through the shared fast path.
- [x] Implement `CUDA_VISIBLE_DEVICES` filtering/reordering and stable errors.
- [x] Run the acceptance application against interpreter, cold/warm JIT, and AOT.

## Exit Gate

Stock CUDA Driver ABI Add/Copy passes with correct cleanup, concurrent
init/shutdown, stale-handle behavior, fault injection, exact exports, and no
provider C++/LLVM/MLIR/Python/systemd/`libatomic` dependency.
