---
id: W0102
delivery: 0.1.0.2
milestone: M0100
status: Complete
area: contracts-runtime
depends_on: [W0101]
updated: 2026-08-30
---

# Contracts and Runtime Fast Path

## Outcome

Qualify the ecosystem-neutral registry, shared-memory queue, client protocol, and
backend C ABI used by the first CPU-backed device.

## Contract Requirements

`VirtualDeviceRecord` is a logical aggregation, not a wire or shared-memory
structure. Each field has exactly one ABI owner:

- an immutable `VirtualDeviceIdentity` record owns `identity_record_id`, logical
  ID/GPU UUID, display name, stable domain-local BDF, virtual compute capability,
  backend/capability identifiers, and the committed generation;
- one `RegistryViewControl` owns `control_latch_sequence`, `registry_view_id`,
  selection policy/default order, the 64-bit allocation high-water/publication
  cursor, ordered reservation head and next-committable state, gate generation/
  state, and mapping-terminal flag;
- a separate `ViewAdmissionControl` references `registry_view_id` and owns the
  view-wide validation generation/state (`OPEN`, `CLOSING`, `TERMINAL`, or
  `QUARANTINED`) as one atomic tuple; it is not control payload and is the
  reader-visible terminal authority even if a payload writer stalls;
- a separate tagged atomic `ViewPublisherControl` owns exclusive fence/control
  writer authority and references one bounded `ViewPublishRecord`; that record
  binds operation kind (`NORMAL`, `RANGE_RESERVE`, `RANGE_RETIRE`,
  `CLOSE_CLOSING`, or `CLOSE_TERMINAL`), full view ID, gate/token generation,
  immutable range, expected/target cursor and fence/control bytes/hash, owner
  incarnation/deadline, recovery plan,
  and tagged `FREE`/`INITIALIZING`/`PREPARED`/`LINKING`/`ACTIVE`/`PUBLISHED`/
  `ABORTED`/`TERMINAL`/`QUARANTINED` state; view creation permanently reserves
  two dedicated records/tags for `CLOSING` and `TERMINAL`, unavailable to normal
  publications;
- a bounded `LifecycleRangeRecord` owns immutable range start/end, transaction,
  token tag/deadline, and tagged open/retired terminal disposition including its
  permanently unused suffix; initialization is `FREE -> INITIALIZING -> PREPARED`,
  reservation commits `OPEN`, and it has no mutable per-token publish position;
- a `VirtualDeviceLifecycleFence` references `identity_record_id` and owns the
  `fence_latch_sequence`, latest committed `lifecycle_sequence`, state, epoch,
  effective quota, and policy used for liveness/admission;
- a separate `DeviceAdmissionControl` references `identity_record_id` and owns the
  per-device validation generation/state (`OPEN`, `UPDATING`, or `CLOSED`) and
  update tag as one tagged atomic tuple; it is not fence payload;
- a bounded `DeviceValidationUpdateRecord` owns update tag, old/new validation
  generation, an exact `(view_publish_slot, publish_tag)` foreign key, owner
  incarnation, deadline, update-link/reopen recovery plan, holder/hazard references,
  and a tagged atomic `FREE`/`INITIALIZING`/`PREPARED`/`LINKING`/`ACTIVE`/
  `FENCE_PUBLISHED`/`REOPENED`/`ABORTED`/`CLOSED`/`TERMINAL`/`QUARANTINED` update state;
- a bounded, pre-registered `AdmissionAttemptRecord` owns one actor's full view/
  identity/generation tuple, owner incarnation, deadline, target lease slot/tag,
  and tagged atomic `IDLE`/`ENTERING`/`CLAIMING`/`INITIALIZING`/`READY`/`EXITED`
  phase used for admission quiescence;
- a bounded `AdmissionLeaseRecord` references one view and identity record and owns
  lease ID, both latch generations, owner incarnation, operation/target cookie,
  deadline, recovery plan, holder/hazard references, and one lock-free atomic
  `(lease_tag, state)` word;
