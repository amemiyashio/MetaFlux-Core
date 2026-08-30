---
id: P20260831-045
status: Recorded
captured: 2026-08-31
milestone: M0130
workstream: W0134
branch: main
git_revision: c3b8c5e
workspace: Vulkan stream/dependency planning is tested; queue submission and pipeline execution remain open
---

# M0130 W0134 Vulkan Stream Graph

## Outcome

The first W0134 stage is recorded at content revision `c3b8c5e`. `StreamGraph`
assigns monotonic submission sequences, adds the previous same-stream timeline
edge, preserves independent streams without fabricated ordering, and accepts
cross-stream waits only when the referenced value is already published. Copy
and launch plans require their respective transfer/compute stage-access masks;
stale generations, unknown/future/duplicate dependencies, and bounded overflow
are rejected before a future `vkQueueSubmit2` call.

This is a host-independent planning layer. It does not claim Vulkan command
buffers, queue submission, pipeline execution, validation layers, or timing.

## Verification evidence

| Gate | Result |
|---|---|
| Stream graph | Passed: FIFO, independent streams, explicit waits, visibility masks, and negative paths |
| Vulkan configure/build | Passed with C++20 using `nix develop .#vulkan --command cmake --preset vulkan` |
| Full Vulkan CTest | Passed: 86/86, including `metaflux.backend.vulkan-stream-graph` |
| Repository gates | Passed: `python3 tools/check-agent-records.py .` and `git diff --check` |
| Content identity | Passed: `c3b8c5e`, Agent Harness (codex) as Author and Committer |

## Boundary

W0134 remains Active. Next actions are command-resource recycling, actual
`vkQueueSubmit2` and timeline completion integration, and composed provider /
runtime dependency evidence on a qualified device. Unsupported kernels must
continue to fail before pipeline creation.

## Cleanup

- Removed: none; the external CMake build directory remains ignored and owned
  by the build workflow.
- Retained: W0134 stream planner source, tests, plan update, and this checkpoint.

## roast

### light roasts

- Stream ordering and dependency planning -> `plugins/backend/vulkan/runtime/src/streams.cpp` (content `c3b8c5e`; stream graph CTest)

### medium roasts

- none.

### dark roasts

- none.

## session-only

- Physical queue submission is not qualified on this host - reason: no
  discoverable Vulkan ICD and no `vkQueueSubmit2` execution path exists yet.

## Handoff

Resume S0134 from this checkpoint and W0134. Run
`nix develop .#vulkan --command ctest --preset vulkan`, then read the Vulkan,
runtime-contracts, and provider skills before wiring queue submission.
