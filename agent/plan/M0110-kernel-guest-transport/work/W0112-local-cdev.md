---
id: W0112
delivery: 0.1.1.2
milestone: M0110
status: Active
area: transport.cdev
depends_on: [W0111]
updated: 2026-08-30
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

## Remaining work

- [ ] Complete queue object/kref/tombstone ownership, backend reference drain, and
  daemon-controlled generation replacement. The payload arena VMA tombstone,
  eventfd references, and bounded registered-memory lifetime are implemented for
  the current fixture.
- [ ] Connect the leased worker to `mf_backend_api_v1` and prove unmodified CPU
  Add/Copy end to end through the mapped payload arena.
- [ ] Test open/mmap/process/daemon death, stale generation, counter wrap, and
  teardown with KUnit, KASAN, KCSAN, lockdep, and kmemleak.

## Exit Gate

The implemented stage is not the W0112 exit gate yet. Closure still requires
unmodified Add/Copy through `/dev/metafluxN`, an uncontended active enqueue with
no syscall, allocation, or global lock, and fd/VMA tombstones that remain safe
after daemon death. Contention and owner-death slow paths must be bounded
separately.
