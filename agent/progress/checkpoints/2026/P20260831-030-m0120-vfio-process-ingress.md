---
id: P20260831-030
status: Recorded
captured: 2026-08-31
milestone: M0120
workstream: W0122
branch: main
git_revision: 699cff8
workspace: vfio-user process_once overload routes Closed EOF/error results through Disconnect ingress; QMP command transport and restart producers remain open
---

# M0120 W0122 Vfio-user Process Ingress

## Outcome

The coordinator-aware `VfioUserServer::process_once` overload now executes the
existing receive path and invokes the disconnect handoff only when it returns
`Closed`. Normal replies, no-reply operations, idle reads, and malformed
packets retain their original `ServerResult` and do not submit a lifecycle
event. The caller still supplies a complete `Disconnect` event tuple.

## Verification evidence

| Gate | Result |
|---|---|
| vfio-user server and lifecycle-dispatch focused selection | Passed: 2/2 |
| Full development CTest | Passed: 79/79 |
| Transport schema | Passed: 5 definitions / 15 records |
| Component graph | Passed: 19 components / 22 edges |
| Content format and whitespace | Passed: touched C++ clang-format and `git diff --check` |
| Commit identity | `Agent Harness (codex)` as Author and Committer for `699cff8` |

## Boundary

This binds the existing EOF/error detection to one lifecycle handoff but does
not implement QMP command socket transport, automatic request/tuple capture,
daemon restart or remaining reset producers, production memfd wiring,
provider-view freeze, failure injection, or lifecycle qualification.

## Cleanup

- Removed: no session-owned disposable artifact; ignored build output remains
  under the external Nix/CMake build path.
- Retained: process ingress overload, disconnect regression, transport
  documentation, and this compact checkpoint.

## roast

### light roasts

- Coordinator-aware process ingress API -> `transports/vfio-user/server/include/metaflux/transport/vfio_user_server.hpp` (focused server/dispatch regression)
- Closed-result handoff and ordinary-result preservation -> `transports/vfio-user/server/src/server.cpp` (full development CTest 79/79)

### medium roasts

- none.

### dark roasts

- none.

## session-only

- none.

## Handoff

Continue S0122/W0122 with an owner that can capture the current lifecycle tuple
and call the overload in the real vfio-user server loop. Keep QMP command
correlation and daemon restart as separate producers, each routed through the
same stateless ingress.
