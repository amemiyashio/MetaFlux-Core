# Guest DMA Lifetime

## Mapping record

Each accepted DMA range records guest IOVA start/end, permissions, backing fd and
offset, host mapping/range, mapping epoch, device generation, overlap-tree entry,
queue/backend/callback reference counts, revoking flag, and finalization state.

## Map path

1. Validate connection state, address width, nonzero length, overflow, flags,
   permissions, fd count/type, page/offset alignment, quota, and overlap.
2. Map only the requested file-backed range and validate host offset arithmetic.
3. Construct metadata off-index, then publish atomically for descriptor lookup.
4. Bind any guest registration to both mapping epoch and generation so an IOVA
   reused after unmap cannot satisfy an old descriptor.

## Descriptor lookup

Validate opcode-specific read/write permission and the full IOVA range before
taking a mapping reference. Split SG ranges deliberately or reject crossings;
never translate only the first byte. Hold the reference through backend and
completion callbacks.

## Unmap path

Remove the range from new lookup first, invalidate registrations, and drain all
queue/backend/callback refs to deadline. Release host maps/fds only after the
drain. Reply success only after no server-side reference can touch guest memory.
On deadline, publish lost and close without a success reply; retain tombstone
state until late references are isolated and released.

Test exact, partial, covering, split, overlapping, duplicate, stale-epoch, and
concurrent map/unmap requests plus IOVA reuse and late completion.
