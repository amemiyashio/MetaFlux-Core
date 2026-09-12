# Lifecycle Ranges, Publication and Terminal Close

Read for sequence allocation, FIFO head publication, interrupted writers or
terminal close. [Record ownership](registry-records.md) defines fields, reserved
capacity and reclamation. Stateful admission uses the separate
[admission protocol](registry-admission.md).

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

## Affected scenarios

Select the scenarios for the mechanism being changed and the active work item's
required qualification; run them after the coherent implementation.

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

Canonical source: [work-item-0.1.0.2](../../../plan/milestone-0.1.0.0-core-foundation/work/work-item-0.1.0.2-contracts-runtime.md).
