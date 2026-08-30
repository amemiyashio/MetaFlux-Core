---
id: P20260831-019
status: Recorded
captured: 2026-08-31
milestone: M0120
workstream: W0122
branch: main
git_revision: 3e89434af6fae4d52b4c7cfdbac20c602fdd1a2d
workspace: W0122 coordinator and bounded transport mirror stage is implemented; concrete adapters and qualification remain active
---

# M0120 W0122 lifecycle coordinator

## Outcome

The runtime core now owns a normalized lifecycle transaction boundary for one
logical device. `Coordinator` accepts requests from all planned sources,
checks daemon incarnation and expected generation/epoch, records idempotent
request IDs, reserves never-reused generation and identity candidates, and
commits retirement epochs only with the replacement transaction. `Mirror`
callbacks provide the bounded memfd, cdev, and guest vfio-user adapter seam for
prepare, quiesce, drain, commit, abort, and loss publication. Intermediate
events expose `PRESENT`, `QUIESCING`, `DRAINING`, and `RESETTING` progress
without making a staged candidate current.

Pre-commit failure consumes a reserved candidate while preserving the old
identity and epoch. A successful reset/recovery retires the old generation and
increments epoch once; a partial commit marks the candidate lost rather than
reopening the old generation. Transport loss preserves the current generation
and epoch as lost, remove reaches `ABSENT`, and all retired generations resolve
as `DeviceLost`.

## Verification evidence

| Gate | Result |
|---|---|
| Focused lifecycle CTest | Passed: `metaflux.unit.runtime-lifecycle` |
| Full development CTest | Passed: 75/75 |
| Agent records | Passed: `python3 tools/check-agent-records.py .` |
| Commit identity | `Agent Harness (codex)` as Author and Committer for `3e89434` |

## Boundary

This is a runtime-core contract and fixture stage. It does not alter the frozen
M0110 descriptor, base ioctl/mmap UAPI, BAR layout, vfio-user wire records, CUDA
or NVML provider views, or QMP implementation. W0122 remains Active until the
concrete memfd/cdev/guest adapters consume this seam, all reset/disconnect/
restart and staging/commit/DMA/completion/teardown faults are injected, and
provider enumeration freeze is qualified.

## Cleanup

- Removed: none; no session-owned disposable artifact was created.
- Retained: canonical runtime source, plan, test, and compact session records;
  external build output remains under CMake/Ninja ownership.

## Handoff

Start from W0122 and this checkpoint. Keep the coordinator as the only
generation/epoch publisher; adapter callbacks may mirror or tombstone state but
may not allocate or publish replacement identity independently.
