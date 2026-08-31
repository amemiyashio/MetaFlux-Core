---
id: P20260831-071
status: Recorded
captured: 2026-08-31
milestone: M0110
workstream: W0112
branch: main
git_revision: 3140df7
workspace: host-independent generation-bound cdev backend binding snapshot
---

# M0110 W0112 cdev Backend Reference Checkpoint

## Outcome

Each pending asynchronous cdev operation now retains the exact backend binding
and lease callbacks that admitted it. Completion polling and lease release use
that snapshot, so replacing the worker binding during an in-flight event cannot
query or release the old operation through the replacement binding. The pending
request remains until completion-ring publication succeeds, including the
backpressure retry path.

## Verification evidence

| Gate | Result |
|---|---|
| Focused cdev/client/component tests | Passed: 3/3 |
| Full development CTest | Passed: 84/84 |
| Diff checks | Passed: `git diff --check` |
| Content identity | `3140df7`, Agent Harness (codex) as Author and Committer |
| Documentation identity | `d7de5a0`, Agent Harness (codex) as Author and Committer |

## Boundary

This proves replacement-safe binding ownership for one host-independent pending
operation. It does not implement production backend memory import, in-flight
device references, daemon generation replacement/drain, lifecycle-loss
cancellation, kernel sanitizer/fault qualification, or physical CUDA/NVIDIA
qualification.

## Cleanup

- Removed: no session-owned disposable artifacts.
- Retained: cdev worker binding snapshot, replacement regression, and compact
  session/checkpoint records.

## roast

### light roasts

- Pending asynchronous operations retain the admitting backend binding -> `transports/cdev/worker/src/worker.cpp` (`3140df7`; focused/full CTest)

### medium roasts

- W0112 in-flight backend ownership boundary -> `agent/plan/M0110-kernel-guest-transport/work/W0112-local-cdev.md` (P071; backend import and generation drain remain open)

### dark roasts

- none.

## session-only

- Replacement binding fixture - reason: bounds old-binding query/release ownership without representing physical backend qualification

## Handoff

Resume W0112 from `3140df7` and P071. Read the cdev worker contract and current
backend binding implementation, then connect registered-memory handles to a
production backend import/reference path without changing the fixed UAPI.
