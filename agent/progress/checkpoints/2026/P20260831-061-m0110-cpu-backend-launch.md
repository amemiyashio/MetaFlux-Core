---
id: P20260831-061
status: Recorded
captured: 2026-08-31
milestone: M0110
workstream: W0112
branch: main
git_revision: b7dbabf
workspace: CPU backend synchronous canonical-KIR module and Add/Copy submit
---

# M0110 W0112 CPU Backend Launch Checkpoint

## Outcome

The CPU backend now exposes an additive `MF_BACKEND_CAP_LAUNCH` capability and
executes pre-serialized canonical Kernel IR through the versioned backend C ABI.
The bounded subset owns a canonical KIR copy per module, validates instance,
queue, module, memory-handle, range, dimension, and argument-block ownership,
and maps interpreter outcomes to backend statuses. A private fixed-width CPU
argument block carries buffer handles plus U32/F32 scalar values, so the Add
fixture runs through the same `load_module`/`submit` ABI that the cdev worker
will consume.

## Verification evidence

| Gate | Result |
|---|---|
| Development configure/build | Passed: CMake dev configure and parallel build |
| CPU backend launch focused CTest | Passed: `metaflux.backend.cpu-launch` 1/1 |
| Focused transport regression | Passed: CPU launch, CPU ABI, and cdev worker 3/3 |
| Full development CTest | Passed: 84/84 |
| Formatting and diff checks | Passed: clang-format and `git diff --check` |
| Record validation | Passed before this record commit |
| Content identity | Passed: `b7dbabf`, Agent Harness (codex) as Author and Committer |

## Boundary

This checkpoint proves the CPU backend's synchronous ABI prerequisite and its
standalone Add/Copy execution path. It does not close W0112: cdev
descriptor-to-launch resolution, generation-bound registered-memory/DMA mapping,
backend reference drain, daemon replacement, asynchronous completion, and
kernel/transport fault qualification remain open. The submit path is a bounded
CPU fixture and still serializes backend state under its local mutex.

## Cleanup

- Removed: none; no build tree, source snapshot, dependency store, download, or
  routine command log was copied into Agent records.
- Retained: the ABI addition, CPU argument-block definitions, focused Add test,
  progress boundary, and this compact checkpoint.

## roast

### light roasts

- CPU launch ABI capability and table wiring -> `contracts/plugin/backend/v1/include/metaflux/backend/api.h` and `plugins/backend/cpu/runtime/src/backend.cpp` (`b7dbabf`; full CTest)
- Private CPU argument-block contract -> `plugins/backend/cpu/runtime/include/metaflux/backend/cpu.h` (`b7dbabf`; focused Add regression)
- Canonical Add execution evidence -> `tests/unit/backend_cpu_launch.cpp` (`b7dbabf`; `metaflux.backend.cpu-launch`)

### medium roasts

- W0112 CPU backend integration boundary -> `agent/plan/M0110-kernel-guest-transport/work/W0112-local-cdev.md` and this checkpoint (`b7dbabf`; cdev launch/DMA gates remain explicit)

### dark roasts

- none.

## session-only

- none; build and test output remains owned by CMake/CTest and the current
  host's CPU fixture is not promoted as physical NVIDIA qualification.

## Handoff

Resume from `b7dbabf` and P061. Read W0112 and the backend dispatch seam, then
extend the leased cdev worker with an explicit generation-bound launch resolver
that returns a fully prepared backend launch record. Keep descriptor/object
translation outside the backend ABI and prove stale-generation and malformed
launch rejection before attempting registered-memory DMA.
