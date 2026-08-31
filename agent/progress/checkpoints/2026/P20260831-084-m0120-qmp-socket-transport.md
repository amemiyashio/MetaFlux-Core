---
id: P20260831-084
status: Recorded
captured: 2026-08-31
milestone: M0120
workstream: W0122
branch: main
git_revision: 0c57d1f4e0e81293a2c23c45d895a39b4d7755c4
workspace: bounded live QMP Unix-stream command/event transport
---

# M0120 W0122 Live QMP Socket Checkpoint

## Outcome

W0122 now has a host-independent `QmpSocket` adapter for live QMP control
traffic. It connects to a Unix `SOCK_STREAM`, sends bounded command objects with
validated non-zero numeric IDs and JSON-object arguments, frames one top-level
JSON object up to 64 KiB, and classifies greetings, replies, errors,
`DEVICE_ADDED`, `DEVICE_DELETED`, other events, closure, and malformed input.
The transport layer composes with the existing `QmpLifecycleAdapter` without
owning lifecycle identity, generation/epoch allocation, retries, or state
publication.

## Verification evidence

| Gate | Result |
|---|---|
| QMP socket focused test | Passed: `metaflux.transport.vfio-user-qmp-socket` 1/1 |
| Vulkan build | Passed: server library and QMP socket test targets built |
| Full Vulkan runtime CTest | Passed: 91/91 under `.#vulkan-runtime` |
| Diff checks | Passed: `git diff --check` |
| Content identity | `0c57d1f`; Agent Harness (codex) is Author and Committer |

## Boundary

This checkpoint proves the bounded wire adapter and its composition boundary;
it does not claim QEMU producer call-site wiring, reset/restart metadata
binding, provider enumeration freeze, staged device commit/rollback, fault
injection, or live QEMU/vPCI qualification. Those remain open in W0122/W0123.

## Cleanup

- Removed: none.
- Retained: no test logs or generated snapshots; the external Nix build tree
  remains under its existing build owner.

## roast

### light roasts

- Bounded QMP Unix-stream framing and classification ->
  `transports/vfio-user/server/src/qmp_socket.cpp` (`0c57d1f`, focused 1/1 and
  full 91/91 CTest)

### medium roasts

- W0122 live QMP transport boundary ->
  `agent/plan/M0120-vpci-lifecycle/work/W0122-existing-transports.md` (P084)

### dark roasts

- none.

## session-only

- none.

## Handoff

Resume W0122 from `0c57d1f` and P084. Read the W0122 plan, the vfio-user
README, `qmp_lifecycle.hpp`, and the runtime lifecycle ingress. The next
bounded unit is producer-side QMP add/remove metadata and staged commit/abort;
keep transport framing separate from coordinator authority and live hardware
qualification.
