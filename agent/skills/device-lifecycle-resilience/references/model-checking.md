# Lifecycle Model Checking

## Canonical inputs

work-item-0.1.2.1 consumes and verifies this frozen milestone-0.1.1.0-owned input:

- frozen base manifest: `contracts/protocol/transport/v1/schema/manifest.json`;

It creates and owns these versioned lifecycle inputs before any lifecycle adapter
is implemented:

- extension manifest:
  `contracts/protocol/transport/v1/schema/extensions/lifecycle/v1/manifest.json`;
- lifecycle model:
  `contracts/protocol/transport/v1/schema/extensions/lifecycle/v1/model.json`;
- exploration bounds and deterministic traversal order:
  `tests/lifecycle/model-bounds.json`;
- repository checker: `tools/check-lifecycle-model.py`.

The extension manifest references the lifecycle model exactly once and imports
the frozen base manifest by canonical path, schema version, and content hash.
The base manifest never references the extension. A manifest may not repeat an
identical tuple in its direct import list. The transitive DAG de-duplicates an
identical `(path, version, content hash)` tuple reached through multiple parents;
cycles or path/version/hash conflicts fail validation. Adding lifecycle cannot
change a frozen milestone-0.1.1.0 data-plane import or the base manifest's content hash. The
lifecycle model provides machine-readable
`states`, `events`, `guards`, `side_effects`, `commit_points`, `terminal_states`,
and `invariants`. Every transition names its source states, request identity,
generation-candidate action, epoch action, public state/error, deadline, and
legal successor states.

The lifecycle model is the normative state/guard/commit source. Generated tables
and adapter fixtures are projections and carry its content hash; prose is
nonnormative. Missing or schema-invalid input is a failed check.

## Reproducible command

Run from the repository root:

```sh
python3 tools/check-lifecycle-model.py \
  --base-manifest contracts/protocol/transport/v1/schema/manifest.json \
  --manifest contracts/protocol/transport/v1/schema/extensions/lifecycle/v1/manifest.json \
  --model contracts/protocol/transport/v1/schema/extensions/lifecycle/v1/model.json \
  --bounds tests/lifecycle/model-bounds.json \
  --output tmp/outputs/lifecycle/model-check.json
```

`model-bounds.json` pins device/request counts, generation and epoch bounds,
maximum sequence depth, enabled fault points, and deterministic traversal order.
The checker explores every enabled interleaving inside those bounds and exits
nonzero on an invalid schema, uncovered transition, invariant failure, or
incomplete search.
Random sampling is supplemental and never substitutes for this command.

The same bounds file contains a `fence_telemetry` branch. It exhaustively
interleaves loss-fence latch acquisition/commit/abort, two-bank telemetry
staging/commit, and reader capture/bank-read/final-recheck events. The branch
requires an even latch for an accepted bank, makes a loss fence invalidate
readiness before a stale writer can restore `ONLINE`, and limits reader retries
to the configured bound.

## Evidence artifact

`tmp/outputs/lifecycle/model-check.json` records:

- checker version and hashes of the base manifest, extension manifest, model, and
  bounds;
- the exact normalized command and deterministic traversal order;
- state, transition, complete-sequence, and maximum-depth counts;
- the separate `fence_telemetry` exploration counts and direct race checks for
  loss-fence precedence, stale `ONLINE` rejection, even-latch bank acceptance,
  and bounded reader retry;
- pass/fail status for single live owner, generation non-reuse, duplicate
  idempotence, atomic retirement/replacement, current-generation continuity,
  exact `epoch_after == epoch_before + 1` at every committed retirement,
  pre-accept generation/epoch exhaustion with no wrap or side effect,
  transport-loss epoch preservation, provider-view
  revision/membership freeze with loss-state updates, generation-safe matching
  across UUID-preserving re-add, monotonic loss-fence behavior against telemetry
  retry/fallback, FIFO view-range publication and permanent suffix retirement,
  range initialization/high-water-tail/marker reservation recovery, whole/partial
  suffix disposition before head advance, cursor-plus-range gap derivation,
  view-publish initialization/linkage and exact fence/cursor/marker/release
  reconciliation with no duplicate token position, compensation or terminal close
  after partial publication, settled-writer stable close using two dedicated
  records/tags/pairs versus live-writer admission-terminal quarantine without
  payload reuse, blocked-loss deadline
  to view close, single-record admission commit/close linearization and helping,
  owner-death/half-publication deadline recovery, tagged lease-slot ABA rejection,
  whole-fence odd/even snapshot safety without C/C++ data races, independent
  device-latch close versus stale fence writer, reader device/view-control close
  recheck, tagged `OPEN -> UPDATING -> OPEN/CLOSED` quota/policy ordering against
  readers and old-generation leases, loss-interrupted stale-reopen rejection,
  seq-cst admission-attempt quiescence with no late lease registration, bounded
  tagged-table scan with hazard/revalidation and no per-latch list-head ABA,
  `FREE -> INITIALIZING -> PREPARED/RESERVED` safety for every record family,
  update `PREPARED -> LINKING -> ACTIVE` linkage, exact fence/cursor/marker crash
  reconciliation, updater proven-death helping versus live-owner quarantine,
  exclusive telemetry-publisher linkage, exact even-latch/publish-marker recovery,
  slow-reader rejection across two bank cycles, live-writer deadline quarantine,
  and telemetry-latch/sequence exhaustion to view close before wrap,
  complete-range checked allocation below reserved `MAX`, pre-accept
  exhaustion with no side effect, accepted/external-event exhaustion to atomic
  close, branch-correct two-pair/control-record reserve with no live or reusable
  permanent `CLOSING`, close/
  new-view stale-actor rejection, terminal token invalidation with no later
  accepted lower write and no reuse of quarantined residual stores,
  lifecycle-authority independence from a closed process mirror, terminal
  completeness, and tombstone isolation invariants;
- the minimal counterexample and replay sequence for every failed invariant.

work-item-0.1.2.1 is incomplete when the checker, any input, or this artifact is absent.
Changing bounds requires review of the versioned bounds file and regenerating the
artifact; reducing bounds cannot silently preserve a prior pass.
