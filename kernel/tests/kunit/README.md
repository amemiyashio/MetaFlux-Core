# KUnit Tests

Planned KUnit suites cover object lifetime, worker-lease exclusivity, queue and
mapping reference counts, long-term page pin unwind, DMA unmap, generation
tombstones, and failure cleanup.

## Skip Harness

The kernel debug CONFIG probe (`metaflux.kernel.debug-qualification`) verifies
that the host kernel has CONFIG_KUNIT=y (and related debug symbols) before any
KUnit suite can run. On hosts where the config is absent or not-y, the probe
exits 77 and CTest records the suite as skipped.

Live KUnit execution remains a batch-0002 host gate; this probe only records
CONFIG presence and skips qualification when unset.
