---
id: P20260831-029
status: Recorded
captured: 2026-08-31
milestone: M0120
workstream: W0122
branch: main
git_revision: ee0ecab
workspace: vfio-user EOF/error handoff submits a captured Disconnect event through lifecycle ingress; metadata capture and live QMP command transport remain open
---

# M0120 W0122 Vfio-user Disconnect Ingress

## Outcome

`VfioUserServer::mark_lost_and_submit` now models the disconnect producer
handoff. It marks the local server `Lost` first, requires a `Disconnect` event,
and routes the caller-supplied tuple through `submit_external_event`. The
server does not infer logical-device identity, daemon incarnation, generation,
epoch, or request ID; `ServerResult` reports local transport closure while
`ResultDetails` reports the Coordinator outcome.

## Verification evidence

| Gate | Result |
|---|---|
| vfio-user server and lifecycle-dispatch focused selection | Passed: 2/2 |
| Full development CTest | Passed: 79/79 |
| Transport schema | Passed: 5 definitions / 15 records |
| Component graph | Passed: 19 components / 22 edges |
| Content format and whitespace | Passed: touched C++ clang-format and `git diff --check` |
| Commit identity | `Agent Harness (codex)` as Author and Committer for `ee0ecab` |

## Boundary

The helper is a producer handoff, not automatic socket integration. The
`process_once` EOF/error path still detects the local closure; its owner must
capture the current lifecycle tuple and call the helper. QMP command transport,
remaining reset/restart producer call sites, production memfd wiring,
provider-view freeze, failure injection, and lifecycle qualification remain
open.

## Cleanup

- Removed: no session-owned disposable artifact; ignored build output remains
  under the external Nix/CMake build path.
- Retained: disconnect handoff, focused regression, transport documentation, and
  this compact checkpoint.

## roast

### light roasts

- vfio-user disconnect handoff API -> `transports/vfio-user/server/include/metaflux/transport/vfio_user_server.hpp` (focused server/dispatch regression)
- Local-loss transition and ingress submission -> `transports/vfio-user/server/src/server.cpp` (full development CTest 79/79)

### medium roasts

- none.

### dark roasts

- none.

## session-only

- none.

## Handoff

Continue S0122/W0122 by binding the real EOF owner to the helper's event tuple,
then cover daemon restart and remaining reset producers. Keep local transport
loss and Coordinator publication observable as separate outcomes.
