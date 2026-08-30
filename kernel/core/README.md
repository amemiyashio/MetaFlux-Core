# Kernel Core

`metaflux_core.ko` now provides the first W0112 local cdev slice. It registers
`/dev/metafluxctl` for an exclusive worker lease and `/dev/metaflux0` for
generation-bound negotiation, a paired submission/completion ring mapping, and
timeline waits. The public records come from the generated transport projection;
the module keeps one online generation (`daemon_incarnation=1`, `view_serial=1`,
generation 1) as a deterministic fixture until the daemon lifecycle authority is
connected.

`MF_UAPI_IOCTL_MEMORY_ALLOC` now creates one page-aligned payload arena (up to
64 MiB) owned by the data-file descriptor. Its returned byte range is mapped at
`MF_UAPI_MMAP_PAYLOAD_V0`; owner close transitions it to an offline tombstone and
the backing is reclaimed only after the final VMA closes. `MEMORY_REGISTER` accepts
one caller-owned range up to the same bound, charges the current process's
memlock quota, pins full pages with `pin_user_pages_fast()` using
`FOLL_LONGTERM` and optional `FOLL_WRITE`, and builds an SG table. Unregister and
owner close remove the live object under the cdev lock, then free the SG table,
dirty-unpin device-written pages, release the memlock charge, and drop the mm
reference outside the lock. The generated registration handle is a deterministic
fixture handle (`3`) for the current generation. Backend DMA mapping and worker
references are intentionally not claimed yet. Queue creation and worker leasing
accept optional eventfd descriptors, retain kernel references, and reject a second
eventfd owner for the same generation.

Build against the exact target kernel tree with:

```sh
make -C /lib/modules/$(uname -r)/build M=$PWD/kernel/core modules
```

The Kbuild rule regenerates the ignored header under `kernel/core/generated/`
from the repository manifest before compiling. Queue and payload mapping lifetime
is held by VMA callbacks. The queue backing has a root, owner, lease, VMA, and
active wait/poll reference graph; an offline queue mapping is retained as a
tombstone until the last reference closes, then its backing is reclaimed under
the cdev lock. Worker leases are exclusive per generation, and closing either
the queue owner or worker lease marks the generation offline and wakes waiters.
Unknown ioctls return `-ENOTTY`, and malformed/short records are rejected before
any allocation or reference acquisition.

Use `LLVM=1` only with a target kernel configuration whose Kbuild flags support
the selected LLVM compiler; the current CachyOS 6.18 headers require GCC for
`mrecord-mcount` and related options.
