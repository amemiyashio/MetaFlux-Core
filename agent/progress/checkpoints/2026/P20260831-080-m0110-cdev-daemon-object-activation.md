---
id: P20260831-080
status: Recorded
captured: 2026-08-31
milestone: M0110
workstream: W0112
branch: main
git_revision: 4bddd92d42bc516d1853206af9e94cc4e4e9e8a6
workspace: daemon object-table activation for embedded CPU region COPY
---

# M0110 W0112 Daemon Object-Table Activation Checkpoint

## Outcome

The daemon's embedded CPU region-COPY path now composes the authoritative
object table with `CdevObjectTableResolver` and the CPU backend C ABI. Each
session owns a backend instance/context/queue; object identity, generation,
permissions, and ranges are checked before importing validated subranges. The
backend copy executes through `mf_backend_api_v1.copy`, imported handles are
released on every return path, and the existing completion and accounting
records remain unchanged. Cdev transport activation remains conditional, so a
daemon build with `METAFLUX_BUILD_CDEV_TRANSPORT=OFF` retains its prior path.

## Verification evidence

| Gate | Result |
|---|---|
| Focused cdev worker and daemon integration | Passed: 2/2 |
| Full development CTest | Passed: 84/84 |
| cdev-disabled daemon configure/build | Passed |
| Diff checks | Passed: `git diff --check` |
| Record checks | Passed before content commit |

## Boundary

This proves daemon-side object-table activation with the host-independent CPU
backend. Live `/dev/metafluxctl` lease attachment, kernel DMA-backed registered
memory, generation replacement, non-cancellable backend wait policy, and
physical CUDA/NVIDIA qualification remain open under W0112/M0120.

## Cleanup

- Removed: none.
- Retained: daemon callback bridge, cdev resolver range-status correction,
  transport documentation, W0112 plan update, and session records.

## roast

### light roasts

- Daemon object lookup/import and synchronous CPU backend COPY ->
  `services/metafluxd/server.cpp` and cdev resolver (`daemon-cross-process`,
  full CTest 84/84)

### medium roasts

- W0112 embedded daemon object-table activation -> W0112 plan and progress
  (`P20260831-080`)

### dark roasts

- none.

## session-only

- cdev-disabled configure/build output under `/tmp/metaflux-no-cdev` remains an
  external build artifact and is not copied into the repository.

## Handoff

Resume W0112 from the content revision recorded below. Read the cdev resolver,
daemon callback bridge, and live UAPI ownership records before attempting
`/dev/metafluxctl` activation or M0120 generation replacement.
