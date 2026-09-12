# Object and VMA Lifetime

## Ownership ledger

For every object, record allocation owner, published references, lookup lock or
RCU domain, parent/child edges, fd and VMA refs, worker/backend refs, generation,
revocation point, finalizer context, and whether finalization may sleep.

## Required sequencing

1. Allocate and fully initialize off-list.
2. Publish under the owning synchronization primitive only after invariants hold.
3. On revoke/remove, reject new lookup/open/submission first.
4. Remove the object from live lookup before draining existing references.
5. Mark old fd/VMA/queue/handle objects as generation-bound tombstones that
   return a deterministic lost/stale error without touching replacement state.
6. Drain or isolate asynchronous work to deadline.
7. Release child resources in reverse dependency order; free only after the last
   kref/VMA/callback reference is gone.

`close()` is not proof that all VMAs disappeared, and device removal is not proof
that userspace stopped accessing existing mappings. VMA open/close callbacks
must hold their own reference. Avoid invoking user callbacks, eventfd signaling,
or sleeping finalizers while holding locks that teardown paths reacquire.

During implementation select cases within the changed ownership boundary;
the work item's final qualification retains its complete required coverage.
Test fd duplication, forked VMAs, process exit, daemon death, concurrent ioctl
and unmap, remove during page fault, worker completion after revoke, repeated
remove, and module unload. KASAN/KCSAN/lockdep/kmemleak evidence should name the
exact interleaving or stress seed.
