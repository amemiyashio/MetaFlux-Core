# Failure Injection

## Stage matrix

Apply this matrix to the changed transition during implementation, after the
mandatory model prerequisite. Final qualification keeps every side-effect
boundary required by the owning work item.

Inject before and after each affected durable or externally visible side effect:

- request acceptance plus generation-candidate reservation and high-water
  persistence;
- registry staging/publication;
- transport socket/node/mapping setup;
- PCI function/config publication and driver bind;
- backend creation and resource import;
- worker lease stage/commit/revoke;
- admission stop, queue fence, drain, and the atomic old-generation
  retirement/epoch/candidate-install replacement transaction;
- QMP command send/reply/event and monitor disconnect;
- terminal-state publication, event notification, and cleanup.

For each point record expected state, current generation, whether the candidate is
consumed, whether replacement/retirement committed, the before/after epoch, live
owner count, accepted new work, old work isolation, resources to unwind,
tombstones retained, external event, public error, and retry behavior.
Pre-acceptance failure consumes no candidate. Every failure after accepted
reservation consumes it. Every failure before the identity transaction leaves
epoch and current generation unchanged; reset/recover commit old retirement,
epoch advancement, and candidate installation together. A later fault marks that
committed candidate `LOST`.

## Concurrency matrix

Race reset/remove/recover with duplicate and conflicting requests, transport
loss, daemon restart, worker/backend death, QEMU exit, cdev close/VMA fault, DMA
unmap, PCI config/rescan, provider calls, telemetry reads, queue submit, and late
completion. Use deterministic barriers/hooks to hit commit boundaries rather
than relying only on random timing.

For process-view publication, admission and telemetry races, compose the runtime
owner and read [view races](view-races.md). Those checks apply to the affected
shared boundary; they are not a prerequisite to every unrelated adapter edit.

## Invariants

- At most one live generation and one worker lease per logical device.
- Generation high-water never decreases or reuses an accepted candidate. Epoch
  changes only at irreversible retirement, by exactly one per retired generation.
- Reset/recover never expose a state between old retirement and candidate
  installation. Every current `ONLINE` or `LOST` identity names one committed,
  not already-retired generation.
- No stale callback publishes completion, telemetry, IRQ, or state into a new
  generation.
- Every accepted request reaches complete `ONLINE`, `LOST`, or `ABSENT` by its
  public deadline.
- Cleanup is exactly once; retry is idempotent; uncertain external state is
  reconciled rather than assumed.
- Provider-view races never add an ordinal to initialized CUDA, never expose an
  addition inside the current NVML init epoch, and never create a second process
  `registry_view_id`.
