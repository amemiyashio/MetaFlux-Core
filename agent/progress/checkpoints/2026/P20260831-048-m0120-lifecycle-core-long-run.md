---
id: P20260831-048
status: Recorded
captured: 2026-08-31
milestone: M0120
workstream: W0123
branch: main
git_revision: 72025af
workspace: lifecycle authority retains the W0123 1,000-cycle envelope; transport and host qualification remain open
---

# M0120 W0123 Lifecycle Core Long Run

## Outcome

The first W0123 core-qualification stage is recorded at content revision
`72025af`. The runtime `Coordinator` now retains 4,096 request replay records
and 2,048 immutable tombstones, enough for the defined 1,000 reset/remove/add
envelope without eviction. A host-independent regression runs all 1,000 cycles,
checks generation/identity high-water and epoch advancement, resolves every
retired generation as `DeviceLost`, exercises all three mirror callback streams,
and verifies replay/conflict behavior after the run.

This is authority-core evidence only. It does not claim the three concurrent
memfd/local-cdev/guest-QMP suites, kernel sanitizer/fuzz/soak gates, canonical
udev/node qualification, or lifecycle extension freeze.

## Verification evidence

| Gate | Result |
|---|---|
| Lifecycle core long-run fixture | Passed: 1,000 reset/remove/add cycles, 2,000 retired generations resolved as `DeviceLost`, three mirror streams checked |
| Focused lifecycle tests | Passed: 4/4 lifecycle, long-run, normalizer, and dispatch tests |
| Full development CTest | Passed: 83/83 |
| Formatting and diff checks | Passed: `clang-format` and `git diff --check` |
| Agent records | Pending the separate record commit for this checkpoint |
| Content identity | Passed: `72025af`, Agent Harness (codex) as Author and Committer |

## Boundary

W0123 remains Active. The next stages must bind this authority run to concurrent
memfd/cdev/guest-QMP activity, old-object lifetime and worker-death faults, and
kernel/package qualification before any `mf_admin_lifecycle_v1` freeze claim.

## Cleanup

- Removed: none; the external CMake build directory remains ignored and owned
  by the build workflow.
- Retained: lifecycle capacity documentation, long-run fixture, W0123 plan
  stage, and this checkpoint.

## roast

### light roasts

- Lifecycle replay/tombstone capacity constants -> `runtime/core/include/metaflux/runtime/lifecycle.hpp` (`72025af`; W0123 capacity calculation)
- Lifecycle retention boundary -> `runtime/core/README.md` (`72025af`; W0123 capacity calculation)
- 1,000-cycle lifecycle authority regression -> `runtime/core/tests/lifecycle_long_run.cpp` (`72025af`; focused and full dev CTest)

### medium roasts

- none.

### dark roasts

- none.

## session-only

- Host-independent lifecycle authority run - reason: live concurrent transport,
  QMP, kernel sanitizer, and physical-host qualification are outside this
  checkpoint.

## Handoff

Resume S0123 from this checkpoint with
`nix develop . --command ctest --preset dev`, then read W0123 and the
device-lifecycle-resilience, Linux UAPI, vfio-user, and PCI skills before
connecting the core evidence to transport and kernel fault suites.
