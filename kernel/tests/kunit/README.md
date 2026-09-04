# KUnit Tests

Planned KUnit suites cover object lifetime, worker-lease exclusivity, queue and
mapping reference counts, long-term page pin unwind, DMA unmap, generation
tombstones, and failure cleanup.

## Current Suite: mf_cdev_generation

The `mf_cdev_generation` KUnit suite (see
`kernel/tests/kunit/mf_cdev_generation_test.c`) exercises the
generation/stale-handle helpers extracted from `kernel/core/metaflux_core_main.c`:

- **Stale generation rejected** — a request carrying a mismatched generation
  is rejected.
- **Matching generation accepted** — an exact-match generation or the zero
  sentinel (no prior binding) is accepted.
- **Exclusive lease occupancy** — only one owner may hold the lease; a second
  take by a different file is rejected.
- **Tombstone** — after release, the queue is marked offline; a stale
  generation handle cannot be re-admitted as current.

A userspace companion test (`mf_cdev_generation_userspace_test.c`) exercises
the same integer logic without `linux/kunit.h` and is built as the CTest gate
`metaflux.kernel.cdev-generation-helper`.

## Skip Harness

The host debug CONFIG probe (`metaflux.kernel.debug-qualification`) verifies
that the host kernel has CONFIG_KUNIT=y (and related debug symbols) before any
KUnit suite can run. On hosts where the config is absent or not-y, the probe
exits 77 and CTest records the suite as skipped.

The KUnit generation probe (`metaflux.kernel.kunit-generation`) is NOT gated
on host CONFIG_KUNIT. It skips only when `METAFLUX_LINUX_SRC` is unset or
points to an invalid tree, which happens when the runner is not launched from
the `linux-debug` Nix shell.

Live KUnit execution uses the pinned `linux_6_12` source via the
`linux-debug` Nix shell:

```sh
nix develop .#linux-debug --command python3 tools/run-kunit-generation.py
```

The environment variable `METAFLUX_LINUX_SRC` must point at the materialized
`linux-debug-tools` package (set automatically by the shell). The runner
exits 77 when `METAFLUX_LINUX_SRC` is unset, so CTest records the test as
skipped when the shell is not active. Host KASAN/KCSAN/lockdep/kmemleak soak
remains a separate debug-kernel gate.
