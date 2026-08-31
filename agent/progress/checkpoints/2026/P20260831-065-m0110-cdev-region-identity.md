---
id: P20260831-065
status: Recorded
captured: 2026-08-31
milestone: M0110
workstream: W0112
branch: main
git_revision: 301c379
workspace: cdev region COPY object identity correction
---

# M0110 W0112 Region COPY Identity Checkpoint

## Outcome

The cdev region COPY path now follows the shared descriptor identity contract:
`target_id` carries the argument-block object ID, `arguments[0]` carries its
generation, and `arguments[1..3]` are reserved zero. The worker bypasses the
queue-generation check only for this region form and delegates object-generation
freshness to the resolver; legacy payload COPY and cdev LAUNCH retain their
queue-generation check. The cdev client now exposes matching descriptor and
submit helpers.

## Verification evidence

| Gate | Result |
|---|---|
| cdev client/worker regression | Passed: descriptor encoding, non-queue target identity, reserved-field rejection, resolver/backend lease ordering, and existing launch/copy paths |
| Full development CTest | Passed: 84/84 |
| Build and formatting | Passed: CMake build, clang-format, and `git diff --check` |
| Agent records | Passed: 57 sessions / 388 events / 332 Markdown files |
| Content identity | Passed: `301c379`, Agent Harness (codex) as Author and Committer |

## Boundary

Region COPY identity and cdev submission encoding are now coherent with the
shared fastpath. This remains a host-independent synchronous seam: production
registered-memory import and DMA mapping, asynchronous lease retention,
generation replacement, and physical NVIDIA/CUDA qualification remain open.

## Cleanup

- Removed: formatter-only changes and temporary diagnostics from the identity
  regression.
- Retained: cdev region descriptor helpers, worker identity validation, focused
  regressions, and this compact checkpoint.

## roast

### light roasts

- Region descriptor identity encoding ->
  `transports/cdev/client/include/metaflux/transport/cdev.h` and
  `transports/cdev/client/src/cdev.c` (`301c379`; cdev client regression)
- Queue/object generation split ->
  `transports/cdev/worker/src/worker.cpp` and
  `transports/cdev/worker/tests/worker_test.cpp` (`301c379`; full CTest 84/84)

### medium roasts

- W0112 COPY object-table boundary ->
  `agent/plan/M0110-kernel-guest-transport/work/W0112-local-cdev.md` (P065;
  registered-memory DMA and device qualification remain open)

### dark roasts

- none.

## session-only

- Host-local object identity fixture - reason: it validates descriptor semantics
  without claiming daemon object-table or physical-device qualification.

## Handoff

Resume S0112/W0112 from `301c379` and P065. Bind resolver object generations to
registered-memory backend import and DMA mapping, then extend operation ownership
through observed asynchronous completion before daemon replacement.
