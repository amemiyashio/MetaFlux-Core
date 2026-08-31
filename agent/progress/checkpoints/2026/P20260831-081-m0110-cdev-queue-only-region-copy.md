---
id: P20260831-081
status: Recorded
captured: 2026-08-31
milestone: M0110
workstream: W0112
branch: main
git_revision: 8095713a6bb3c4af0de726ba9d975111711b0be1
workspace: queue-only cdev worker region COPY
---

# M0110 W0112 Queue-Only Region COPY Checkpoint

## Outcome

The cdev worker now accepts a queue-only view when a bound backend provides the
object-table region COPY resolver. Direct payload COPY and LAUNCH still require
the caller-owned payload arena and return `MF_SHARED_NOT_SUPPORTED` when it is
absent. The queue-only regression executes two CPU backend imports, balances
retain/release callbacks, and verifies the copied bytes.

## Verification evidence

| Gate | Result |
|---|---|
| Focused cdev worker and daemon integration | Passed: 2/2 |
| Full development CTest | Passed: 84/84 |
| Diff checks | Passed: `git diff --check` |
| Record checks before content commit | Passed |

## Boundary

This is a host-independent CUDA/cdev worker contract correction. Live
`/dev/metafluxctl` daemon attachment, kernel DMA-backed backend import,
generation replacement, and physical NVIDIA qualification remain open under
W0112/M0120.

## Cleanup

- Removed: none.
- Retained: queue-only worker regression, W0112 plan wording, and cdev README
  boundary text.

## roast

### light roasts

- Queue-only region COPY dispatch with CPU backend reference balancing ->
  `transports/cdev/worker/src/worker.cpp` and its regression test (`c3227c9`,
  focused 2/2, full 84/84)

### medium roasts

- Queue-only cdev worker boundary ->
  `agent/plan/M0110-kernel-guest-transport/work/W0112-local-cdev.md` (P081)

### dark roasts

- none.

## session-only

- External `/tmp/metaflux-no-cdev` build output remains under its existing build
  owner and is not copied into the repository.

## Handoff

Resume W0112 from `8095713` and P081. The next CUDA/cdev boundary is live
`/dev/metafluxctl` lease and registered-memory/backend attachment; do not claim
physical qualification from this host-independent worker test.
