---
id: work-item-0.1.2.1
delivery: 0.1.2.1
milestone: milestone-0.1.2.0
status: Active
area: lifecycle.contract
depends_on: [milestone-0.1.1.0]
updated: 2026-08-30
---

# Lifecycle Model and ABI 0.x

## Outcome

Specify and model one idempotent lifecycle state machine before transport or PCI
implementation. `metafluxd` remains the only authority that reserves generation
candidates, publishes committed generations, and advances epoch; transport
owners mirror committed state and retain local tombstones. The complete ownership
model is maintained in
[the control/data-plane architecture](../../../../docs/architecture/control-and-data-plane.md).

Each daemon process receives a non-reusable 128-bit `daemon_incarnation_id`,
separate from device UUID/generation/epoch. Every request carries request ID,
source, operation, expected UUID, expected generation, daemon incarnation, and
deadline. Replayed IDs are idempotent; duplicate, stale, or conflicting requests
consume no new generation candidate and never advance epoch.

| Event | Valid source | Admin-visible intermediate states | Terminal state | Identity action |
| --- | --- | --- | --- | --- |
| Add | `ABSENT` | `PRESENT` | `ONLINE` or `LOST`/`ABSENT` | Reserve one candidate; add commit installs it `ONLINE` without changing epoch |
| Remove | `ONLINE` or `LOST` | `QUIESCING -> DRAINING` | `ABSENT` | Retire generation and advance epoch once; replacement gets a new generation |
| Reset | `ONLINE` | `QUIESCING -> DRAINING -> RESETTING` | `ONLINE` or `LOST` | Reserve one candidate; atomic replacement retires old, advances epoch once, and installs candidate `ONLINE` |
| Transport loss | Any live state | none required | `LOST` | Preserve current identity as a tombstone |
| Recover | `LOST` | `RESETTING` | `ONLINE` or `LOST` | Reserve one candidate; atomic replacement retires old `LOST`, advances epoch once, and installs candidate `ONLINE` |

Intermediate states report transaction progress against the old or absent
identity. They never make a staged candidate current or provider-enumerable;
`PRESENT` is an admin transaction state, not an added process-view member.

Add/reset/recover acceptance includes a successful checked reservation of the
generation high-water mark. Remove/reset/recover acceptance first proves one
fixed-width epoch increment fits and serializes the retirement right. Generation
or epoch exhaustion returns the stable pre-accept overflow/resource error with no
candidate, intermediate state, identity, or epoch change. Only accepted requests
enter the table above and its public terminal-state deadline.

Add persists the generation high-water mark and stages immutable identity,
transport backing, and the exclusive worker lease before committing registry
visibility. Failure before commit destroys staged resources but consumes the
candidate; failure after commit makes that generation `LOST`.

Reset acceptance durably reserves exactly one never-reused generation candidate,
then publishes `QUIESCING`, rejects new work, and drains. Any failure after
acceptance consumes that candidate. Every replacement owner is staged before one
atomic identity transaction retires the old generation, advances epoch exactly
once, and installs the candidate as the current `ONLINE` generation. A failure
before that transaction destroys staging, leaves epoch unchanged, and may restore
the old generation to `ONLINE`; a later fault can only mark the committed
candidate `LOST`, never restore the retired generation. A duplicate consumes no
candidate and causes no epoch change. Removal rejects new opens/submissions before
removing transport; old fd, VMA, DMA, queue, event, memory, executable, and handle
objects remain generation-bound tombstones returning `DEVICE_LOST`.

Transport loss by itself preserves the current generation/epoch as a `LOST`
tombstone. Accepted recovery reserves one candidate, stages the replacement, and
uses the same atomic replacement transaction to retire the old lost generation,
advance epoch once, and install the candidate `ONLINE`. A pre-transaction failure
consumes the candidate but leaves the old generation current, epoch unchanged,
and state `LOST`; a later fault marks the committed candidate `LOST`. There is no
state in which a retired generation remains current or a replacement-less `LOST`
identity can be retired again.

A public deadline means state reaches complete `ONLINE`, `LOST`, or `ABSENT`; it
does not promise physical cancellation. Non-cancellable work remains isolated
behind old-generation tombstones. Lifecycle/admin UAPI is frozen as
`mf_admin_lifecycle_v1` by work-item-0.1.2.3 from the hashed extension admin
schema; kernel sanitizer soaks remain host gates and do not reopen the wire.

## Canonical Model Gate

work-item-0.1.2.1 consumes and verifies this frozen milestone-0.1.1.0-owned input:

- `contracts/protocol/transport/v1/schema/manifest.json`;

It creates and owns these versioned lifecycle inputs and the checker before any
adapter implementation:

- `contracts/protocol/transport/v1/schema/extensions/lifecycle/v1/manifest.json`;
- `contracts/protocol/transport/v1/schema/extensions/lifecycle/v1/model.json`;
- `tests/lifecycle/model-bounds.json`;
- `tools/check-lifecycle-model.py`.

