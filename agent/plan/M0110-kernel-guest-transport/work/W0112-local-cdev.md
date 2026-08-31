---
id: W0112
delivery: 0.1.1.2
milestone: M0110
status: Active
area: transport.cdev
depends_on: [W0111]
updated: 2026-08-31
---

# Local cdev Vertical Slice

## Outcome

Execute unmodified CPU-backed CUDA Add/Copy through canonical cdev mappings with
one leased local worker and no PCI module.

```text
application -> CUDA/NVML provider -> /dev/metafluxN mmap
            -> shared ring -> leased local worker -> mf_backend_api_v1 -> CPU
metafluxd -> /dev/metafluxctl lease/broker -> local worker
```

The worker may be embedded in `metafluxd`, but queue/backend ownership remains
explicit. The authoritative ownership rule is maintained in
[the architecture record](../../../../docs/architecture/control-and-data-plane.md).

The M0100 memfd path is local-only fallback. Before any visible object succeeds,
cdev may fall back only for `ENOENT`, `ENODEV`, or explicit ABI incompatibility.
Permission, malformed state, integrity, or policy rejection never falls back.
Transport commits for the provider initialization epoch; a later provider joins
the same mode/transport/`registry_view_id`.

Each stream owns one SPSC descriptor lane. Same-stream host threads serialize
with a robust userspace owner word; independent streams share no producer lock.
Descriptors are 64 bytes with slot sequence, opcode, flags, generation, timeline,
handle, offset, and length. Cursors/timeline/armed state use separate cache lines.
Owner death marks that queue generation `LOST`; incomplete slots are not reused.
Empty-to-nonempty uses arm/recheck/wake. Active queues poll atomically; blocking
queues arm/recheck before futex/eventfd. Every handle/range/permission/generation
and arithmetic operation is validated.

## Implemented stage

- [x] Add C17 client and C++20 worker halves with a paired submission/completion
  ring projection and generation validation.
- [x] Build `metaflux_core.ko` through target Kbuild; register `/dev/metafluxctl`
  and `/dev/metaflux0`, negotiate the fixed candidate UAPI, map the paired rings,
  and enforce one worker lease per generation.
- [x] Keep cdev fallback limited to `ENOENT`/`ENODEV`/explicit ABI incompatibility
  in the userspace client; permission and malformed states remain visible.
- [x] Allocate one generation-bound, page-aligned driver payload arena through
  `MF_UAPI_IOCTL_MEMORY_ALLOC`, expose it through the generated payload mmap
  offset, and retain an offline tombstone until the final VMA closes.
- [x] Attach one complete caller-owned eventfd pair to either the data queue or
  worker lease, retain kernel `eventfd_ctx` references, and reject a second owner
  for the generation with `-EBUSY`.
- [x] Register one bounded caller-owned range with `FOLL_LONGTERM`/`FOLL_WRITE`
  pinning, normal memlock accounting, an SG table, partial-pin unwind, dirty
  unpin, and owner-close or explicit unregister revocation. Backend DMA mapping,
  multi-region quota, and in-flight device references remain open.
- [x] Add a worker-side `mf_backend_api_v1` COPY dispatch seam with sized-table,
  capability, handle, offset, and backend-status validation. An unbound worker
  retains the local fixture copy path; a malformed bound API returns
  `MF_SHARED_NOT_SUPPORTED` without fallback. Backend memory import and cdev
  descriptor-to-launch wiring remain open.
- [x] Expose the CPU backend's transport-facing COPY and synchronous launch
  subset (instance, context, queue, caller-owned host-memory import, canonical
  KIR module load/unload, and memory-handle argument blocks). The CPU backend
  executes the Add fixture through `mf_backend_api_v1.submit` with strict
  dimensions, ranges, ownership, and typed status mapping.
- [x] Add cdev launch descriptor helpers and a backend-agnostic,
  generation-bound `CdevLaunchResolver`. The resolver translates module and
  argument-block object references into payload-relative backend argument bytes;
  the worker validates the range and 2D launch shape, then invokes the backend
  submit ABI. The real CPU backend Add fixture now passes through this cdev
  worker path; asynchronous events and backend DMA remain open.
- [x] Retain an offline queue mapping as a VMA tombstone after module teardown
  and reclaim its backing under the cdev lock when the final queue VMA closes.
- [x] Mark the current generation offline and wake waiters when the queue owner
  or worker lease closes, preventing new users from reusing that tombstone.
- [x] Track queue backing with distinct root, owner, lease, VMA, and active
  wait/poll `kref` references so the final reference performs reclamation.
- [x] Track payload backing with distinct root, owner, VMA, and active allocation
  operation `kref` references so owner or module close cannot reclaim an arena
  while `MEMORY_ALLOC` or an existing VMA still observes it.

## Remaining work

- [ ] Complete daemon-controlled generation replacement and backend reference
  drain beyond the queue and payload kref/tombstone graphs. The payload and
  queue VMA tombstones, owner-death transition, eventfd references, and bounded
  registered-memory lifetime are implemented for the current fixture.
- [ ] Extend the leased worker/backend binding from the synchronous Add/launch
  descriptor path to production registered-memory import and DMA mapping, and
  prove Add/Copy with those references through the mapped payload arena.
- [ ] Test open/mmap/process/daemon death, stale generation, counter wrap, and
  teardown with KUnit, KASAN, KCSAN, lockdep, and kmemleak.

## Exit Gate

The implemented stage is not the W0112 exit gate yet. Closure still requires
unmodified Add/Copy through `/dev/metafluxN`, an uncontended active enqueue with
no syscall, allocation, or global lock, and fd/VMA tombstones that remain safe
after daemon death. Contention and owner-death slow paths must be bounded
separately.
