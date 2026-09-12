# Registry Reads, Admission and Policy Updates

Read for stable handle validation, side-effect admission or policy/loss races.
Use [record ownership](registry-records.md) when changing fields or reclamation
and [publication](registry-publication.md) when changing the fence writer itself.
The steady-state gate preserves bounded close deadlines and no global mutex.
Publication recovery names an idempotent cancel/complete/tombstone target and
places its release marker last.

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

## Affected scenarios

Select the scenarios for the mechanism being changed and the active work item's
required qualification; run them after the coherent implementation.

- Tear every multi-field fence write between fields. Require the reader's odd/even
  fence-latch copies plus outer device/view rechecks to return one whole commit or
  retry, never mixed identity, state, epoch, policy, or lifecycle sequence.
- Tear view-control writes too; stable control snapshots plus the independent
  view-admission latch must reject mixed gate state/generation/cursor fields.
- Race independent device-latch close against an older fence writer. Fence payload
  publication must never copy/reopen device admission, and readers must observe the
  closed latch even while the old payload commit completes.
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

Canonical source: [work-item-0.1.0.2](../../../plan/milestone-0.1.0.0-core-foundation/work/work-item-0.1.0.2-contracts-runtime.md).
