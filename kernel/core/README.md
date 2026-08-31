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
the backing is reclaimed only after the root, owner, VMA, and active allocation
references all drain. `MEMORY_REGISTER` accepts
up to four caller-owned ranges, each up to 64 MiB, with a 256 MiB aggregate
quota per generation. It charges the current process's
memlock quota, pins full pages with `pin_user_pages_fast()` using
`FOLL_LONGTERM` and optional `FOLL_WRITE`, and builds an independent SG table
per slot. Each SG table is direction-mapped through the data cdev's DMA device.
Unregister and owner close remove live slots under the cdev lock, then unmap
each SG table, free it, dirty-unpin device-written pages, release the memlock
charge, and drop the mm reference outside the lock. A map failure unwinds in
the same order before publication. Registration handles are unique per slot
(`3` through `6`) and generation-bound. This proves the kernel pin/map lifetime
only; a physical GPU DMA master, backend memory import, and worker references
remain unqualified. Queue creation and worker leasing
accept optional eventfd descriptors, retain kernel references, and reject a second
eventfd owner for the same generation.

Build against the exact target kernel tree with:

```sh
make -C /lib/modules/$(uname -r)/build M=$PWD/kernel/core modules
```

The Kbuild rule regenerates the ignored header under `kernel/core/generated/`
from the repository manifest before compiling. Queue and payload mapping lifetime
is held by VMA callbacks. The queue backing has a root, owner, lease, VMA, and
active wait/poll reference graph; the payload backing has root, owner, VMA, and
active allocation-operation references. Offline queue and payload mappings are
retained as tombstones until their last references close, then their backing is
reclaimed under the cdev lock. Worker leases are exclusive per generation, and
the leased control fd may map the paired queue at offset zero while the data fd
maps the payload arena. Closing either the queue owner or worker lease marks the
generation offline and wakes waiters.
Unknown ioctls return `-ENOTTY`, and malformed/short records are rejected before
any allocation or reference acquisition.

Use `LLVM=1` only with a target kernel configuration whose Kbuild flags support
the selected LLVM compiler; the current CachyOS 6.18 headers require GCC for
`mrecord-mcount` and related options.
