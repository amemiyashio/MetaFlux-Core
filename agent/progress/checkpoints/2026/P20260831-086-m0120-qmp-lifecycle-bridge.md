---
id: P20260831-086
status: Recorded
captured: 2026-08-31
milestone: M0120
workstream: W0122
branch: main
git_revision: a41a4a9ee1a0d4c8fa72f1b4759a3c5a96d9016c
workspace: QMP socket lifecycle completion bridge
---

# M0120 W0122 QMP Lifecycle Bridge Checkpoint

## Outcome

`QmpLifecycleAdapter::receive_and_submit` now consumes one bounded
`QmpSocket` lifecycle reply and returns separate transport and lifecycle
outcomes. Only a correlated reply reaches `Coordinator`; a wrong-kind event
leaves the adapter pending, while malformed, timeout, and closed-socket results
leave lifecycle authority state unchanged.

## Verification evidence

| Gate | Result |
|---|---|
| QMP socket bridge regression | Passed: `metaflux.transport.vfio-user-qmp-socket` 1/1 |
| Full Vulkan runtime CTest | Passed: 91/91 under `.#vulkan-runtime` |
| Build | Passed: QMP lifecycle/server targets and all Vulkan preset targets |
| Diff checks | Passed: `git diff --check` |
| Content identity | `a41a4a9`; Agent Harness (codex) is Author and Committer |

## Boundary

This checkpoint covers the host-independent socket-to-lifecycle composition
boundary. It does not wire every QEMU reset/disconnect/restart producer, add
provider enumeration freeze, claim live QEMU/kernel qualification, or close
the W0122/W0123 fault and lifecycle ABI-freeze gates.

## Cleanup

- Removed: none; no source snapshot, repository-local build output, or
  session-owned failed route was created.
- Retained: external build trees under their existing owner and foreign
  guidance packets in the active S01322/S01323 inboxes.

## roast

### light roasts

- QMP socket lifecycle completion bridge -> `transports/vfio-user/server/src/qmp_lifecycle.cpp` (`a41a4a9`, focused 1/1 and full 91/91 CTest)

### medium roasts

- W0122 socket-to-Coordinator composition boundary -> `agent/plan/M0120-vpci-lifecycle/work/W0122-existing-transports.md` (P086)

### dark roasts

- none.

## session-only

- none.

## Handoff

Resume W0122 from P086 and `a41a4a9`. The next bounded unit is producer-side
reset/restart wiring or failure injection; keep transport errors separate from
Coordinator authority and preserve the no-live-qualification claim.
