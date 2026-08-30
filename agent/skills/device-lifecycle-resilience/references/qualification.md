# Lifecycle Qualification

## Evidence ladder

1. Schema validation and the mandatory bounded exploration command in
   [model checking](model-checking.md) generate legal/illegal event sequences and
   emit the machine-readable bounded ownership/identity model-check evidence.
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
- every accepted nonduplicate reset consumes one generation candidate even on a
  later failure; duplicate and pre-acceptance failures consume none;
- epoch and current identity remain unchanged before the replacement transaction;
  reset/recover atomically retire old, increment epoch exactly once, and install
  the candidate, while remove atomically retires old and reaches `ABSENT`;
- transport loss preserves epoch; accepted recovery retires the old `LOST`
  generation, advances epoch exactly once, and installs the candidate in one
  transaction; any later fault marks that committed candidate `LOST`;
- old fd/VMA/DMA/queue/event/memory/module/pipeline/handle objects stay isolated;
- CUDA/NVML/PCI/sysfs/cdev state and events converge without a half-online view;
- lifecycle loss/removal fence publication wins every race with telemetry
  publication/retry/fallback, so no post-fence call observes stale `ONLINE`;
- CUDA and NVML share one process `registry_view_id`; re-add creates no ordinal in
  initialized CUDA, and NVML sees an addition only after its next permitted
  zero-to-one initialization epoch or in a new process; same-revision views agree,
  while different revisions compare common live incarnations by
  `(UUID, generation)` and may differ in count/order;
- no leak, warning, hung task, duplicate PCI function, stale completion, false
  unmap acknowledgement, or vendor resource displacement;
- terminal state is published by deadline even when physical work cannot cancel.

The M0120 core and experimental vroot have separate promotion gates. The vroot
requires the planned Linux 6.12/6.18 `lspci`/sysfs/config qualification and 1,000
concurrent-use add/remove and load/unload cycles; it does not gate lifecycle-core
completion.