- one multiword `TelemetryControl` owns a dedicated 64-bit odd/even
  `telemetry_latch_sequence` plus atomic payload words for the full 64-bit
  `snapshot_sequence`, active bank, and telemetry state;
- a separate tagged atomic `TelemetryPublisherControl` references one bounded,
  pre-materialized `TelemetryPublishRecord` with owner incarnation, deadline, old
  and intended latch/control payloads, operation kind (`NORMAL` or `TERMINAL`),
  target snapshot sequence/bank, immutable staging source/hash, and finish/abort/
  reconcile plan plus tagged `FREE`/`INITIALIZING`/`PREPARED`/`LINKING`/`ACTIVE`/
  `PUBLISHED`/`ABORTED`/`TERMINAL`/`QUARANTINED` state; view creation reserves one
  dedicated terminal record/tag unavailable to normal telemetry;
- a `VirtualDeviceTelemetrySnapshot` row references `identity_record_id`, carries
  the lifecycle sequence it observed, and owns atomic-word commit counters and
  metrics. Dynamic records use identity foreign keys and never repeat UUID, BDF,
  generation, or other identity fields.

Identity records are immutable while published and `identity_record_id` values are
never reused for a replacement generation. The lifecycle fence is a separately
published release/acquire control record. Its writer, while holding the view gate,
sets a dedicated `fence_latch_sequence` odd, writes the complete fixed-layout
fence payload, then release-publishes the next even latch value. It never writes,
copies, or reopens `DeviceAdmissionControl`. A reader acquire-loads an
even latch, copies every fence field, and acquire-loads the same even value again;
odd or changed values retry within the contract's bound. `lifecycle_sequence` is
payload and never doubles as this latch. Loss/removal advances it and publishes
the non-`ONLINE` state before the terminal transition is observable. Latch values
use checked addition and never wrap; inability to reserve the next odd/even pair
closes the device latch and starts view close before any partial fence write.

Every concurrently accessed fence payload unit is a naturally aligned 32/64-bit
word read or written through `mf_atomic_*`; payload accesses may be relaxed only
inside the dedicated seqlock begin/end wrappers, which supply the required compiler
and hardware read/write barriers. Subword values occupy a containing atomic word.
Plain C/C++ loads, stores, structure copies, and `memcpy` never race a publisher.
The separately atomic device control may close concurrently with a fence writer;
the writer cannot overwrite it, and the reader's outer device recheck detects it.
`RegistryViewControl` uses the same data-race-free odd/even stable-copy protocol
for its multiword payload, while `ViewAdmissionControl` remains a separate atomic
record that control publication never overwrites.

Every handle-validation or admission call first acquire-reads an `OPEN`
`(registry_view_id, view_latch_generation)` admission snapshot, a stable
`(registry_view_id, gate_generation, gate_state)` control payload, and an
`(identity_record_id, device_validation_generation, device_validation_state)`
snapshot,
requires all states `OPEN`, and then reads and validates the lifecycle fence. It
accepts only the stable even fence copy above and reacquires view admission,
control, and device admission before returning. A changed ID/generation or any
non-`OPEN` state discards the result and returns the contracted lost/terminal-view
result. Closing a view/device latch and advancing its generation is therefore the
immediate read-side invalidation point even before a later control/fence write.
When those final checks pass, read-only validation linearizes at an instant within
the earlier stable fence copy; the final checks establish lifetime, not a later
policy linearization point. It can never combine fields from two control or fence
commits.

A call with admission side effects first uses a sequentially consistent CAS to
publish its exclusively owned, pre-registered attempt record as tagged `ENTERING`,
then performs the final sequentially consistent reads of the exact `OPEN` view/
device tuples. Only then may
it claim a generation-bound lease slot with `FREE(old_tag) ->
INITIALIZING(new_tag, attempt_tag)`, fill the immutable payload, and release-CAS it
to `RESERVED`; helpers never interpret lease payload while it is `INITIALIZING`.
The attempt advances through `CLAIMING`/`INITIALIZING` and may become `READY` or
`EXITED` only after the lease is fully visible or definitively tombstoned. Before
commit, the lease contains a fully materialized, idempotent descriptor/resource
publication target and recovery plan.

