---
id: P20260831-034
status: Recorded
captured: 2026-08-31
milestone: M0120
workstream: W0122
branch: main
git_revision: 3259683
workspace: QMP and vfio-user disconnect producers capture lifecycle metadata from an authority snapshot; live socket and remaining reset/restart producer wiring remain open
---

# M0120 W0122 Snapshot-Bound Event Metadata

## Outcome

W0122 now provides one `capture_external_event` helper that copies the logical
device, daemon incarnation, identity record, generation, epoch, and optional
deadline from a `Coordinator::snapshot()` at event-observation time. QMP
commands expose a `from_snapshot` factory, and the coordinator-aware vfio-user
`process_once` overload uses the helper when the receive path observes `Closed`.
The existing explicit-event overload remains available for callers that already
captured a tuple.

Captured events remain immutable through correlation and completion. If the
authority advances before completion, normal validation returns `Stale` rather
than rebinding the event to the newer generation. No shared transport record,
descriptor, UAPI, BAR layout, or lifecycle ABI changed.

## Verification evidence

| Gate | Result |
|---|---|
| Snapshot capture unit | Passed: normalizer regression covers online and absent snapshots and all copied tuple fields |
| QMP producer regression | Passed: snapshot factory and stale completion case |
| vfio-user producer regression | Passed: snapshot-bound `Closed` path submits the captured disconnect tuple |
| Focused transport/lifecycle CTest | Passed: 3/3 |
| Full development CTest | Passed: 79/79 |
| Formatting and diff checks | Passed: clang-format dry-run and `git diff --check` |

## Boundary

This checkpoint covers metadata binding for the QMP and vfio-user fixtures. It
does not claim live QMP/socket command transport, automatic reset/restart
producer call sites outside these fixtures, production memfd worker wiring,
provider enumeration freeze, repeated fault qualification, or lifecycle ABI
freeze. Those remain active W0122/W0121/W0113 work.

## Cleanup

- Removed: temporary build/test output under the external build owner.
- Retained: snapshot-bound helper, producer regressions, and this compact checkpoint.

## roast

### light roasts

- Snapshot-bound event helper -> `runtime/core/include/metaflux/runtime/lifecycle_normalizer.hpp` (copies the authority tuple once at producer observation)
- QMP snapshot factory -> `transports/vfio-user/server/include/metaflux/transport/qmp_lifecycle.hpp` (correlated command retains the captured tuple)
- vfio-user closed-path binding -> `transports/vfio-user/server/include/metaflux/transport/vfio_user_server.hpp` (automatic disconnect ingress uses the current snapshot)

### medium roasts

- none.

### dark roasts

- none.

## session-only

- none.

## Handoff

Resume S0122/W0122 from this checkpoint. Extend the same capture rule to live
QMP/socket and remaining reset/restart producers, then connect production memfd
ownership and qualification without synthesizing or rebinding lifecycle tuples.
