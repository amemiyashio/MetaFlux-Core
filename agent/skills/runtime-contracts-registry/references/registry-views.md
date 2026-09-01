# Registry Views

The daemon owns logical-device identity, policy, generation, and worker leases.
A process negotiates one shared `registry_view_id`; provider DSOs join that view
instead of keeping mutable per-DSO registry copies. The shared ID fixes selection
policy and default order; it does not claim that providers initialized at
different times always have identical membership.

Keep these views distinct:

- the authoritative registry and its stable default order;
- the process selection view shared by participating providers;
- provider membership snapshots, each labeled by provider, initialization epoch,
  and `process_view_revision`;
- CUDA's initialized ordinal set, which `CUDA_VISIBLE_DEVICES` may filter or
  reorder without changing NVML;
- NVML's zero-to-one initialization epoch;
- immutable per-generation identity records, each named by a never-reused
  `identity_record_id`;
- a separately published, monotonically sequenced lifecycle fence used for
  liveness, admission, state, epoch, effective quota, and policy;
- generation/sequence-tagged telemetry snapshots, which reference identity
  records and never duplicate UUID, BDF, generation, or lifecycle state fields.

Default unfiltered CUDA and NVML live membership/order must agree when both
providers captured the same `process_view_revision`. After CUDA filtering, or
when provider initialization revisions differ, count and ordinal parity are not
required. UUID or logical ID correlates a persistent device across replacement,
but live-incarnation parity requires `(UUID, generation)` from the same applicable
view revision; a stable BDF is not an incarnation key.

Loss marks the captured `identity_record_id` and generation lost immediately. A
re-add creates a new identity record and generation. It never enters an initialized
CUDA ordinal set and may appear in NVML only after the contract's next allowed
initialization epoch. An old CUDA entry and a new NVML entry can therefore share
UUID/BDF while intentionally having no live-incarnation parity.

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

Handle validation brackets its fence read with an `OPEN` view-admission snapshot,
a stable even view-control snapshot, and an `OPEN`
`(identity_record_id, device_validation_generation)` snapshot, requiring exact
matches at the final acquire. Inside that bracket it acquire-loads an even
`fence_latch_sequence`, copies the complete fence, and accepts it only when a
second acquire-load returns the same even value. Writers under the view gate mark
the latch odd and release-publish the next even value after every field; the
lifecycle sequence remains payload. Device-latch close is therefore immediate
read-side loss invalidation, and no reader accepts a torn fence. Latch pair
allocation is checked and never wraps; exhaustion closes the device/view before an
odd write. All concurrent payload words use aligned `mf_atomic_*` access inside the
seqlock wrappers, never plain loads or copies. Fence publication never copies or
writes the separate device control, so an old writer cannot reopen a loss-closed
latch. View control uses the same atomic-payload seqlock discipline and never
copies its separate admission control. Telemetry uses the same view-control/
admission/device/fence bracket around its bank read. Its control and bank payload
words are atomic, and readers accept a bank only when two acquire-loads return the
same even telemetry latch.
A successful read linearizes within its stable fence copy; final outer checks only
prove that no view/device validation transition invalidated that copy before return.

Stateful admission first seq-cst publishes its exclusively owned, pre-registered
attempt as `ENTERING`, then seq-cst reads the exact `OPEN` view/device tuples. It
claims a lease as `FREE -> INITIALIZING(attempt_tag)`, fills immutable recovery
metadata, and release-publishes `RESERVED`; helpers never interpret initializing
payload. Before `RESERVED -> COMMITTING`, it requires attempt `READY` and seq-cst
revalidates the exact open tuples. A control transition away from `OPEN` uses a
seq-cst CAS, then seq-cst scans/helps all attempts that could have observed the old
tuple to `READY` or `EXITED` before scanning leases. A later attempt must observe
non-open and exit. Thus neither a paused pre-close reserver nor an owner dying in
slot initialization can appear after quiescence.

