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
the backing is reclaimed only after the final VMA closes. `MEMORY_REGISTER` remains
unsupported until the FOLL_PIN/SG accounting shim is qualified. Queue creation and
worker leasing accept optional eventfd descriptors, retain kernel references, and
reject a second eventfd owner for the same generation.

Build against the exact target kernel tree with:

```sh
make -C /lib/modules/$(uname -r)/build M=$PWD/kernel/core modules
```

The Kbuild rule regenerates the ignored header under `kernel/core/generated/`
from the repository manifest before compiling. Queue and payload mapping lifetime
is held by VMA callbacks, worker leases are exclusive per generation, unknown
ioctls return `-ENOTTY`, and malformed/short records are rejected before any
allocation or reference acquisition.

Use `LLVM=1` only with a target kernel configuration whose Kbuild flags support
the selected LLVM compiler; the current CachyOS 6.18 headers require GCC for
`mrecord-mcount` and related options.
