---
id: P20260831-072
status: Recorded
captured: 2026-08-31
milestone: M0110
workstream: W0112
branch: main
git_revision: a536dd0
workspace: host-independent cdev backend memory reference lifetime
---

# M0110 W0112 cdev Memory Reference Checkpoint

## Outcome

Region COPY resolution now carries a retain/release pair for each returned
backend memory handle. The worker validates the pair, retains destination and
source before dispatch, rolls back partial admission failure, and keeps both
references through an asynchronous event and completion-ring backpressure. It
releases them only after synchronous completion or successful asynchronous
completion publication.

## Verification evidence

| Gate | Result |
|---|---|
| Focused cdev/client/component tests | Passed: 3/3 |
| Full development CTest | Passed: 84/84 |
| Diff checks | Passed: `git diff --check` |
| Content identity | `a536dd0`, Agent Harness (codex) as Author and Committer |

## Boundary

This proves host-independent reference lifetime for resolver-returned backend
memory handles. It does not implement production import from kernel registered
memory/DMA mappings, daemon generation replacement/drain, lifecycle-loss
cancellation, kernel sanitizer/fault qualification, or physical CUDA/NVIDIA
qualification.

## Cleanup

- Removed: no session-owned disposable artifacts.
- Retained: cdev memory-reference contract, regressions, and compact
  session/checkpoint records.

## roast

### light roasts

- Resolver-owned source and destination memory references survive COPY completion -> `transports/cdev/worker/src/worker.cpp` (`a536dd0`; focused/full CTest)

### medium roasts

- W0112 backend memory reference lifetime boundary -> `agent/plan/M0110-kernel-guest-transport/work/W0112-local-cdev.md` (P072; production import and generation drain remain open)

### dark roasts

- none.

## session-only

- Async region COPY filler and memory-reference fixture - reason: bounds retain/release lifetime through event completion and completion-ring backpressure without physical backend evidence

## Handoff

Resume W0112 from `a536dd0` and P072. Read the cdev worker contract and kernel
registered-memory ownership path, then connect registered-memory handles to a
production backend import/reference implementation without changing the fixed
UAPI.
