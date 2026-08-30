---
id: P20260831-023
status: Recorded
captured: 2026-08-31
milestone: M0120
workstream: W0122
branch: main
git_revision: 71956ff3e4c6d15a1b19b3f6c2e6b386b74f4d4d
workspace: typed external-event normalizer is implemented; producer call-site wiring and lifecycle qualification remain active
---

# M0120 W0122 Lifecycle Request Normalizer

## Outcome

The runtime core now defines a fixed `ExternalEventKind` vocabulary and a
stateless `RequestNormalizer`. Admin add/remove/reset, VFIO-user reset, QMP
add/remove, transport disconnect, and daemon restart map to the existing
`lifecycle::Request` source and operation fields. The normalizer copies the
captured request identity, generation, epoch, daemon incarnation, and deadline;
it rejects zero or malformed envelopes and unknown event kinds. It does not
allocate candidates, advance epochs, publish intermediate state, or infer
missing identity data. External producers still need to call it and submit the
result to `Coordinator`.

## Verification evidence

| Gate | Result |
|---|---|
| `metaflux.unit.runtime-lifecycle` and `metaflux.unit.runtime-lifecycle-normalizer` | Passed: 2/2 |
| Full development CTest | Passed: 77/77 |
| Transport schema | Passed: 5 definitions / 15 records |
| Component graph | Passed: 19 components / 22 edges |
| Skill routing | Passed: 89 cases |
| Commit identity | `Agent Harness (codex)` as Author and Committer for `71956ff` |

## Boundary

This is a normalized request contract and regression fixture, not integration
of a live QMP or daemon event producer. Producer call-site wiring, command/event
correlation, failure injection, provider enumeration freeze, repeated lifecycle
qualification, and ABI freeze remain open under W0122/W0123.

## Cleanup

- Removed: none; no session-owned disposable artifact was created.
- Retained: canonical source, plan, test, and compact session records; external
  CMake/Ninja output remains under its owning build path.

## roast

### light roasts

- Fixed external-event to lifecycle-request mapping ->
  `runtime/core/include/metaflux/runtime/lifecycle_normalizer.hpp` (content
  revision `71956ff`; focused normalizer test and full development CTest 77/77)
- Normalizer ownership and producer integration boundary ->
  `runtime/core/README.md` and W0121/W0122 plans (content revision `71956ff`;
  malformed/unknown rejection regression)

### medium roasts

- none.

### dark roasts

- none.

## session-only

- none.

## Handoff

Continue S0122/W0122 from `71956ff`. Route every external event producer through
`RequestNormalizer`, preserve the captured identity tuple, and submit only the
resulting request to `Coordinator`; do not move authority into an adapter.
