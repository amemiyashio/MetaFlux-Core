---
name: device-lifecycle-resilience
description: Implement or review generation and epoch state machines across cdev, vfio-user, vPCI, registry, workers, QMP events, reset, remove, re-add, provider enumeration freeze, tombstones, idempotence, deadlines, and fault injection. Use for the milestone-0.1.2.0 lifecycle authority and any backend or transport adapter, including milestone-0.1.3.0 device loss. Do not use for layer-local wire layouts or target compiler/runtime behavior.
---

# Device Lifecycle Resilience

## Implementation Focus

For an implementation request, use the shared
[implementation guidance](../main/references/implementation-guidance.md).
Select the affected inputs and obligations below; broad qualification lists
do not make every invocation a new inventory or full-suite run.

Select the concrete transition blocking the assigned adapter. Reuse the
canonical model and implement its guard, side effect, completion and stale
object behavior through the affected layers. The mandatory model prerequisite
still applies; after it passes, implement the adapter and its meaningful fault
boundary instead of repeatedly rechecking an unchanged model. A model-only
pass does not complete an adapter implementation.

## Inputs

- The active lifecycle-authority or adapter work item (for example milestone-0.1.2.0 core or
  milestone-0.1.3.0 device loss), frozen milestone-0.1.1.0 transport-envelope manifest, lifecycle-extension
  manifest and model, model bounds, registry authority, daemon incarnation,
  generation/epoch persistence, and deadline policy.
- Layer adapters for cdev, vfio-user, PCI/vroot, QMP, worker leases, providers,
  and backend resources affected by the transition.
- Event traces, request IDs, fault points, old-object/tombstone behavior, and the
  qualification kernel/QEMU matrix.

The daemon lifecycle authority alone reserves generation candidates, publishes
committed generations, and advances epoch in the atomic identity transaction
that retires the current generation.
Transport and presentation layers mirror committed state and retain local
tombstones; they do not invent replacement identity or advance epoch.

## Routing

- Use [state machine](references/state-machine.md) for guards, commit points,
  identity, idempotence, deadlines, and tombstones.
- Use [model checking](references/model-checking.md) for canonical paths, bounded
  exploration semantics, the exact root-run command, and the evidence contract.
- Use [QMP events](references/qmp-events.md) for command/event correlation,
  asynchronous completion, disconnect, and reconciliation.
- Use [failure injection](references/failure-injection.md) for stage-by-stage and
  racing faults.
- Use [qualification](references/qualification.md) for model, integration, stress,
  and release evidence.
- Route layer-local mechanics to `$linux-device-driver-uapi`,
  `$gpu-virtualization-vfio-user`, or `$pcie-vpci-device-model`; route Vulkan
  device loss to `$vulkan-spirv-compute` as a paired target adapter.
- Lifecycle owns the loss/addition events delivered to provider views. Compose
  `$runtime-contracts-registry` whenever changing process-view membership,
  ordering, freeze epochs, or first-visibility rules; pure reset/transport work
  that only emits the existing events does not add that owner.

## Workflow

1. Write the canonical transition schema first at
   `contracts/protocol/transport/v1/schema/extensions/lifecycle/v1/model.json`.
   Its sibling extension manifest imports the frozen milestone-0.1.1.0 root manifest by
   content hash; the dependency never points back from the base manifest. Model
   source/event, valid source states, guards, request identity, owner, staged
   resources, irreversible retirement point, published intermediate states,
   terminal states, errors, and deadline. Generated adapters and fixtures are
   projections.
2. Bind every request to request ID, source, operation, UUID, expected generation,
   daemon incarnation, and deadline. Define duplicate, stale, and conflicting
   behavior before side effects.
3. Make checked capacity part of acceptance. Add/reset/recover is accepted only
   after durably reserving exactly one generation candidate; numeric high-water
   exhaustion rejects it with no candidate, state, or epoch change. Remove/reset/
   recover is accepted only after checked-add proves one epoch increment fits and
   the authority serializes that retirement right. Epoch is not advanced by this
   guard. Rejection publishes no intermediate state. Once accepted, a candidate
   remains consumed even if later staging or pre-transaction work fails; a
   duplicate consumes none.
4. Stage candidate identity, transport, presentation, backend, and exclusive
   worker lease off-registry. Admin-visible `PRESENT`, `QUIESCING`, `DRAINING`,
   or `RESETTING` may describe transaction progress but never make the candidate
   current or provider-enumerable.
5. For reset/recover, reject new work first, fence queues/completions, drain to
   deadline, then atomically retire the old generation, advance epoch exactly
   once, and install the fully staged candidate as current `ONLINE`. A
   pre-transaction failure leaves old identity and epoch unchanged; a later fault
   marks the committed candidate `LOST`. Retain old fd/VMA/DMA/queue/event/memory/
   module/pipeline/handle objects as generation-bound tombstones. Remove uses an
   atomic retirement/epoch commit to `ABSENT`; add commits a staged candidate
   without advancing epoch.
6. Commit only the epoch increment reserved by the acceptance guard. It is exactly
   `epoch_before + 1`, never wraps, and cannot fail for capacity after an operation
   has entered `QUIESCING`/`DRAINING`/`RESETTING`. Thus every accepted remove still
   reaches `ABSENT` by its public deadline.
