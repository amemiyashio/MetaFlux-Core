---
name: runtime-contracts-registry
description: Implement or review ecosystem-neutral registry and provider views, client negotiation, shared-memory layouts, backend C ABI, and canonical schema generation. Use across milestones for milestone-0.1.0.0 runtime/contracts, milestone-0.1.1.0 data-plane schema ownership, milestone-0.1.2.0 process-view lifecycle, or milestone-0.1.3.0 neutral external-memory ABI. Do not use for ecosystem API semantics, target lowering, or layer-local transport mechanics.
---

# Runtime Contracts and Registry

## Implementation Focus

For an implementation request, use the shared
[implementation guidance](../review/references/implementation-guidance.md).
Select the affected inputs and obligations below; broad qualification lists
do not make every invocation a new inventory or full-suite run.

Trace one real producer-to-consumer request or state transition. Implement the
canonical field/lifetime change, generated projections and affected consumers
as one coherent unit; identify the first owner still missing behavior. Reuse
unchanged negotiated contracts and frozen schemas. For a warm-path change,
account for actual copies, registration, locking and dispatch rather than
adding a parallel request representation or reporting only layout-test passes.

## Inputs

- The active contract-owning work item (for example work-item-0.1.0.2, work-item-0.1.1.1/work-item-0.1.1.4,
  work-item-0.1.2.1, or work-item-0.1.3.1/work-item-0.1.3.2/work-item-0.1.3.6) and every consumer affected by the contract or
  process-view change.
- The canonical schema, generated-artifact manifest, registry/view policy,
  client-protocol versions, shared-layout definitions, and backend ABI versions.
- C/C++ layout results, encoded-byte fixtures, negotiation traces, queue stress
  results, and ABI compatibility evidence for the supported build matrix.

The `contracts/` source is ecosystem-neutral and points outward. CUDA, NVML,
PTX, a transport, a kernel module, or a backend may consume a contract but may
not privately redefine it.

## Routing

- Use [contract zones and schema ownership](references/contract-zones-and-schema.md)
  when choosing the canonical source and generated consumers.
- Use [registry views](references/registry-views.md) for identity, ordering,
  provider join/freeze, generation, and snapshot behavior.
- Use [shared fast path and backend ABI](references/shared-fast-path-and-abi.md)
  for fixed layouts, atomics, queues, negotiation, and plugin ownership.
- Route CUDA/NVML-visible behavior to their compatibility skills, PTX meaning to
  `$ptx-simt-semantics`, target compilation/execution to the target backend,
  wire/DMA details to the transport skill, and replacement identity to
  `$device-lifecycle-resilience`.

## Workflow

1. Name the crossing boundary and select exactly one contract zone before
   defining fields: in-process plugin ABI, encoded protocol, shared memory, or
   Linux UAPI.
2. Identify one canonical schema owner and enumerate every generated header,
   encoder/decoder, layout assertion, documentation table, and test vector.
   Reject hand-maintained copies in consumers.
3. Specify persistent logical identity, live-incarnation identity, ordering,
   provider join/freeze revisions, loss, addition, snapshot freshness, and
   stale-generation behavior independently of CUDA/NVML APIs.
4. Define version negotiation, `struct_size`, capability bits, extension rules,
   reserved-field handling, downgrade behavior, and ownership/lifetime for every
   contract family.
5. For mapped data, define byte order, width, alignment, cache-line placement,
   atomic access, publication/acquire edges, doorbell arming, wrap behavior, and
   process-death recovery before implementing a queue.
6. Keep client-protocol and backend-ABI versions independent. Ensure the
   application-side C17 fast path remains free of C++/LLVM/backend types and that
   backend calls cross only the versioned C function table.
7. Generate artifacts, build every consumer against them, and run cross-language,
   cross-version, shortened-counter, multiprocess, and failure tests.

## Output

Select the applicable outputs for the requested task:

- A boundary/owner table naming the canonical schema and every generated output.
- Registry-view, snapshot, negotiation, compatibility, and lifetime contracts.
- Fixed byte/offset/alignment/atomic tables for encoded or mapped records.
- Exact qualification commands and archived artifacts, or an explicit `Missing
  harness` item naming the owner and required evidence.

## Verification

- Compare generated C and C++ size, alignment, offset, enum, reserved-field, and
  byte fixtures on every supported build; include native/compat UAPI where used.
- Exercise compatible, older, newer, truncated, unknown-extension, malformed,
  and unsupported-capability negotiation without reinterpreting native structs.
- Stress provider join/freeze, `CUDA_VISIBLE_DEVICES` isolation, zero/one/many
  devices, loss/addition, generation replacement, identity/generation/epoch
  exhaustion without wrap, ordered view-global lifecycle-sequence allocation,
  abandoned-writer recovery, terminal exhaustion, same-revision parity,
  cross-revision divergence, and stale handles.
- With shortened lifecycle counters, reserve a range that fits exactly and one that
  crosses reserved `MAX`. Require whole-range checked allocation, no truncation or
  partial publish, pre-accept failure without side effect, and accepted/external
  event conversion to atomic mapping close.
- Split `RANGE_RESERVE` across range-slot initialization, stable high-water/tail
  commit, range marker, publisher release, and caller acceptance. Split
  `RANGE_RETIRE` across whole/partial suffix disposition and head advance. Require
  exact recovery, no unexplained gap, and next `max(begin, cursor + 1)` from the
  immutable retirement ledger for abort-before-first and partial suffix cases.
