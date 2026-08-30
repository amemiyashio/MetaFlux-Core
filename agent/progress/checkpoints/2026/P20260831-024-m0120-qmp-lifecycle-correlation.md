---
id: P20260831-024
status: Recorded
captured: 2026-08-31
milestone: M0120
workstream: W0122
branch: main
git_revision: 5e1c3bc49d7d2e83ccab079ebeb99dacdd8679d4
workspace: vfio-user QMP command/event correlation fixture is implemented; live socket and producer wiring remain active
---

# M0120 W0122 QMP Lifecycle Correlation

## Outcome

The vfio-user server now contains a cold-control QMP correlation fixture. It
accepts one pending add or remove command, validates the typed lifecycle event,
and emits a normalized request only after the matching `device-added` or
`device-deleted` reply. A failed remove maps to the existing QMP transport-loss
operation so the Coordinator can publish `LOST`; a failed add emits no device
request. The fixture owns no generation or epoch allocation and does not
implement a QMP socket or production producer call site.

## Verification evidence

| Gate | Result |
|---|---|
| `metaflux.transport.vfio-user-qmp` | Passed |
| `metaflux.unit.runtime-lifecycle` and `metaflux.unit.runtime-lifecycle-normalizer` | Passed: 2/2 |
| QMP/lifecycle focused selection | Passed: 3/3 |
| Full development CTest | Passed: 78/78 |
| Transport schema | Passed: 5 definitions / 15 records |
| Component graph | Passed: 19 components / 22 edges |
| Skill routing | Passed: 89 cases |
| Agent records | Passed |
| Commit identity | `Agent Harness (codex)` as Author and Committer for `5e1c3bc` |

## Boundary

The adapter is a typed correlation fixture at the vfio-user server boundary.
Live QMP socket parsing, external producer call-site wiring, production memfd
worker integration, provider enumeration freeze, failure injection, repeated
cycle qualification, and lifecycle ABI freeze remain open under W0122/W0123.

## Cleanup

- Removed: none; no session-owned disposable artifact was created.
- Retained: canonical source, plan, test, and compact session records; external
  CMake/Ninja output remains under its owning build path.

## roast

### light roasts

- QMP command/event correlation ->
  `transports/vfio-user/server/include/metaflux/transport/qmp_lifecycle.hpp`
  (content revision `5e1c3bc`; focused QMP regression and full development CTest
  78/78)
- QMP failure mapping ->
  `runtime/core/include/metaflux/runtime/lifecycle_normalizer.hpp` (failed
  remove maps to `Source::Qmp` / `Operation::TransportLoss`; normalizer
  regression)

### medium roasts

- none.

### dark roasts

- none.

## session-only

- none.

## Handoff

Continue S0122/W0122 from `5e1c3bc`. Wire live QMP producers through the
normalizer and this correlation boundary, preserve the captured identity tuple,
and submit only the resulting request to `Coordinator`; do not move lifecycle
authority into the adapter.