Every transition of a relevant view/device admission tuple away from `OPEN` uses
a sequentially consistent CAS, then scans and rechecks attempt phases with
sequentially consistent loads as part of the same total-order handshake. It waits/
helps all matching attempts that could have observed the prior `OPEN` until each is
`READY` or `EXITED`. An attempt published after that control transition must
observe the non-`OPEN` tuple and exit without a committable lease. Owner incarnation/
deadline plus the tagged attempt phase lets a helper determine whether the
advertised slot CAS won, finish initialization, or tombstone it; death before
complete recovery metadata never exposes `RESERVED`. Only after this quiescence
barrier does close, loss, or policy update scan the negotiated bounded central
lease table, filtering immutable full view/identity/generation IDs plus tagged
state. Thus no pre-close reserver or half-initialized slot can appear after the
stable scan.

View and device latches guard that same lease record rather than maintaining two
independently committable leases or mutable per-latch list heads.
Its tagged atomic state machine is `RESERVED -> COMMITTING -> COMMITTED ->
PUBLISHED -> RELEASED`, with `REVOKED` or generation-bound `TOMBSTONED` terminal
alternatives. Before `RESERVED -> COMMITTING`, the owner requires its attempt to be
`READY` and sequentially consistently revalidates the exact `OPEN` view/device
tuples; failure revokes the lease. If the control transition follows that recheck,
the quiescence barrier and central scan include the already visible lease. The
`COMMITTING -> COMMITTED` CAS is the admission linearization
point and the lease remains live through publication. Every owner/helper action
compares the expected lease tag and state in the same CAS; separately pre-reading
the lease ID is insufficient.

Close/loss first blocks new records in the applicable latch, then resolves every
record through that same atomic state: it revokes `RESERVED`; competes with or
helps `COMMITTING`; and drains, cancels, or helps finish `COMMITTED`/`PUBLISHED`.
If commit wins a CAS during close start, it linearizes before close completion and
is included in that drain. Owner incarnation and deadline make a dead holder
detectable. Publication writes a release marker last, so a helper can distinguish
unpublished from complete or half-published state and apply the target owner's
idempotent cancel/complete/tombstone rule. At the close/loss deadline, unresolved
records become generation-bound tombstones rejected by consumers and control
reaches its final state; no owner death can leave a live admission or permanent
`CLOSING`. Thus admission has one commit arbiter and no two-latch half-commit.

Every fence update that can affect admission, including state, epoch, effective
quota, or policy, participates in the same ordering. Before publishing it, the
runtime first claims an update slot with `FREE(old_tag) ->
INITIALIZING(new_tag)`, fills its immutable atomic-word payload, and release-CASes
it to `PREPARED`; helpers never interpret initializing payload. It then CASes that
record to `LINKING`. This record-state CAS is update acceptance: `PREPARED` may be
aborted, while `LINKING` is never reclaimed and must be helped to link or join a
monotonic close. The owner or helper then CASes `DeviceAdmissionControl`
from `OPEN(g, old_tag)` to
`UPDATING(g+1, update_tag)` (or a monotonic terminal `CLOSED`) with checked no-wrap
generation advance, preventing new lease commit. Success, or observing that exact
`UPDATING` tuple, advances the matching record to `ACTIVE`; an incompatible newer
tuple advances it only to the close/aborted terminal branch. A helper finding
`LINKING` never applies a merely `PREPARED` policy or cancels an accepted link while
the old tuple is still current: it performs the exact latch CAS itself. Every
owner/helper action revalidates the record tag/state and exact latch tuple, so a
paused owner cannot link a reclaimed record or mutate a later update. After the
attempt-quiescence barrier, the active update repeatedly resolves every
shared lease record bound to the old full tuple until none is live: a commit CAS
that wins during update start is ordered before the update and must be drained/
canceled; a losing record is revoked/helped. Through its embedded/referenced
`ViewPublishRecord`, the active update binds the full view ID, gate/token
generation, immutable FIFO range, intended lifecycle sequence, expected/target
cursor, and fence hash, and acquires the one exact `ViewPublisherControl`. While
that token is still the FIFO head, the owner publishes the exact stable fence
before advancing the cursor, then CASes `ACTIVE -> FENCE_PUBLISHED`.

