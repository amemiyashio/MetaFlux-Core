---
id: P20260831-050
status: Recorded
captured: 2026-08-31
milestone: M0130
workstream: W0134
branch: main
git_revision: d9e1ef9
workspace: host-independent command-resource ownership is verified; Vulkan queue integration remains open
---

# M0130 W0134 Vulkan Command Resource Recycling

## Outcome

The host-independent command-resource stage is recorded at content revision
`d9e1ef9`. `CommandResourcePool` assigns finite resources to a generation and
stream, gives each handle a monotonic identity and sequence, and accepts a
submission only with a strictly increasing completion timeline. A submitted
resource remains in flight until an observed completion reaches its value;
generation reconfiguration is rejected while acquired or submitted resources
remain, and retired handles are stale after a successful switch.

This is ownership and admission evidence. It does not claim Vulkan command
buffer allocation, `vkQueueSubmit2`, pipeline creation, timeline semaphore
integration, composed provider/runtime behavior, or driver qualification.

## Verification evidence

| Gate | Result |
|---|---|
| Command-resource pool regression | Passed: exhaustion, no early recycle, monotonic completion, stale handles, and busy reconfigure |
| Focused stream-graph CTest | Passed: `metaflux.backend.vulkan-stream-graph` |
| Full Vulkan CTest | Passed: 88/88 |
| Formatting and diff checks | Passed: `clang-format` and `git diff --check` |
| Content identity | Passed: `d9e1ef9`, Agent Harness (codex) as Author and Committer |
| Agent records | Pending the separate record commit for this checkpoint |

## Boundary

W0134 remains Active. The next stage must attach the pool to real Vulkan command
buffers and pipeline ownership, submit through `vkQueueSubmit2` with timeline
semaphores, and preserve the stream planner's dependency admission. Physical
driver-family and provider/runtime qualification remain open.

## Cleanup

- Removed: none; the external CMake build directory remains ignored and owned
  by the build workflow.
- Retained: command-resource pool implementation/tests, W0134 plan stage, and
  this checkpoint.

## roast

### light roasts

- Command-resource ownership and completion-gated recycling -> `plugins/backend/vulkan/runtime/src/streams.cpp` (`d9e1ef9`; full Vulkan CTest)
- Generation retirement and in-flight reconfigure guard -> `plugins/backend/vulkan/runtime/tests/streams_test.cpp` (`d9e1ef9`; stale and busy regressions)

### medium roasts

- none.

### dark roasts

- none.

## session-only

- Host-independent resource lifecycle - reason: physical queue submission,
  pipeline execution, and driver qualification are outside this checkpoint.

## Handoff

Resume W0134 from this checkpoint with
`nix develop .#vulkan --command ctest --preset vulkan`, then read the W0134 plan
and Vulkan memory/synchronization reference before implementing queue submission.
