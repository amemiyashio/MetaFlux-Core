---
id: M0001-W02
milestone: M0001
status: Queued
area: contracts-runtime
depends_on: [M0001-W01]
updated: 2026-08-27
---

# Contracts and Runtime Fast Path

## Outcome

Qualify the ecosystem-neutral registry, shared-memory queue, client protocol, and
backend C ABI used by the first CPU-backed device.

## Contract Requirements

`VirtualDeviceRecord` contains persistent logical ID/GPU UUID, display name,
stable local BDF, virtual compute capability, memory quota/commit counters,
backend/capability identifiers, state, epoch, and generation. Default CUDA/NVML
ordering agrees; `CUDA_VISIBLE_DEVICES` may filter or reorder only CUDA.

Static identity uses a read-only registry page and dynamic metrics use a separate
two-bank latch snapshot. Readers acquire bank/generation, read one snapshot, and
verify generation; they retry at most twice and may return the prior complete
snapshot instead of spinning.

One process shares one selection/view record across CUDA and NVML:

- The first active provider commits mode, transport, and `registry_view_id`
  before returning a device or handle.
- CUDA freezes its view after successful `cuInit` until process exit.
- NVML freezes on init refcount zero-to-one and releases on matching shutdown.
- A later provider joins the existing view or reports incompatibility.
- Loss updates frozen-view state immediately; additions wait for a later NVML
  epoch or new process and never change an initialized CUDA ordinal set.

Shared layouts contain fixed-width integers, byte arrays, offsets,
generation-bound handles, and explicit padding/alignment. They contain no
pointers, `size_t`, language `bool`, native enum, `std::atomic`, C `_Atomic`,
kernel `atomic_t`, STL object, packed atomic, or naturally unaligned atomic.

C and C++ use one `mf_atomic_*` wrapper over `__atomic_*`. Builds require aligned,
lock-free 32/64-bit operations and reject `libatomic`. Cursors occupy separate
64-byte cache lines; descriptors are exactly 64 bytes with per-slot sequence.
Empty-to-nonempty uses an armed/sleeping handshake and at most one doorbell.

`mf_backend_api_v1` covers enumeration/capabilities, compilation/loading,
context/queue/memory lifecycle, submit/copy/event/sync/cancel, metrics, and policy.
The allocating side frees. No exception, STL type, compiler class, or allocator
ownership crosses the ABI. Backend registration lives in
`contracts/plugin/backend/v1`; client negotiation lives independently in
`contracts/protocol/client/v1`. Statically linked v0.x backends are called only
through their C entrypoint/function table.

## Work

- [ ] Implement fixed-layout C types and generated C/C++ size/alignment/offset
  assertions.
- [ ] Implement registry and dynamic latch pages, generation-bound handles, and
  stale-handle errors.
- [ ] Implement per-context rings, timeline completion, doorbell, and blocking
  futex/eventfd fallback.
- [ ] Stress process death, generation rollover, shortened counter wrap, NUMA,
  and false sharing across multiple processes.

## Exit Gate

One client and one daemon exchange one million no-op descriptors with correct
ordering, no lost wakeup, and zero syscall/global lock on an active queue. C and
C++ ABI assertions match on every supported build and no `libatomic` dependency
appears.