Recovery under the gate stable-reads fence and cursor before acting. If both equal
the record's target, it only supplies the missing record CAS. If the exact target
fence is stable but cursor remains at the recorded predecessor, it advances that
cursor once and supplies the marker. If both remain at the predecessor, only the
same still-valid FIFO-head token may perform the original publish. Any mismatched
fence/cursor, invalid token, or loss of head authority monotonically closes the
view instead of replaying or skipping a sequence. Thus death between fence stable-
even commit, cursor advancement, and record marker is idempotently distinguishable.
It may reopen only with an exact tagged CAS from
`UPDATING(g+1, update_tag)` to `OPEN(g+1)` when the new fence permits admission, or
CAS to `CLOSED` otherwise. Loss/close monotonically CASes any state to a newer
`CLOSED` generation; if it interrupts the update, the stale reopen CAS fails and
the updater joins close recovery. No store may decrease generation or reopen a
different tag. `FENCE_PUBLISHED` records the only payload eligible for tagged
reopen; if exact reopen already happened before the terminal record marker, a
helper only marks that same-tag record terminal. If the updater dies without
another event, the tagged update record's owner
incarnation/deadline triggers recovery. A helper may finish the exact link/payload/
reopen sequence only after owner quiescence or proven incarnation death; a deadline
with a possibly executing owner monotonically closes and quarantines the view/
record until that owner quiesces or dies. Update-record slots use the same tag/
hazard no-ABA reclaim rules as leases. Generation exhaustion
closes the device/view rather than reusing a value. The final validation-generation
encoding is reserved for terminal `CLOSED`;
normal `OPEN`/`UPDATING` transitions never consume it, so loss/close is always
representable. Thus new policy cannot overtake an old-policy lease or later loss.

Telemetry rows use two banks protected by `TelemetryControl`'s dedicated 64-bit
odd/even latch. Exactly one writer owns a publication at a time through the tagged
CAS on `TelemetryPublisherControl`. Before acquiring it, the writer fully
claims a publish slot with `FREE(old_tag) -> INITIALIZING(new_tag)`, fills the old
stable control payload and either an immutable intended bank image or an idempotent
source/recovery plan, release-CASes the record to `PREPARED`, transitions it to
irrevocable `LINKING`, and then CASes publisher
control from `IDLE` to `WRITING(record_tag)`. A helper completes that exact link;
neither it nor a resumed writer may point publisher control at a reclaimed or
terminal record. After exact ownership, it reacquires the view and telemetry
control. A `NORMAL` record requires `OPEN` view admission and its expected even
latch/control; a stale normal target before bank touch tag-CASes to no-side-effect
`ABORTED`, releases exact ownership, and retries fresh. A `TERMINAL` record uses only
the dedicated tag, requires the exact non-open close generation and expected even
telemetry predecessor, sets terminal control without selecting/writing a bank, and
never follows the normal stale-retry path. Odd or structurally inconsistent state
quarantines the view. Otherwise ownership advances the record to `ACTIVE`. A normal
active record checked-adds both the no-wrap
snapshot sequence and one complete
latch pair, CASes the expected even latch to odd before touching the inactive bank,
writes that bank and the control payload only through aligned relaxed
`mf_atomic_*` words, then release-publishes the next even latch, CASes the record
`ACTIVE -> PUBLISHED`, releases exact tagged publisher ownership, and finally marks
the record terminal. Readers bracket the whole fence/bank
operation with the same view-and-device acquire/recheck protocol, acquire an even
telemetry latch, copy the atomic control payload, read its bank, resolve every row
through `identity_record_id`, and accept only if a second acquire returns the same
even latch. They retry at most twice. Plain concurrent bank/control copies are
forbidden. The final snapshot-sequence encoding and final latch pair are reserved;
inability to retain one complete next publication starts view close before making
the latch odd or touching the inactive bank. Recovery first stable-reads the exact
latch, control payload, publisher tag, and (when selected) intended bank hash. If
the target even latch/control/bank already committed while the record remains
`ACTIVE`, it only supplies `PUBLISHED` and releases ownership; a `PUBLISHED` record
still referenced by `WRITING(same_tag)` requires only exact ownership release and
terminal marking; ownership already released requires only terminal marking. It
never republishes the bank. If `ACTIVE+WRITING(same_tag)` still has the exact old
even latch/control, protocol ordering proves no bank was touched; after proven
owner death a helper CASes it to `ABORTED`, releases exact ownership, and marks it
terminal. A live expired owner takes the quarantine branch because it may be about
to CAS odd. If the latch remains odd, only proof that the publisher process/
owner incarnation has died permits a helper to touch its target bank and either
finish the intended bank/control commit or restore the old control payload and
publish the next even latch while leaving the partial inactive bank unselected. A
deadline with an owner that may still execute instead closes view admission and
quarantines the old mapping and both banks without reuse until that owner explicitly
quiesces or is proved dead; no helper writes the bank or releases publisher
ownership in that interval. A stale owner loses its publisher-tag/latch CAS, and
any residual store is confined to a closed, non-reusable view. Thus two publishers
in a live view never touch one bank concurrently, odd cannot remain permanent in a
live view, and a slow reader cannot accept an ABA-wrapped mixed bank. This protocol
requires only the guaranteed lock-free 64-bit atomics, not a 128-bit CAS.