The extension manifest imports the frozen milestone-0.1.1.0 root manifest by path, version,
and content hash. The dependency is one-way: the milestone-0.1.1.0 root never imports the
extension, so adding lifecycle leaves every frozen milestone-0.1.1.0 definition and root
content hash unchanged.

Run from the repository root:

```sh
python3 tools/check-lifecycle-model.py \
  --base-manifest contracts/protocol/transport/v1/schema/manifest.json \
  --manifest contracts/protocol/transport/v1/schema/extensions/lifecycle/v1/manifest.json \
  --model contracts/protocol/transport/v1/schema/extensions/lifecycle/v1/model.json \
  --bounds tests/lifecycle/model-bounds.json \
  --output tmp/outputs/lifecycle/model-check.json
```

The bounds file pins object/request counts, sequence depth, fault points, and a
deterministic exploration order that still enumerates every enabled interleaving
inside the bounds. The JSON artifact records input hashes, checker
version, normalized command, explored state/transition/sequence counts, invariant
results, and minimal replayable counterexamples. Missing input, incomplete search,
failed invariant, or absent artifact fails the gate.

The lifecycle model references and validates the runtime-owned process-view rules
for one `registry_view_id`, CUDA's initialized membership revision, NVML's
zero-to-one initialization epoch, loss of old entries, and first visibility of an
addition. It defines only lifecycle loss/addition event payloads and their ordering
relative to the identity transaction. Any membership, ordering, freeze-revision,
or first-visibility rule change composes `$runtime-contracts-registry`.
The publication branch also models an even/odd lifecycle fence latch, a two-bank
telemetry writer, and a bounded reader retry/final-fence recheck. A loss fence
invalidates telemetry readiness before a stale writer can publish an acceptable
`ONLINE` result.

## Work

Implemented stage:

- [x] Define the one-way lifecycle extension manifest importing the frozen milestone-0.1.1.0
  root by content hash and a canonical generation/epoch transition model.
- [x] Add deterministic exploration bounds and the repository model checker with
  explicit candidate exhaustion, retirement, tombstone, replay, provider-freeze,
  and loss/recovery checks.
- [x] Add positive and tampered-manifest checker fixtures and register the exact
  bounded command in CTest.

- [x] Specify guards, owner, commit points, deadlines, terminal errors, and
  high-water persistence for every transition.
- [x] Define request ID, incarnation, generation, epoch, idempotence, lease
  staging/revocation, worker death, and stale-completion rules.
- [x] Exercise shortened generation and epoch widths: exhaustion rejects before
  acceptance/intermediate publication, preserves state/identity/epoch, consumes
  no candidate, and never wraps.
- [x] Define a typed normalizer for admin, vfio-user, QMP, disconnect, and
  restart sources. Runtime callers still need to wire each external event
  producer through this mapping before submitting the resulting request.
- [x] Define a vfio-user QMP command/event correlation fixture. It emits a
  normalized add/remove request only after the matching event and maps a failed
  remove to QMP transport loss; live QMP socket integration remains open.
- [x] Provide one runtime ingress for external events. It normalizes and submits
  only accepted requests to the Coordinator, preserving the captured identity
  tuple and leaving source-specific producer capture to the adapters.
- [x] Implement the canonical model, versioned bounds, deterministic checker, and
  machine-readable evidence contract; run the exact root command above.
- [x] Model provider removal/re-add with one `registry_view_id`, initialized CUDA,
  current/later NVML init epochs, and a new process.
- [x] Model loss/removal fence publication racing telemetry-bank publication and
  bounded reader retry/fallback; forbid stale `ONLINE` after the observed fence.
- [x] Bind `RegistryView` telemetry producers to stable online fence/admission
  snapshots before, during, and after bank staging; recovery validates a
  marker-complete target bank before promoting it.
