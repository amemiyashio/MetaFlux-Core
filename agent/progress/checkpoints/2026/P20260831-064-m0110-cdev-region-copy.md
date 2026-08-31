---
id: P20260831-064
status: Recorded
captured: 2026-08-31
milestone: M0110
workstream: W0112
branch: main
git_revision: 7bdcedf
workspace: cdev region COPY resolver boundary
---

# M0110 W0112 Region COPY Checkpoint

## Outcome

The cdev worker now accepts the region argument-block COPY form through a
backend-agnostic `CdevCopyResolver`. The resolver owns daemon/object-table
semantics and returns independent destination and source backend memory handles,
offsets, and a byte count. The worker validates handles, range arithmetic, and
reserved fields, holds the synchronous backend-operation lease across resolver
and `mf_backend_api_v1.copy`, and maps resolver/backend status into the shared
completion record. Direct-host COPY flags remain unsupported.

This is a host-independent backend import/resolution seam. It does not claim
registered-memory `dma_map_sg`, asynchronous ownership, or physical NVIDIA/CUDA
qualification.

## Verification evidence

| Gate | Result |
|---|---|
| cdev worker regression | Passed: region resolver success and resolver-stale paths, independent source/destination handles, lease ordering, and existing COPY/LAUNCH fault paths |
| Full development CTest | Passed: 84/84 |
| Build and formatting | Passed: CMake build, clang-format, and `git diff --check` |
| Agent records | Passed: 57 sessions / 387 events / 331 Markdown files |
| Content identity | Passed: `7bdcedf`, Agent Harness (codex) as Author and Committer |

## Boundary

The synchronous cdev worker now has bounded payload COPY, region COPY
resolution, and primary-entry LAUNCH dispatch through the CPU backend seam.
Production registered-memory import and DMA mapping, asynchronous lease
retention through observed completion, daemon-controlled generation replacement,
and KUnit/KASAN/KCSAN/lockdep/kmemleak qualification remain open.

## Cleanup

- Removed: temporary debug diagnostics from the region resolver regression.
- Retained: the resolver contract, focused regression, W0112 boundary, and this
  compact checkpoint.

## roast

### light roasts

- Region COPY resolver contract ->
  `transports/cdev/worker/include/metaflux/transport/cdev_worker.hpp` and
  `transports/cdev/worker/src/worker.cpp` (`7bdcedf`; cdev worker regression)
- Independent backend memory handles and lease ordering ->
  `transports/cdev/worker/tests/worker_test.cpp` (`7bdcedf`; full CTest 84/84)

### medium roasts

- W0112 region-copy import boundary ->
  `agent/plan/M0110-kernel-guest-transport/work/W0112-local-cdev.md` (P064;
  registered-memory DMA and device qualification remain open)

### dark roasts

- none.

## session-only

- Host-local resolver/backend fixture - reason: it verifies ABI and lease
  ordering without claiming physical-device or asynchronous qualification.

## Handoff

Resume S0112/W0112 from `7bdcedf` and P064. Connect the resolver to
generation-bound registered-memory import and DMA mapping, then extend backend
ownership through observed asynchronous completion before daemon replacement.
