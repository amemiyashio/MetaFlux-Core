---
id: P20260831-073
status: Recorded
captured: 2026-08-31
milestone: M0110
workstream: W0112
branch: main
git_revision: 89c6bb4
workspace: capability-gated cdev pending cancellation on transport loss
---

# M0110 W0112 cdev Lifecycle Cancellation Checkpoint

## Outcome

When a pending asynchronous cdev operation receives transport loss, the worker
uses the generation-bound backend binding's `cancel_queue` only when the backend
advertises `MF_BACKEND_CAP_CANCELLATION`. A successful cancellation converts the
operation to `MF_SHARED_DEVICE_LOST` through the normal completion path. Lease
and resolver-owned memory references remain held while the completion ring is
full and are released only after completion publication succeeds. A backend
without cancellation capability keeps its existing event/lease behavior.

## Verification evidence

| Gate | Result |
|---|---|
| Focused cdev/lifecycle/component tests | Passed: 6/6 |
| Full development CTest | Passed: 84/84 |
| Diff checks | Passed: `git diff --check` |
| Content identity | `89c6bb4`, Agent Harness (codex) as Author and Committer |

## Boundary

This proves capability-gated lifecycle-loss cancellation for one pending
host-independent backend operation. It does not implement production
registered-memory import, non-cancellable backend generation drain, daemon
replacement transaction, kernel sanitizer/fault qualification, or physical
CUDA/NVIDIA qualification.

## Cleanup

- Removed: no session-owned disposable artifacts.
- Retained: capability-gated cancellation implementation, lifecycle regression,
  and compact session/checkpoint records.

## roast

### light roasts

- Capability-gated pending cancellation maps transport loss to device-lost completion -> `transports/cdev/worker/src/worker.cpp` (`89c6bb4`; focused/full CTest)

### medium roasts

- W0112 lifecycle-loss cancellation boundary -> `agent/plan/M0110-kernel-guest-transport/work/W0112-local-cdev.md` (P073; generation drain and production import remain open)

### dark roasts

- none.

## session-only

- Cancellation fixture and transport-loss request - reason: bounds lifecycle cancellation through the existing coordinator without physical backend qualification

## Handoff

Resume W0112 from `89c6bb4` and P073. Read the cdev lifecycle mirror and backend
binding contract, then implement production generation drain and registered-
memory import/reference integration without changing the fixed UAPI.