One lease is bound to both latch generations, never two commit states. Close/loss/
update scans the bounded central tagged table by full view/identity/generation;
there is no mutable per-latch list head or index CAS. One atomic tagged `RESERVED
-> COMMITTING -> COMMITTED -> PUBLISHED -> RELEASED` machine, with `REVOKED`/
`TOMBSTONED` alternatives, arbitrates commit against close/loss. Every CAS compares
`(lease_tag, state)`. The record predeclares an idempotent publication target/
recovery plan and release marker. Close/loss revokes, competes, or helps according
to state; owner incarnation/deadline lets another actor finish, cancel, drain, or
tombstone a dead holder or half-published target. A commit racing the control CAS
is already visible through its attempt/lease and is included in the drain.

Every admission-relevant fence change claims an update slot with tagged `FREE ->
INITIALIZING`, fills immutable atomic-word payload that helpers do not yet
interpret, release-publishes `PREPARED`, then accepts it with `PREPARED -> LINKING`.
`LINKING` is irrevocable and never reclaimed: the owner or helper performs the exact
`OPEN(g) -> UPDATING(g+1, update_tag)` CAS, or joins a newer monotonic close. Exact
`UPDATING` advances the same-tag record to `ACTIVE`; a helper never applies merely
prepared policy, and every action revalidates record tag/state plus latch tuple.
After admission-attempt quiescence, the update resolves all old-tuple leases and
publishes only its immutable intended fence while its exact token remains FIFO head.
Recovery stable-reads fence and cursor: an exact target only needs the missing
marker; target fence with predecessor cursor advances the cursor once; predecessor
state may publish only with the same valid head token; any mismatch closes the
view. `FENCE_PUBLISHED` is the only state eligible for exact tagged reopen.
Loss/close moves to newer `CLOSED`, defeating a
stale reopen. Owner death/deadline lets a helper finish the exact link/payload/
reopen sequence after quiescence/proven death, or close and quarantine the view on
deadline while the owner may execute; a resumed owner cannot link a reclaimed
record. Checked generation exhaustion reserves terminal `CLOSED`. This orders
policy with admission and later loss without a vulnerable store or final sequence
reread.

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

Telemetry uses two banks and one tagged publisher owner. A writer claims its
publish record through `FREE -> INITIALIZING`, fills old/intended payload and
recovery data, release-publishes `PREPARED`, accepts `LINKING`, and establishes
exclusive `WRITING(record_tag)` ownership. It reacquires and compares the expected
even latch/control after ownership. A stale value before bank touch atomically
aborts that record, releases exact ownership, and retries with a fresh record; it
never leaves `WRITING` stuck. The active writer checked-adds the full
64-bit snapshot sequence and one complete 64-bit control-latch pair, CASes the
expected even latch to odd before touching the inactive bank, writes that bank and
control payload through aligned `mf_atomic_*` words, and release-publishes the next
even latch. It then marks the record `PUBLISHED`, releases exact ownership, and
marks terminal. Recovery that observes the exact target latch/control/bank supplies
only missing markers and never republishes. `PUBLISHED+WRITING(tag)` only releases
ownership; `ACTIVE+WRITING(tag)` with the unchanged old even latch aborts/releases
only after proven death, while a live timeout quarantines. A reader acquires an
even latch, copies the atomic control payload,
reads the selected atomic bank, and accepts only if a second acquire returns the
same even latch. The final sequence encoding and final latch pair are reserved;
inability to retain one complete publication starts view close before odd/bank
write. Only proven owner death permits a helper to finish or restore old control.
A mere deadline closes and quarantines the view/banks until owner quiescence/death,
so a resumed writer cannot corrupt a reused bank. Neither counter can wrap, two
live-view writers cannot overlap, and no lock-free 128-bit CAS is required.

