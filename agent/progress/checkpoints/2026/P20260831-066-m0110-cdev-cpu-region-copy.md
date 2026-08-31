---
id: P20260831-066
status: Recorded
captured: 2026-08-31
milestone: M0110
workstream: W0112
branch: main
git_revision: b15a208
workspace: cdev CPU backend region COPY evidence
---

# M0110 W0112 CPU Region COPY Checkpoint

## Outcome

The cdev worker regression now drives a region COPY through the real CPU
backend ABI. Two independent imported host-memory handles are returned by the
resolver, the worker holds its synchronous lease across resolution and copy,
and the backend copies the requested byte range with the expected result.

## Verification evidence

| Gate | Result |
|---|---|
| cdev CPU region COPY regression | Passed: independent source/destination handles, byte offsets, resolver invocation, lease ordering, completion status, and data result |
| Full development CTest | Passed: 84/84 |
| Build and formatting | Passed: CMake build, clang-format, and `git diff --check` |
| Agent records | Passed: 57 sessions / 389 events / 333 Markdown files |
| Content identity | Passed: `b15a208`, Agent Harness (codex) as Author and Committer |

## Boundary

This proves the host-independent cdev-to-CPU backend region COPY seam. It does
not prove kernel registered-memory DMA mapping, asynchronous completion
ownership, daemon generation replacement, or physical NVIDIA/CUDA execution.

## Cleanup

- Removed: no session-owned disposable artifacts; imported host handles are
  released by the regression before teardown.
- Retained: the real CPU region-copy regression and its compact evidence.

## roast

### light roasts

- Real CPU backend region COPY through cdev ->
  `transports/cdev/worker/tests/worker_test.cpp` (`b15a208`; full CTest 84/84)

### medium roasts

- W0112 host-independent copy seam ->
  `agent/plan/M0110-kernel-guest-transport/work/W0112-local-cdev.md` (P066;
  registered-memory DMA and asynchronous ownership remain open)

### dark roasts

- none.

## session-only

- Imported host-memory fixture - reason: it exercises backend handle and range
  semantics without claiming a physical accelerator or kernel DMA map.

## Handoff

Resume S0112/W0112 from `b15a208` and P066. Replace the host fixture with
generation-bound registered-memory import and DMA mapping, then extend the
operation lease through observed asynchronous completion.