- With shortened view-control latch counters, leave one and two complete pairs.
  Also exhaust ordinary view-publish slots/tags. View creation must reserve two
  dedicated close records/tags with the two pairs. A settled-writer close publishes
  stable `CLOSING` and `TERMINAL`; a possibly executing expired writer instead
  publishes terminal `ViewAdmissionControl`, consumes no pair, and quarantines the
  mapping. Neither branch may leave a live/reusable intermediate view.
- Race loss/removal fence publication against telemetry bank publication, retry,
  and fallback; no call after observing the fence may recover stale `ONLINE`
  liveness from telemetry.
- Tear every multi-field fence write between fields. Require the reader's odd/even
  fence-latch copies plus outer device/view rechecks to return one whole commit or
  retry, never mixed identity, state, epoch, policy, or lifecycle sequence.
- Tear view-control writes too; stable control snapshots plus the independent
  view-admission latch must reject mixed gate state/generation/cursor fields.
- Race independent device-latch close against an older fence writer. Fence payload
  publication must never copy/reopen device admission, and readers must observe the
  closed latch even while the old payload commit completes.
- Exhaustively interleave A partial publish, B publish attempt, A complete or
  abort/compensate/skip, A abort-before-first, B ordinary loss/deadline, owner
  death, and mapping-terminal close with shortened sequences. Assert head-only
  publication, loss-triggered close when B is blocked, permanent suffix retirement,
  wakeup/progress, no live intermediate mirror, atomic admission invalidation, and
  no accepted lower write after close. Residual stores are allowed only in a
  terminal quarantined mapping that is never read or reused.
- Split `ViewPublishRecord` at `FREE`/`INITIALIZING`/`PREPARED`/`LINKING`/`ACTIVE`,
  fence stable-even, control cursor, `PUBLISHED`, ownership release, and terminal.
  Require one tagged publisher, kind-specific validation, exact reconcile without
  fence/range replay, no duplicate mutable token position, proven-death helping,
  and live-owner deadline quarantine.
- Interleave control acquire, fence/telemetry read, terminal close, and final
  device/view control recheck. No read-only validation or telemetry call may
  return stale `ONLINE` from a changed, closing, or terminal latch generation.
- Interleave an `OPEN`-to-`OPEN` quota/policy fence update with stable readers and
  `RESERVED`/`COMMITTING` leases. Device validation must pass through tagged
  `UPDATING`, resolve every old-generation lease, and reopen only after the new
  whole fence; reads linearize within their stable copy, not at final recheck.
- Interrupt that update at every step with loss/view close. A stale updater's exact
  tagged reopen CAS must fail after monotonic `CLOSED`; generation never decreases
  and no old update reopens a lost device.
- Split update-record `FREE -> INITIALIZING -> PREPARED -> LINKING -> ACTIVE ->
  FENCE_PUBLISHED` at every step. Helpers never read initializing payload or apply
  merely prepared policy; accepted linking is completed against the exact latch.
  Kill the updater at each split without another event and require proven-death
  helping. A deadline with a possibly executing owner closes/quarantines instead
  of reusing its record; `UPDATING` may not persist in a live view.
- Shorten view/device validation generations and require normal updates to preserve
  their terminal encodings, so loss/close remains representable without wrap.
- Interleave the shared admission record's reserve/committing/commit/publication
  states with device loss and view close, including owner death and a half-written
  target. Require helping, revocation, drain/cancel/tombstone, and bounded final
  state. Reuse a terminal slot while a stale owner/helper is paused and require a
  tagged-state CAS failure; never permit half-commit, ABA, or late side effects.
- Publish an attempt `ENTERING` with seq-cst before its final `OPEN` reads, pause it
  before and during `FREE -> INITIALIZING -> RESERVED`, and race update/close's
  seq-cst control transition, quiescence scan, and lease scan. Require READY/EXITED
  quiescence, exact-open recheck before COMMITTING, and no missed late lease.
- Assert admission latches expose no mutable lease head/index. During bounded-table
  scan, reuse terminal lifecycle-range/view-publish/attempt/lease/update/telemetry-
  publish slots around hazard publication and require full tag/state revalidation.
  Initializing/linking/active slots and a timed-out owner that may resume payload
  stores remain quarantined until quiescence/proven death.
- Shorten telemetry latch and snapshot-sequence widths, pause a reader, publish
  through two bank cycles, and approach exhaustion. Require atomic bank/control
  payload words, identical even-latch bracketing, no mixed-bank acceptance, and
  view close before either terminal encoding or the inactive bank is consumed.
- Race two telemetry publishers through record initialization/linking and expected
  even-to-odd CAS; exactly one owns the bank. Two stale prepared targets must make
  the later owner abort/release before bank touch and retry. Kill its process at
  every store/even/marker/ownership-release boundary and reconcile without replay.
  For a live but expired owner, require view/bank quarantine with no helper write
  or reuse until owner quiescence/death.
- Close a view, negotiate a fresh mapping, and replay stale tokens, handles, leases,
  and reader snapshots. The never-reused full view incarnation must reject all of
  them even if gate and lifecycle counters restart.
- Stress queue wrap, false sharing, lost wakeups, process death, and concurrent
  producers/consumers under the active work item's syscall and ordering gates.
- Prove every public milestone-0.1.1.0 data-plane layout is generated from the selected
  canonical schema and that no kernel, server, provider, or backend copy drifts.
