# Kernel Qualification

## Build matrix

Build against pinned Linux 6.12 LTS and 6.18 LTS headers/configurations with each
supported distribution compiler and with `LLVM=1` where supported. Record kernel
release/config digest, compiler, module flags, compat probes, modversions, signing
state, and package form. Treat current online kernel docs as guidance; each
target's source is authoritative for API shape.

## Test matrix

- KUnit for state, range, extension, refcount, and ordering helpers.
- Userspace native/compat tests for ioctl encoding, layouts, mmap, poll/wait,
  eventfd, errno, and interrupted calls.
- KASAN for lifetime/bounds, KCSAN for races, lockdep for lock order, kmemleak for
  ownership, and fault injection for every allocation/pin/map/publish stage.
- Fuzz command, size, flags, reserved bytes, offsets, lengths, extensions, and
  legal concurrent sequences.
- Stress open/dup/fork/mmap/unmap/close, daemon and worker death, module remove,
  stale completion, owner death, counter wrap, and generation replacement.

## Acceptance evidence

Archive commands, kernel logs, test counts, seeds, sanitizer configuration, and
failure-free duration/cycles. Explicitly inspect warnings, hung tasks, refcount
reports, lock inversions, and leaked pins. A module load plus one happy ioctl is
not UAPI or lifetime qualification.

For build mechanics use the target kernel's [external module
documentation](https://docs.kernel.org/kbuild/modules.html); for API lifetime,
pinning, and ordering use the matching versioned kernel documentation/source.
