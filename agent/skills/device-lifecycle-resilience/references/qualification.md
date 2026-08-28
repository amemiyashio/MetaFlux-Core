# Lifecycle Qualification

## Evidence ladder

1. Schema validation and bounded model exploration generate legal/illegal event
   sequences and prove core ownership/identity invariants.
2. Adapter unit tests prove prepare/commit/abort/revoke/drain/tombstone behavior
   for registry, cdev, vfio-user, PCI/QMP, workers, and each backend.
3. Integration tests exercise full reset/remove/re-add and loss paths with
   unmodified CUDA/NVML clients and guest/static transports.
4. Stress and fault suites run concurrent users plus repeated lifecycle cycles on
   the pinned Linux/QEMU matrix with sanitizers and kernel diagnostics.
5. Release evidence measures deadline distribution, steady-state regression,
   resource residue, package lifecycle, and restart persistence.

## Required assertions

- one authoritative UUID view and at most one live generation/worker owner;
- exact idempotence under repeated requests and daemon/QMP retries;
- old fd/VMA/DMA/queue/event/memory/module/pipeline/handle objects stay isolated;
- CUDA/NVML/PCI/sysfs/cdev state and events converge without a half-online view;
- no leak, warning, hung task, duplicate PCI function, stale completion, false
  unmap acknowledgement, or vendor resource displacement;
- terminal state is published by deadline even when physical work cannot cancel.

The M0003 core and experimental vroot have separate promotion gates. The vroot
requires the planned Linux 6.12/6.18 `lspci`/sysfs/config qualification and 1,000
concurrent-use add/remove and load/unload cycles; it does not gate lifecycle-core
completion.
