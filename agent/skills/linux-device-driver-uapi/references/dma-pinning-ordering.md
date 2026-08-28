# DMA, Pinning, and Ordering

## Page and DMA ledger

For each registration record, preserve mm/credential owner, user range, page
count, `FOLL_PIN`/`FOLL_LONGTERM`/`FOLL_WRITE` flags, quota charge, DMA direction,
SG table, mapped segments, IOVA/mapping epoch, permissions, generation,
in-flight refs, and dirty state.

- Use `pin_user_pages*()` for DMA-pinned user pages. Long-lived mappings use
  `FOLL_LONGTERM`; device-write targets also require `FOLL_WRITE`.
- Reject non-pinnable/DAX or policy-disallowed memory. Do not fall back to
  short-term GUP.
- Charge normal memlock policy and project context quota before publication.
- Map and unmap with one consistent DMA direction; synchronize non-coherent
  ownership at every CPU/device transition.
- Remove a mapping from new lookup before draining existing refs. A successful
  unmap means all queue, backend, callback, host-map, and SG/DMA refs are gone.
- Dirty device-written pages as required and unpin every page exactly once,
  including partial-pin and partial-map failures.

## Ordering ledger

Write the producer store sequence, release/DMA write barrier, typed 32-bit MMIO
doorbell, device/server acquire or DMA read barrier, completion stores, release,
timeline publication, waiter acquire, and interrupt/eventfd arming protocol.
Name which address domain each barrier orders. CPU atomics do not automatically
order DMA or MMIO.

Primary sources:

- [Pinning user pages](https://docs.kernel.org/core-api/pin_user_pages.html)
- [DMA API how-to](https://docs.kernel.org/core-api/dma-api-howto.html)
- [Linux memory barriers](https://docs.kernel.org/core-api/wrappers/memory-barriers.html)
