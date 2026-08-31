---
id: P20260901-093
status: Recorded
captured: 2026-09-01
milestone: M0110
workstream: W0112
branch: main
git_revision: 0bba77f87c8f0737513a6873eb8325af54ff4852
workspace: embedded daemon cdev backend reference drain
---

# M0110 W0112 Daemon Backend Reference Drain Checkpoint

## Outcome

The embedded daemon CPU path now binds each object-table host/device memory
object to one full-range backend handle. Region COPY retains both references
around the synchronous backend call. Resolver-created temporary imports are
tracked separately and reclaimed after their final operation reference;
persistent object handles are marked retired and reclaimed only after active
references drain. Session teardown releases the remaining backend handles
before backend destruction.

This is an internal lifetime breakthrough within W0112. It does not claim a
live `/dev/metafluxN` lease, kernel registered-memory import, replacement-
generation isolation, or Linux 6.12/6.18 fault qualification.

## Verification evidence

| Gate | Result |
| --- | --- |
| Daemon cdev backend build | Passed with `METAFLUX_DAEMON_CDEV_BACKEND=1` |
| Focused cdev/daemon CTest | Passed: 6/6 |
| Full development CTest | Passed: 85/85 |
| Diff checks | Passed: `git diff --check` |
| Agent records before record commit | Passed |

## Cleanup

- Removed: none.
- Retained: only the source change, plan boundary, current projection, and
  this compact checkpoint; build output remains outside the repository.

## roast

### light roasts

- none.

### medium roasts

- Embedded daemon object-table backend handles with operation-reference
  draining -> `services/metafluxd/server.cpp` (`0bba77f`; focused CTest 6/6,
  full CTest 85/85)

### dark roasts

- none.

## session-only

- none.

## Handoff

Continue as `S0112-20260901-001-m0110-w0112-current-epoch`. The next bounded
W0112 unit is the live daemon lease and registered-memory/backend attachment;
keep the current embedded lifetime result separate from live cdev and kernel
qualification claims.