Readers may return a prior complete telemetry snapshot only
where the API has an explicit maximum-age rule and only when its recorded
lifecycle sequence is not older than the fence observed at call entry. Otherwise
they return the contracted stale/unavailable result. Missing identity, mixed
sequence, changed/non-`OPEN` latch, or a prior `ONLINE` state after an observed
loss is never returned as a partial fallback.

The mapping negotiates a hard record/segment limit. The daemon may reclaim a
physical identity slot only after it is absent from the authoritative index, both
telemetry banks, the lifecycle fence, every provider membership revision, and all
generation-bound object/handle references; the reclaimed slot is published with a
fresh, never-reused `identity_record_id`. Physical record-capacity exhaustion
during add/reset/recover staging fails explicitly before identity commit, consumes
any already accepted lifecycle generation candidate, and never wraps or aliases an
old ID/generation. Every epoch increment is a checked add in its persistent
fixed-width representation.
Generation high-water and epoch capacity are acceptance guards: exhaustion returns
the stable overflow/resource error before candidate reservation or intermediate
state, leaving current state/identity/epoch unchanged and never wrapping.

The negotiated limit also bounds lifecycle-range, view-publish, admission-attempt,
admission-lease, device-update, and telemetry-publish records. A slot is reusable
only after its record is terminal and has no queue, resource, consumer, recovery,
holder, or hazard reference. A retired range remains until no later head/gap proof
or replay evidence can reference its immutable ledger entry.
`LINKING`, `ACTIVE`, and `INITIALIZING` are never reclaimable. An attempt slot also
requires `EXITED` and no target lease; a telemetry-publish slot requires publisher
control to no longer reference it, plus an even latch or a quarantined view whose
owner is proved quiescent/dead. A view-publish slot likewise requires
`ViewPublisherControl` to drop its exact tag and all affected payload latches to be
even, or the quarantined mapping owner to be proved quiescent/dead. A deadline may
make an operation logically terminal,
but a slot whose owner could resume payload stores remains physically quarantined;
only explicit owner quiescence or proven incarnation death permits reuse. Reuse
atomically installs a fresh no-wrap record tag and initial state; stale owners/
helpers still compare the old tag and fail. Capacity or tag exhaustion fails before
reservation with the contracted resource/retry result and never aliases a live or
recoverable record. Because lifecycle slow paths scan this bounded tagged table
after the attempt-quiescence barrier, there is no mutable per-latch head/index CAS
and no separate list-head ABA surface.

