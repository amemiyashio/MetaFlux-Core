---
id: P20260831-076
status: Recorded
captured: 2026-08-31
milestone: M0110
workstream: W0112
branch: main
git_revision: b03c4f3
workspace: cdev worker direct backend-memory reference lifetime
---

# M0110 W0112 cdev Direct Memory Reference Checkpoint

## Outcome

The cdev worker binding can now carry a complete optional
`CdevBackendMemoryReference` for the direct payload-memory handle. The worker
validates empty-or-complete callback sets, retains the handle before a bound
COPY, and releases it after synchronous completion or after asynchronous
completion, cancellation, and completion-ring backpressure. Pending operations
snapshot the reference together with the backend binding, so replacement cannot
release through a different owner. The backend ABI and Linux UAPI records are
unchanged.

## Verification evidence

| Gate | Result |
|---|---|
| Focused cdev/component tests | Passed: 2/2 |
| Full development CTest | Passed: 84/84 |
| Diff checks | Passed: `git diff --check` |
| Content identity | `b03c4f3`, Agent Harness (codex) as Author and Committer |

## Boundary

This proves host-independent direct backend-memory reference retention through
worker completion and backpressure. It does not import kernel registered-memory
handles into a production backend, implement daemon generation replacement,
qualify kernel sanitizers/fault injection, or provide physical CUDA/NVIDIA
evidence.

## Cleanup

- Removed: no session-owned disposable artifacts; ignored build outputs remain under external build owners.
- Retained: worker reference contract, regressions, transport documentation, and compact session records.

## roast

### light roasts

- Direct backend-memory retain/release lifetime through cdev worker completion -> `transports/cdev/worker/src/worker.cpp` (`b03c4f3`; focused/full CTest)

### medium roasts

- W0112 in-flight direct-memory reference boundary -> `agent/plan/M0110-kernel-guest-transport/work/W0112-local-cdev.md` (P076; production backend import remains open)

### dark roasts

- none.

## session-only

- Optional reference callbacks preserve existing fixture bindings - reason: production import ownership is supplied by the eventual daemon/object-table adapter.

## Handoff

Resume W0112 from `b03c4f3` and P076. Read the cdev resolver and backend ABI ownership contracts, then connect registered-memory/object-table import and generation replacement without changing the frozen ABI records.
