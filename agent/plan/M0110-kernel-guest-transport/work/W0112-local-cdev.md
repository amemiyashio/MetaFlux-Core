---
id: W0112
delivery: 0.1.1.2
milestone: M0110
status: Queued
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

## Work

- [ ] Implement `metaflux_core.ko` object/kref/cdev/VMA/tombstone ownership.
- [ ] Implement negotiate/context/queue/mapping/eventfd/wait UAPI.
- [ ] Implement the worker broker and prove one leased consumer per generation.
- [ ] Select cdev under the fallback rules and retain M0100 memfd.
- [ ] Execute CPU Add/Copy with no PCI module.
- [ ] Test open/mmap/process/daemon death, stale generation, counter wrap, and
  teardown with KUnit, KASAN, KCSAN, lockdep, and kmemleak.

## Exit Gate

Unmodified Add/Copy succeeds through `/dev/metafluxN`; uncontended active enqueue
uses no syscall, allocation, or global lock; fd/VMA tombstones remain safe after
daemon death. Contention and owner-death slow paths are bounded separately.
