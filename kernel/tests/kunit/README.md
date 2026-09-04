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

The kernel debug CONFIG probe (`metaflux.kernel.debug-qualification`) verifies
that the host kernel has CONFIG_KUNIT=y (and related debug symbols) before any
KUnit suite can run. On hosts where the config is absent or not-y, the probe
exits 77 and CTest records the suite as skipped.

Live KUnit execution remains a batch-0002 host gate; this probe only records
CONFIG presence and skips qualification when unset.