7. Normalize admin, vfio-user, QMP, disconnect, daemon restart, worker death, and
   backend/device loss into the same authority model. Reconcile missed external
   events by querying state, not by guessing.
8. Emit loss/addition events into the runtime-owned process-view contract.
   Existing rules update frozen entries to lost immediately, never create a CUDA
   ordinal in an initialized process, and admit additions to NVML only after a
   later zero-to-one init epoch or in a new process. Compose the runtime owner if
   any membership, ordering, or visibility rule changes.
9. Run the mandatory bounded model command from
   [model checking](references/model-checking.md). Missing model, bounds, checker,
   or JSON evidence is a failed gate, not an optional omission.
10. Generate positive, invalid, duplicate, stale, reordered, racing, timeout, and
   injected-failure adapter tests from the same transition schema.

## Output

Select the applicable outputs for the requested task:

- The canonical transition schema, generated transition/ownership/commit table,
  versioned exploration bounds, and machine-readable model-check evidence.
- Per-layer prepare, commit, abort, revoke, drain, tombstone, and reconcile
  obligations with exact public errors/events.
- Lifecycle event/input rows for the runtime-owned CUDA/NVML process-view table,
  covering removal, reset, re-add, and committed generation identity; jointly
  update that table with `$runtime-contracts-registry` when its rules change.
- A fault matrix covering every side-effect boundary and late callback.
- Qualification evidence proving single ownership, monotonic identity,
  idempotence, bounded terminal states, and old-work isolation.

## Verification

- Run the exact repository-root command in
  [model checking](references/model-checking.md) before adapter implementation;
  assert one live owner, generation-candidate non-reuse, no pre-retirement epoch
  change, `epoch_after == epoch_before + 1` for every committed retirement,
  pre-accept generation/epoch exhaustion with no side effect or wrap,
  transport-loss epoch preservation, current-generation continuity, and no
  half-online terminal state. Reset/recover install the candidate in that same
  transition; every accepted remove reaches `ABSENT` in it.
- Test duplicate/stale/conflicting requests, daemon incarnation changes,
  persistence failure, missed/reordered QMP events, deadline expiry, and restart
  reconciliation.
- Inject failure before and after every allocation, publish, lease, drain,
  transport, PCI, backend, and registry commit point; verify exact unwind or lost
  behavior.
- Run reset/remove/re-add under concurrent open, mmap, DMA, submit, completion,
  telemetry, config access, QMP, daemon/server/worker loss, and module lifecycle.
- In one process, assert CUDA and NVML share one `registry_view_id`; loss updates
  old entries, re-add creates no CUDA ordinal, and NVML sees an addition only at
  the next permitted zero-to-one initialization epoch. Require count/order parity
  only for the same captured revision and match common live incarnations by
  `(UUID, generation)` across differing revisions. Repeat in a new process.
- With `$runtime-contracts-registry`, model FIFO range head blocking, completion,
  abort-before-first, partial compensation, suffix retirement, owner death,
  ordinary loss/deadline-triggered view close, single-record admission commit/
  helping/half-publication/tagged-slot reuse, and terminal close. Bracket read-only
  validation/telemetry with device/view acquire/recheck and a data-race-free stable
  odd/even fence copy; keep device admission outside fence payload. Stateful
  admission uses seq-cst attempt quiescence plus one recoverable tagged lease, and
  fresh view IDs reject old actors. Shortened sequence bounds must prove all-or-
  nothing range fit below reserved `MAX`, no pre-accept side effect, and post-
  accept/external-event close.
- Split `RANGE_RESERVE` at range initialization, high-water/tail stable commit,
  marker/release and `RANGE_RETIRE` at suffix disposition/head advance. Derive next
  as `max(range.begin, checked(cursor + 1))` with immutable gap proof. Then split
  normal view publication at fence commit, cursor, marker, and ownership release.
  Reconcile exact targets without replay; no separate token position exists.
  Proven-death/quiesced close consumes
  two dedicated records/tags and latch pairs for stable `CLOSING`/`TERMINAL`; a
  live expired writer reaches terminal admission by quarantine without consuming
  them or reusing the mapping.
- Model tagged `OPEN -> UPDATING -> OPEN/CLOSED` for every admission-relevant fence
  change: drain/revoke old-generation leases before new quota/policy is admissible,
  require loss/close to defeat every stale reopen without generation rollback, and
  split update record initialization/linking, fence/cursor commit, marker, and
  reopen; require exact-state reconciliation. Proven owner death may help, while a
  deadline with a possibly executing owner closes/quarantines the view.
- Keep admission latches free of mutable lease heads/indices; model bounded tagged-
  table scan, seq-cst attempt `ENTERING`/control transition/quiescence, all record
  `INITIALIZING` states, hazard revalidation, and lifecycle-range/attempt/lease/
  update/view-publish/telemetry-publish slot reuse. With shortened telemetry latch/
  sequence, model a slow reader across two
  bank cycles, exclusive publisher linkage, even-commit/record-marker recovery,
  proven-death helping, and live-owner deadline quarantine before either counter
  wraps or any bank is reused.
- Keep performance and 1,000-cycle promotion claims tied to archived milestone-0.1.2.0
  evidence; a state-model unit test is necessary but not release qualification.