The full `registry_view_id` is a never-reused mapping incarnation minted from a
fresh daemon incarnation and checked per-daemon serial. New mappings never reuse
the old full ID even if storage/address is reused. Tokens, handles, memberships,
leases, and reader snapshots bind it; view-ID/serial exhaustion rejects creation,
so counter restart in a fresh view cannot form cross-view ABA.

`lifecycle_sequence` is a 64-bit serial scoped to one `registry_view_id` and never
wraps. One runtime-owned view-global gate allocates disjoint ranges into a FIFO
reservation queue. `RANGE_RESERVE` initializes the immutable range record, checked-
adds its entire length below reserved `UINT64_MAX`, and under exclusive publisher
ownership stable-commits allocation high-water plus queue tail together. The
following range marker may lag and is helped from exact target control; predecessor
state plus an accepted link completes once, while mismatch closes. Ranges never
truncate or leave an unexplained high-water gap. Pre-accept fit/capacity failure has
no side effect; accepted/external failure closes.

Tokens bind view/gate generation, transaction, immutable range, and deadline. No
mutable token position exists. For the proven head, next is `max(range.begin,
checked(cursor + 1))`; cursor remains the last real fence and immutable retired-
range ledger entries justify gaps. `RANGE_RETIRE` first commits the head range's
whole or partial unused-suffix disposition, then advances head; recovery only adds
the missing head marker. Abort-before-first may retire the whole range only while
authority is unchanged; partial abort first publishes a compensating fence. Failure
to mirror/compensate closes the mapping. Proven owner death may help; a live-owner
deadline quarantines.
Ordinary device loss atomically closes and advances its device latch, resolves
matching shared lease records, and may publish through a normal range only when
immediately at the FIFO head; head blocking or expiry of the explicit loss-
publication deadline starts mapping-terminal close instead of leaving `ONLINE`
visible.

Every fence/control step claims and fully initializes a tagged view-publish record
and links exclusive publisher ownership. Normal, range-reserve, range-retire, and
close kinds validate their distinct exact predecessors. Recovery compares affected
latches/ledger/hashes and supplies only missing cursor/head/range markers or
ownership release; it never repeats a committed fence/range. Only owner quiescence
or proven incarnation death permits raw-write helping.
A deadline with a possibly executing writer closes view/device admission and
quarantines the old mapping instead of stealing ownership.

`UINT64_MAX` is reserved for mapping-terminal close. Under the same gate, close
first seq-cst closes/advances view admission and every device validation tuple.
When publisher ownership is idle, quiesced, or proved dead, it consumes the final
two dedicated records/tags and reserved control-latch pairs to publish stable
`CLOSING` with the sentinel and then `TERMINAL`. When a writer may still execute,
the independent view-admission
tuple reaches `QUARANTINED`/terminal by deadline; control/fence may remain odd and
the entire mapping stays physically unreused until quiescence/death. Readers and
tokens reject either branch immediately, and the lifecycle authority may continue.
Short-width fixtures enumerate head blocking, completion, abort/skip,
compensation, owner death, live-owner deadline quarantine, ordinary loss deadline,
lease-holder death/half-publication helping, terminal close, torn-fence writes, and
reader device/view control-read/close/recheck races. They also cover shortened
range exhaustion, settled-writer use versus quarantined non-use of the final
control-latch pairs, tagged attempt/lease/update/view-publish/telemetry-publish slot
reuse, updater death at every update state without another close event, bounded
table scan during slot reuse, shortened telemetry-latch/sequence exhaustion, two
publisher stale-target races, a slow reader across two bank cycles, and close/new-
view stale replay, and
forbid partial ranges, later-range publication, abandoned intermediate state,
mixed fence/control fields, post-close admission/stale read or write, cross-view
comparison, and ABA.

Canonical source: [work-item-0.1.0.2](../../../plan/milestone-0.1.0.0-core-foundation/work/work-item-0.1.0.2-contracts-runtime.md).
