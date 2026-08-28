# Failure Injection

## Stage matrix

Inject before and after every durable or externally visible side effect:

- candidate allocation and high-water persistence;
- registry staging/publication;
- transport socket/node/mapping setup;
- PCI function/config publication and driver bind;
- backend creation and resource import;
- worker lease stage/commit/revoke;
- admission stop, queue fence, drain, and old-generation retirement;
- QMP command send/reply/event and monitor disconnect;
- terminal-state publication, event notification, and cleanup.

For each point record expected state, whether the candidate is consumed, live
owner count, accepted new work, old work isolation, resources to unwind,
tombstones retained, external event, public error, and retry behavior.

## Concurrency matrix

Race reset/remove/recover with duplicate and conflicting requests, transport
loss, daemon restart, worker/backend death, QEMU exit, cdev close/VMA fault, DMA
unmap, PCI config/rescan, provider calls, telemetry reads, queue submit, and late
completion. Use deterministic barriers/hooks to hit commit boundaries rather
than relying only on random timing.

## Invariants

- At most one live generation and one worker lease per logical device.
- Generation/epoch high-water marks never decrease or reuse a consumed value.
- No stale callback publishes completion, telemetry, IRQ, or state into a new
  generation.
- Every accepted request reaches complete `ONLINE`, `LOST`, or `ABSENT` by its
  public deadline.
- Cleanup is exactly once; retry is idempotent; uncertain external state is
  reconciled rather than assumed.
