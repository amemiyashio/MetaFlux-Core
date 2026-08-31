---
id: P20260831-069
status: Recorded
captured: 2026-08-31
milestone: M0110
workstream: W0112
branch: main
git_revision: 7f3b8f3
workspace: host-independent cdev asynchronous backend completion and lease retention
---

# M0110 W0112 cdev Async Lease Checkpoint

## Outcome

The cdev worker now supports a host-independent asynchronous backend boundary.
When a backend returns success with a nonzero completion event, the worker
retains the consumed request and operation lease, polls `query_event`, and
publishes one completion only after the event completes. Query failures map to
the shared status ABI. A full completion ring leaves both pending state and the
lease intact for a later retry, and the completion timeline advances only when
publication succeeds.

## Verification evidence

| Gate | Result |
|---|---|
| Dev build | Passed: `nix develop . --command cmake --build --preset dev --parallel 2` |
| Focused cdev/client/component tests | Passed: 3/3 |
| Full development CTest | Passed: 84/84 |
| Diff checks | Passed: `git diff --check` |
| Content identity | `7f3b8f3`, Agent Harness (codex) as Author and Committer |

## Boundary

This proves one in-flight asynchronous operation over the existing backend
event ABI, including pending, completion, query-error, and ring-backpressure
behavior. It does not prove production registered-memory DMA mapping, backend
replacement generations, lifecycle-loss cancellation, daemon replacement,
kernel fault injection, or physical CUDA/NVIDIA qualification.

## Cleanup

- Removed: no session-owned disposable artifacts.
- Retained: cdev worker implementation, focused regression, and compact session
  records.

## roast

### light roasts

- Async cdev event polling and lease retention ->
  `transports/cdev/worker/src/worker.cpp` (`7f3b8f3`; focused/full CTest).

### medium roasts

- W0112 host-independent asynchronous completion boundary ->
  `agent/plan/M0110-kernel-guest-transport/work/W0112-local-cdev.md`
  (P069; production DMA and lifecycle qualification remain open).

### dark roasts

- none.

## session-only

- Ring filler and local backend event fixture - reason: bounds retry and lease
  behavior without representing physical DMA, replacement, or hardware evidence.

## Handoff

Resume W0112 from `7f3b8f3` and this checkpoint. Read the cdev worker contract,
then implement production registered-memory/DMA references and define
generation-aware replacement and lifecycle-loss cancellation before seeking
the cdev exit gate.