The two view-close publish records/tags and one telemetry-terminal record/tag are
minted with the full view ID at mapping creation, never enter the normal free pool,
and are consumed at most once. Creation fails if all reserved records, tags,
terminal generations, and latch pairs cannot be represented together. Normal
capacity/tag exhaustion therefore cannot remove the settled-writer close path; an
accepted/external operation that exhausts ordinary records starts close using only
the reserve.

`registry_view_id` is a never-reused mapping incarnation, encoded from a fresh
daemon incarnation plus a checked no-wrap per-daemon view serial. New mapping
negotiation always mints a different full ID even when storage or virtual addresses
are reused; exhaustion rejects creation. Tokens, handles, lease records, reader
snapshots, and provider membership revisions bind the full ID, so a fresh view may
restart its gate/sequence counters without accepting an old-view actor.

`lifecycle_sequence` is an unsigned 64-bit serial scoped to one
`registry_view_id`, and comparisons never cross view IDs. A single runtime-owned,
view-global publication gate maintains an allocation high-water plus a FIFO queue
of non-overlapping schema-bounded ranges. Reservation uses `RANGE_RESERVE`: it
claims/initializes the target `LifecycleRangeRecord`, records predecessor high-
water/tail and a checked complete target below reserved `UINT64_MAX`, then accepts
the linked view-publish record. Under exclusive publisher ownership, one stable
control commit advances allocation high-water and queue tail together to reference
the already complete range payload; a following record marker makes it `OPEN`.
That control commit is reservation linearization. Recovery seeing exact target
high-water/tail only supplies the range/publish marker; predecessor state plus an
accepted link completes the one control commit; any mixed/mismatched state closes
the view. It never creates a partial range or unexplained high-water gap.

Before authority accepts an optional operation, fit or pre-link capacity failure
returns the stable resource/overflow result with no side effect. A reserved range
whose optional caller disappears before authority acceptance is retired whole by
`RANGE_RETIRE`. For an already accepted operation or external authority event,
failure to complete reservation begins mapping close.

Every fence/control publication step first claims a `ViewPublishRecord` through
`FREE -> INITIALIZING`, fills immutable predecessor/target payload and recovery
metadata, release-publishes `PREPARED`, accepts `LINKING`, and links its exact tag
to `ViewPublisherControl`. Only `ACTIVE` exclusive ownership may set a fence or
control latch odd. After ownership, a `NORMAL` record requires exact `OPEN` view
admission, FIFO head/token, and predecessor cursor. `CLOSE_CLOSING` instead requires
the first dedicated tag, exact non-open close generation and predecessor control,
and targets the sentinel cursor; `CLOSE_TERMINAL` requires the second dedicated tag
and exact stable `CLOSING` predecessor. `RANGE_RETIRE` requires `OPEN`, the exact
head range, the authority-dependent abort/completion guard, and a predecessor
ledger/head; it targets that range's terminal suffix disposition followed by the
next head and does not invent a fence/cursor value. `RANGE_RESERVE` requires
`OPEN`, exact predecessor allocation high-water/tail, a checked complete target
  below reserved `MAX`, and a fully `PREPARED` range slot; it targets one stable
  high-water/tail commit that makes the whole range visible. A stale optional normal
  step before any odd/raw write can abort and release exact ownership, while an
  accepted/external or close-kind mismatch quarantines the view.
`NORMAL` stable-publishes fence then cursor; `RANGE_RESERVE` publishes high-water/
tail then range marker; `RANGE_RETIRE` publishes terminal range disposition then
head; close kinds publish only their exact control target. Each kind then CASes its
record to `PUBLISHED`, releases exact publisher ownership, and marks it terminal.
Device-validation update records reference this exact publish slot/tag;
all range/cursor/fence/control fields remain owned only by `ViewPublishRecord` and
are never duplicated in the update record.