- [x] Compose the runtime view gate and enumerate reserve(A), reserve(B), A partial
  publish, B blocked publish, A complete/abort/compensate/suffix-skip, owner death,
  B authority loss plus admission/read and loss-deadline close, one shared admission
  attempt through seq-cst ENTERING/open-read/quiescence and its lease through
  `FREE`/`INITIALIZING`/`RESERVED`/committing/commit/publication, holder death and
  target half-publication/helping, tagged slot reuse with a stale actor, identity commit,
  mapping-terminal close, close/new-view stale replay, every split point of a fence
  or view-control write, device-latch close versus stale fence writer, full range
  fit/cross-`MAX` before and after authority acceptance, `RANGE_RESERVE` range-slot/
  high-water-tail/marker/acceptance, `RANGE_RETIRE` whole/partial suffix/head,
  abort-before-first and `max(begin, cursor + 1)` gap derivation; view-publish `FREE`/
  `INITIALIZING`/`PREPARED`/`LINKING`/`ACTIVE`, fence stable-even, actual
  publication cursor plus immutable range/retirement ledger,
  marker/ownership release, proven-death reconcile, and live-writer quarantine;
  zero/one/two ordinary publish records/tags with two dedicated close records/
  tags/pairs and normal/close-kind validation; tagged quota/policy record `FREE`/
  `INITIALIZING`/`PREPARED`/`LINKING`/`ACTIVE`/fence/cursor/marker/reopen versus
  readers/old leases and loss/close, including proven-death reconcile and live-
  owner quarantine; bounded central-table scan during tagged attempt/lease/update/
  publish-slot reuse; telemetry record/link/exclusive owner, two stale prepared
  targets, pre-odd death, each bank/control/even/marker/release split, live-owner
  quarantine, shortened latch/sequence, a slow reader across two bank cycles, and
  exhaustion; one/two remaining view-control latch pairs, and reader device/view-
  control close-recheck. Only the lowest unclosed full range publishes, and no
  separate mutable token position exists;
  blocked loss closes admission, both `CLOSING`/`TERMINAL` commits retain reserved
  pairs/records in the settled-writer branch, while the live-writer branch reaches
  terminal admission/quarantine without consuming them or reusing the mapping;
  policy reopens only after old leases resolve, updater death cannot strand
  `UPDATING` in a live view, stale reopen CAS cannot defeat loss, admission latches
  expose no mutable lease head/index, telemetry never wraps or returns a mixed bank,
  stale telemetry targets abort/retry without bank touch or stuck ownership, stable
  readers never tear or reopen independent latches, invalidated/old-view actors
  never publish, and a closed mirror does not roll back authority. Added
  `view_gate_model` to model.json with all 7 state machines (ViewState, DeviceState,
  AttemptState, LeaseState, UpdateState, PublishState, RangeState), transitions,
  and16 invariants. Added `view_gate` bounds to model-bounds.json. Added
  `ViewGateSnapshot`, `assert_view_gate_snapshot`, `view_gate_direct_scenarios`
  (21 checks), and `explore_view_gate` (2520 states, 5098 transitions) to
  `check-lifecycle-model.py`.
- [x] Generate positive, invalid, repeated, racing, and injected-failure model
  fixtures from one schema. Added `generate_fixtures()` to `check-lifecycle-model.py`
  with `--generate-fixtures` CLI option. Generates 32 fixtures across 5 categories:
  positive (5 valid transitions), invalid (10 guard-violating sequences), repeated
  (5 idempotent replay scenarios), racing (4 concurrent event pairs), and
  injected_failure (8 pre_commit/post_commit fault points). Output is
  language-neutral JSON consumable by any test harness.

## Exit Gate

`tmp/outputs/lifecycle/model-check.json` demonstrates
within the pinned bounds that no explored legal event sequence publishes two
live owners, reuses an accepted generation candidate, advances epoch before
retirement, commits a retirement without
`epoch_after == epoch_before + 1`, accepts or publishes an intermediate state at
generation/epoch exhaustion, changes identity for a
duplicate, separates old retirement from replacement installation, changes a provider's
captured membership/order before its permitted reinitialization, adds a CUDA
ordinal, prevents the required loss-state update, confuses two generations with
  the same UUID/BDF, permits telemetry fallback to restore `ONLINE` after an
  observed loss fence, publishes a later view range before an earlier one closes,
  splits range high-water from queue tail, leaves a reserved range marker or an
  unexplained gap, advances head before immutable suffix retirement, maintains a
  mutable token position separate from cursor/range ledger, reuses an aborted
  suffix, leaves a live intermediate mirror after owner failure,
  leaves admission open behind a blocked loss, half-commits view/device admission,
  misses an attempt that observed old `OPEN`, interprets initializing payload,
  accepts a stale CAS after tagged record reuse, strands a live/reusable `CLOSING`
  view after owner death, steals a possibly executing writer instead of
  quarantining,
  accepts mixed fields from two fence commits, lets a stale writer reopen the
  device latch, truncates a range or consumes reserved `MAX`, fails to close after
  accepted-event exhaustion, lets normal publication consume the two-record/tag/
  pair close reserve, fails settled close publication, or fails live-writer
  terminal-admission quarantine, admits an old-policy lease under a new quota,
  lets a stale updater
  defeat loss or lower validation generation, strands `UPDATING` after updater
  death, replays an exact fence/cursor commit, accepts a per-latch index/list-head
  ABA or a stale helper after tagged attempt/lease/update/view-publish/telemetry-
  publish slot reuse, overlaps telemetry publishers, strands stale-target or
  pre-odd/post-marker ownership, wraps telemetry latch/sequence or accepts mixed
  bank fields, accepts an old actor in a fresh view,
  commits admission after close, returns stale `ONLINE` across device/
  view close recheck, writes through an invalidated token after mapping-terminal
  close, rolls back the authority because one process mirror closed, or leaves a
  half-online state. The
exact command above succeeds
from a clean repository checkout.
