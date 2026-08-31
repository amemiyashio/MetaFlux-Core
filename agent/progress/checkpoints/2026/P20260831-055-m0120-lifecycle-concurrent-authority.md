---
id: P20260831-055
status: Recorded
captured: 2026-08-31
milestone: M0120
workstream: W0123
branch: main
git_revision: f8a786a
workspace: Coordinator control-plane serialization and host-independent concurrent replay/observer evidence are recorded; live transport qualification remains open
---

# M0120/W0123 Concurrent Lifecycle Authority Checkpoint

## Outcome

The lifecycle Coordinator now serializes public mutation and observation APIs
with one reentrant authority mutex. This protects lifecycle state, bounded
request replay records, tombstones, and generation high-water marks when reset,
remove, and add requests race with read-only open/mmap/telemetry-style
observations or duplicate submit replays. A move constructor preserves the
existing factory-return usage while a live Coordinator is move-only.

The host-independent fixture races four identical reset submissions and then
executes 128 reset/remove/add cycles alongside observer threads. It proves one
accepted replay, idempotent duplicates, legal intermediate states, and final
generation/tombstone invariants. This does not claim that the real memfd,
local-cdev, guest-QMP, kernel, or process-death qualification suites pass.

## Changes

- Added recursive authority locking to Coordinator public accessors and
  mutation entry points, with locked state transfer for move construction.
- Added concurrent replay and observer coverage to the lifecycle long-run
  fixture.
- Documented the synchronization ownership boundary and retained the W0123
  real-transport/fault gates as Active.

## Verification

| Gate | Result |
| --- | --- |
| Concurrent replay | Four identical reset submissions produced one Accepted and three Duplicate results |
| Concurrent activity | Open, mmap, submit-replay, and telemetry-style observers raced 128 reset/remove/add cycles with no illegal state or stale-generation observation |
| Full development CTest | 83/83 passed |
| Formatting and patch hygiene | `clang-format` and `git diff --check` passed |
| Product identity | `f8a786a`; Author and Committer are `Agent Harness (codex)` |
| Agent records | Recorded separately after this checkpoint |

## Boundary

W0123 remains Active. The next stage must bind the concurrency envelope to real
memfd/cdev/guest-QMP open, mmap, submit, completion, and telemetry activity;
cover worker/daemon/QEMU death, fd/VMA/DMA/queue/event tombstones, kernel
sanitizers and fault injection; and only then freeze the lifecycle extension.

## Cleanup

No product or build artifacts were removed. The external development build
directory remains outside the repository record.

## roast

### light roasts

- Coordinator authority serialization -> `runtime/core/src/lifecycle.cpp` (`f8a786a`; development CTest 83/83)
- Duplicate replay linearization -> `runtime/core/tests/lifecycle_long_run.cpp` (`f8a786a`; four-way replay)
- Concurrent snapshot/tombstone observation -> `runtime/core/include/metaflux/runtime/lifecycle.hpp` (`f8a786a`; 128-cycle observer fixture)

### medium roasts

- none.

### dark roasts

- none.

## session-only

- Real transport and kernel lifecycle qualification - reason: this checkpoint covers the host-independent authority API and does not substitute for memfd/cdev/guest-QMP or sanitizer evidence

## Handoff

```sh
nix develop .#default --command ctest --preset dev
```

Read W0123, `device-lifecycle-resilience`, and `runtime-contracts-registry`
before integrating live transport callbacks.
