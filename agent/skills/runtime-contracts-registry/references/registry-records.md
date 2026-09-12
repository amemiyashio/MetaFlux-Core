# Registry Records and Reclamation

Read when changing shared record fields, bounded tables, tags or storage reuse.
[Registry views](registry-views.md) owns identity and membership distinctions.
[Admission](registry-admission.md), [publication](registry-publication.md) and
[telemetry](registry-telemetry.md) own each record's operational transitions.

`VirtualDeviceRecord` is a logical join, not an ABI struct. Immutable identity owns
logical ID, UUID, name, domain-local BDF, capabilities, backend identity, and
generation. `RegistryViewControl` owns the shared view ID, selection/default-order
policy, `control_latch_sequence`, allocation high-water, publication cursor,
ordered reservation head/next-committable state, gate generation/state, and
mapping-terminal flag. A separate atomic `ViewAdmissionControl` owns the view
validation generation/state tuple and is the reader-visible terminal authority if
a payload writer stalls. A tagged `ViewPublisherControl` references a bounded
`ViewPublishRecord` containing exact token/cursor, intended fence/control, owner,
deadline, recovery, operation kind, and initialization/linkage state. Normal kind
requires `OPEN` plus FIFO head/token; range-reserve/retire kinds own queue control
commits; the two close kinds require dedicated tags and exact non-open close-
generation/`CLOSING` predecessors. Two records/tags
are reserved at view creation for `CLOSING` and `TERMINAL` and never enter the
normal pool. `DeviceAdmissionControl` owns identity, device
validation generation/state, and update tag as one atomic tuple; its states are
`OPEN`, `UPDATING`, and `CLOSED`. Neither admission record maintains a mutable
lease head/index, and neither is control/fence payload. A bounded tagged
`DeviceValidationUpdateRecord`
owns the old/new validation generations, update tag, an exact `(view_publish_slot,
publish_tag)` foreign key, owner incarnation/deadline, update-link/reopen recovery,
hazards, and tagged `FREE`/`INITIALIZING`/`PREPARED`/`LINKING`/`ACTIVE`/
`FENCE_PUBLISHED`/`REOPENED`/`ABORTED`/`CLOSED`/`TERMINAL`/`QUARANTINED` state.
Range/cursor/fence/control fields remain solely
owned by `ViewPublishRecord`. Pre-registered `AdmissionAttemptRecord`
slots own actor/view/identity,
deadline, target lease slot/tag, and the tagged phase used by the reservation
quiescence handshake. A lifecycle fence owns liveness/state, epoch, effective
quota/policy, a dedicated `fence_latch_sequence`, and its latest committed
`lifecycle_sequence`, but not admission control.
A bounded `LifecycleRangeRecord` owns immutable start/end, transaction, token tag/
deadline, and open or terminal unused-suffix disposition; it has no mutable publish
position.
A bounded admission-lease table owns records keyed by view and identity, with both
latch generations, owner incarnation, deadline, publication target/recovery
cookie, and one atomic tagged state plus holder/hazard references. A telemetry row
owns commit counters/metrics, carries the fence sequence it observed, and otherwise
has only an `identity_record_id` foreign key. A multiword `TelemetryControl` owns a
dedicated 64-bit odd/even latch plus atomic payload words for the full 64-bit
no-wrap snapshot sequence, active bank, and telemetry state. A separate tagged
`TelemetryPublisherControl` references a bounded `TelemetryPublishRecord` with
owner incarnation, deadline, expected/target latch and snapshot sequence/bank,
old/intended control, immutable staging source/hash, and finish/abort/reconcile
plan plus normal/terminal operation kind. One dedicated telemetry-terminal record/
tag is reserved at view creation; it validates non-open close state and never writes
a bank. Tagged publish states are `FREE`/`INITIALIZING`/`PREPARED`/`LINKING`/
`ACTIVE`/`PUBLISHED`/`ABORTED`/`TERMINAL`/`QUARANTINED`.

Identity values never wrap or get reused, but physical record storage is bounded.
A slot is reclaimable only after no authoritative index, telemetry bank,
lifecycle fence, provider membership revision, or generation-bound object
references its prior record. Slot
reuse publishes a fresh ID, and exhaustion returns the contract's explicit
resource error before lifecycle identity commit rather than aliasing an old
incarnation. Epoch advancement uses checked addition; exhaustion rejects the
retirement before commit and preserves current identity/epoch rather than wrapping.

Lifecycle-range, view-publish, attempt, lease, update, and telemetry-publish storage
is also bounded.
Reclaim requires terminal record state plus no queue, target slot, publisher-
control, resource, recovery, holder, or hazard reference. `INITIALIZING`, `LINKING`,
and `ACTIVE` are not reclaimable. View/telemetry publish records also require the
exact publisher tag released and affected payload latches even, or a quarantined
mapping whose owner is quiescent/proved dead. A deadline may close the operation/
view, but physical reuse waits for explicit owner quiescence or proven incarnation
death whenever the owner could resume payload stores. Reuse atomically installs a
fresh tag with its initial state; a paused stale owner/helper compares its old tag
and fails.
Capacity/tag exhaustion fails before reservation and never aliases a record. Table
scans acquire a hazard reference and revalidate full tag/state after publication,
so record reuse has no secondary list-head ABA surface.
A retired range record remains while any later head/gap proof or replay evidence
can reference its immutable disposition.
View creation succeeds only after reserving the two view-close publish records/
tags, one telemetry-terminal record/tag, terminal generations, and all final latch
pairs together; normal publication cannot consume this reserve.

## Affected scenarios

Select the scenarios for the mechanism being changed and the active work item's
required qualification; run them after the coherent implementation.

- Assert admission latches expose no mutable lease head/index. During bounded-table
  scan, reuse terminal lifecycle-range/view-publish/attempt/lease/update/telemetry-
  publish slots around hazard publication and require full tag/state revalidation.
  Initializing/linking/active slots and a timed-out owner that may resume payload
  stores remain quarantined until quiescence/proven death.

Canonical source: [work-item-0.1.0.2](../../../plan/milestone-0.1.0.0-core-foundation/work/work-item-0.1.0.2-contracts-runtime.md).
