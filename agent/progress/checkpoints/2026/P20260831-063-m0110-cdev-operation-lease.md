---
id: P20260831-063
status: Recorded
captured: 2026-08-31
milestone: M0110
workstream: W0112
branch: main
git_revision: fcdcbbd
workspace: cdev synchronous backend-operation lease boundary
---

# M0110 W0112 Backend Operation Lease Checkpoint

## Outcome

The cdev worker now requires every bound backend to provide a synchronous
operation lease pair. COPY and LAUNCH acquire the lease before resolver or
backend access, surface a rejected admission in the completion record, and
release it after the synchronous backend ABI call returns. A bound request with
no lease is rejected as unsupported; it never falls back to local COPY.

The lease is deliberately host-independent. It gives the future registered-
memory DMA owner a single admission boundary for backend instance, queue, and
memory handles, while keeping asynchronous completion and actual kernel DMA
mapping outside this checkpoint.

## Verification evidence

| Gate | Result |
|---|---|
| cdev worker regression | Passed: successful COPY/LAUNCH, backend timeout, resolver stale/payload failure, malformed flags, missing lease, and lease rejection |
| Full development CTest | Passed: 84/84 |
| Build and formatting | Passed: CMake build, clang-format, and `git diff --check` |
| Agent records | Passed: 57 sessions / 385 events / 329 Markdown files |
| Content identity | Passed: `fcdcbbd`, Agent Harness (codex) as Author and Committer |

## Boundary

This stage closes synchronous worker/backend admission only. The lease ends
when the synchronous backend API returns; an asynchronous backend must retain it
until an observed completion. Registered-memory `dma_map_sg`, production
resolver wiring, daemon-controlled generation replacement, and KUnit/KASAN/
KCSAN/lockdep/kmemleak fault qualification remain open.

## Cleanup

- Removed: no session-owned disposable artifact; ignored build outputs remain
  in the external build tree.
- Retained: cdev lease contract, regression coverage, W0112 plan boundary, and
  this compact checkpoint.

## roast

### light roasts

- Synchronous cdev backend-operation lease ->
  `transports/cdev/worker/include/metaflux/transport/cdev_worker.hpp` and
  `transports/cdev/worker/src/worker.cpp` (`fcdcbbd`; cdev worker regression)
- Lease rejection and release ordering ->
  `transports/cdev/worker/tests/worker_test.cpp` (`fcdcbbd`; full CTest 84/84)

### medium roasts

- W0112 backend lifetime boundary ->
  `agent/plan/M0110-kernel-guest-transport/work/W0112-local-cdev.md` (P063;
  asynchronous retention and DMA remain open)

### dark roasts

- none.

## session-only

- Host-local synchronous lease fixture - reason: it verifies ordering without
  claiming physical NVIDIA or asynchronous device qualification.

## Handoff

Resume S0112/W0112 from `fcdcbbd` and P063. Connect the lease owner to
generation-bound registered-memory import and DMA mapping, then extend the
ownership through observed asynchronous completion before attempting daemon
generation replacement.