Recovery stable-reads every affected latch/ledger record and exact payload hash. It
supplies only a missing cursor/head/range marker/ownership release when predecessor/
target state proves earlier stages committed; it never repeats an already stable
fence or range allocation. Only explicit owner quiescence or proven process-
incarnation death allows a helper to resume raw payload publication. A deadline
while the owner may execute atomically terminal-closes `ViewAdmissionControl` and
device admission,
then quarantines the mapping without stealing publisher ownership, touching an odd
payload, or reusing its storage. Residual stores are confined to that rejected view.

A token identifies its view ID, gate generation, immutable range, lifecycle
transaction, and deadline. It has no independently mutable next-position field:
the exact next value is `max(range.begin, checked(publication_cursor + 1))`, valid
only while the immutable queue/retirement ledger proves that range is head and all
predecessor ranges/suffixes are terminal. The cursor always means last actually
published lifecycle value; it never advances merely to skip a suffix. A later token
releases the gate and waits on the gate wake sequence or returns the contracted
retry result; it never writes ahead. Each head publish validates that derived value,
release-publishes the fence, and advances only the publication cursor. Thus no
duplicate mutable position exists, while a permanent ledger entry justifies every
gap between ranges.

A head token closes only after the mirror represents the transaction's final
authoritative state. A `RANGE_RETIRE` publication first release-commits the range's
terminal disposition and exact unused suffix, then stable-publishes the next head;
death between those steps is reconciled by supplying only the head advance. The
cursor stays at the last real fence. Abort before first publish may retire the
whole range only while authority is unchanged. After a partial publish, abort must
first publish a compensating authoritative fence, then retire its suffix. If that
cannot be done, the gate closes the mapping. Proven owner death allows exact-record
helping through compensation/retirement. Deadline expiry while the owner may execute
closes admission and quarantines instead of stealing raw-writer ownership, so no
live mapping retains an abandoned intermediate fence.

`UINT64_MAX` is reserved for mapping-terminal close, not ordinary single-device
loss. On authoritative loss notification, the runtime atomically sets per-device
validation to `CLOSED` and advances its generation, then resolves that device's
shared lease records and attempts a normal range under the view gate. If that
range is not immediately the FIFO head, or cannot publish `LOST` by the explicit
loss-publication deadline, the same gate begins mapping-terminal close; loss never
waits behind an unrelated token while either latch remains open.

Close first seq-cst CASes `ViewAdmissionControl` away from `OPEN` and monotonically
CASes every device validation tuple to its reserved terminal `CLOSED` generation.
That independent admission tuple is the immediate reader/token invalidation point;
normal view updates cannot consume its reserved terminal generation. It then
completes attempt quiescence and routes leases, update records, and publisher
records through their recovery/quarantine rules.

When view publisher ownership is idle, explicitly quiesced, or proved dead, close
uses the two dedicated close records/tags and independently reserved final two
control-latch pairs to stable-publish `CLOSING` with cursor `UINT64_MAX`/terminal
gate generation, then `TERMINAL` after drain/cancel/tombstone. Normal publication
never consumes those resources. A settled telemetry publisher may likewise use
its dedicated terminal record/tag and reserved final pair without touching a bank.

When a view or telemetry publisher may still execute raw payload stores, close
does not steal it or wait for a stable seqlock. By the public deadline it seq-cst
publishes `ViewAdmissionControl=QUARANTINED`/terminal, leaves any affected latch
odd or payload incomplete, and quarantines the whole old mapping, records, and
banks until explicit owner quiescence or proven incarnation death. Readers and
tokens reject from the independent admission tuple; a new view uses a new full ID
and storage. Thus stable `RegistryViewControl=TERMINAL` is required only on the
settled-writer branch, while every branch reaches reader-visible terminal admission
by deadline. A losing token performs no later accepted write; residual physical
writes in a quarantined mapping are never read or reused. The authoritative
lifecycle transaction continues outside that failed mirror. Sequence values never
wrap.

One process shares one selection policy and `registry_view_id` across CUDA and
NVML, while each provider captures a labeled membership revision at its own
permitted initialization boundary:

