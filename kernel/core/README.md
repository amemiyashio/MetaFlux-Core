# Kernel Core

`metaflux_core.ko` now provides the first W0112 local cdev slice. It registers
`/dev/metafluxctl` for an exclusive worker lease and `/dev/metaflux0` for
generation-bound negotiation, a paired submission/completion ring mapping, and
timeline waits. The public records come from the generated transport projection;
the module keeps one online generation (`daemon_incarnation=1`, `view_serial=1`,
generation 1) as a deterministic fixture until the daemon lifecycle authority is
connected.

Build against the exact target kernel tree with:

```sh
make -C /lib/modules/$(uname -r)/build M=$PWD/kernel/core modules
```

The Kbuild rule regenerates the ignored header under `kernel/core/generated/`
from the repository manifest before compiling. Queue mapping lifetime is held by
VMA callbacks, worker leases are exclusive per generation, unknown ioctls return
`-ENOTTY`, and memory allocation/registration plus eventfd attachment remain
explicitly unsupported until their UAPI stages are implemented.

Use `LLVM=1` only with a target kernel configuration whose Kbuild flags support
the selected LLVM compiler; the current CachyOS 6.18 headers require GCC for
`mrecord-mcount` and related options.