- The first active provider commits mode, transport, and `registry_view_id`
  before returning a device or handle.
- CUDA freezes its membership revision after successful `cuInit` until process
  exit.
- NVML freezes a membership revision on init refcount zero-to-one and releases it
  on matching shutdown.
- A later provider joins the existing view or reports incompatibility.
- Loss updates frozen-view state immediately; additions wait for a later NVML
  epoch or new process and never change an initialized CUDA ordinal set.
- Default unfiltered CUDA/NVML membership and order agree only when both captured
  the same process-view revision. Across different revisions, compare only common
  live incarnations by `(UUID, generation)`; UUID, BDF, or logical ID alone may
  correlate persistent identity but cannot establish live-incarnation parity.

Shared layouts contain fixed-width integers, byte arrays, offsets,
generation-bound handles, and explicit padding/alignment. They contain no
pointers, `size_t`, language `bool`, native enum, `std::atomic`, C `_Atomic`,
kernel `atomic_t`, STL object, packed atomic, or naturally unaligned atomic.

C and C++ use one `mf_atomic_*` wrapper over `__atomic_*`, including the explicit
fence-seqlock begin/end barriers above. Builds require aligned, lock-free 32/64-bit
operations and reject `libatomic`. Cursors occupy separate
64-byte cache lines; descriptors are exactly 64 bytes with per-slot sequence.
Empty-to-nonempty uses an armed/sleeping handshake and at most one doorbell.

`mf_backend_api_v1` covers enumeration/capabilities, compilation/loading,
context/queue/memory lifecycle, submit/copy/event/sync/cancel, metrics, and policy.
The allocating side frees. No exception, STL type, compiler class, or allocator
ownership crosses the ABI. Backend registration lives in
`contracts/plugin/backend/v1`; client negotiation lives independently in
`contracts/protocol/client/v1`. Statically linked v0.x backends are called only
through their C entrypoint/function table.

## Work

- [x] Implement fixed-layout C types and generated C/C++ size/alignment/offset
  assertions.
- [x] Implement registry and dynamic latch pages, generation-bound handles, and
  stale-handle errors; bracket handle/admission/telemetry reads with control
  acquire/recheck, use an odd/even latch for whole-fence snapshots, bind stateful
  admission to one helper-recoverable shared lease record, use helper-recoverable
  update records and bounded central-table scans without per-latch indices, protect
  telemetry banks with a no-wrap odd/even latch and atomic payload, race loss/
  terminal publication against lease commit and snapshot fallback, and prove no
  torn fence/bank or changed/closing/terminal view returns stale `ONLINE` liveness
  or late work.
- [x] Implement per-context rings, timeline completion, doorbell, and blocking
  futex/eventfd fallback.
- [x] Stress process death, generation replacement, shortened identity/generation
  and epoch exhaustion with no wrap or alias, shortened lifecycle-sequence FIFO
  reservations, head blocking, loss-triggered close, suffix retirement, abort
  compensation, owner/lease-holder death at every lease state, half-publication
  helping/tombstone, publish/terminal-close races, ordinary queue-counter wrap,
  NUMA, and false sharing across multiple processes.

## Exit Gate

One client and one daemon exchange one million no-op descriptors with correct
ordering, no lost wakeup, no global lock, and at most one wake syscall per
active memfd dispatch. The zero-syscall active-queue gate begins with the M0110
doorbell transports. C and C++ ABI assertions match on every supported build and
no `libatomic` dependency appears.
Short-width exhaustive fixtures additionally prove head-only range publication,
bounded completion/compensation/terminal close after owner failure, permanent
suffix retirement, token invalidation, admission commit/close linearization,
lease/update helping and deadline completion, tagged-slot and central-scan ABA
rejection, loss-deadline close, stable whole-fence and telemetry-bank reads,
telemetry latch/sequence exhaustion before wrap, device-latch close versus stale
fence writer safety, device/view-control close-reader recheck, and old-view replay
rejection.
