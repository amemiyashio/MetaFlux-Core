---
id: P20260831-054
status: Recorded
captured: 2026-08-31
milestone: M0130
workstream: W0132
branch: main
git_revision: 96d3a55
workspace: Host-independent staging memory visibility and non-coherent range rules are recorded; physical allocation remains open
---

# M0130/W0132 Memory Visibility Checkpoint

## Outcome

The W0132 staging baseline now has a host-independent memory visibility ledger.
Each registered allocation is bound to a generation and submission timeline.
Host writes on non-coherent memory create dirty ranges that require an explicit
flush before device access. Device writes create dirty ranges that require an
invalidate before host access. Ranges are checked against the configured
non-coherent atom size, and in-flight allocations reject host access,
reconfiguration, and unregister. Coherent allocations use the same lifetime
and timeline checks with no-op visibility transitions.

This is a contract/model checkpoint. It does not claim physical
`VkDeviceMemory`, Vulkan flush/invalidate calls, external-handle import, queue
submission, lifecycle integration, or driver qualification.

## Changes

- Added `MemoryVisibilityLedger`, `VisibilityRange`, and stable visibility
  statuses to the Vulkan runtime memory model.
- Implemented dirty interval merge/subtraction, atom-size range validation,
  explicit host/device visibility transitions, and generation/timeline lifetime
  guards.
- Added coherent, non-coherent, partial-range, stale, timeline, duplicate,
  in-flight, and teardown regressions and documented the boundary.

## Verification

| Gate | Result |
| --- | --- |
| Focused memory test | Passed coherent/non-coherent access, partial dirty ranges, flush/invalidate, stale generation, timeline, duplicate registration, and teardown fixtures |
| Full Vulkan CTest | 88/88 passed |
| Formatting and patch hygiene | `clang-format` and `git diff --check` passed |
| Product identity | `96d3a55`; Author and Committer are `Agent Harness (codex)` |
| Agent records | Recorded separately after this checkpoint |

## Boundary

W0132 remains Active. The next implementation stage must bind the ledger to
qualified Vulkan instance/device/queue discovery, memory-type selection,
`VkDeviceMemory` allocation, mapped flush/invalidate calls, staging copies,
external-handle ownership, lifecycle drain, and driver qualification. Keep the
host-independent model separate from physical evidence.

## Cleanup

No product or source snapshots were added. Build output remains in the external
build directory and is not part of the repository record.

## roast

### light roasts

- Host/device dirty-range admission -> `plugins/backend/vulkan/runtime/src/memory.cpp` (`96d3a55`; full Vulkan CTest 88/88)
- Non-coherent atom-size enforcement -> `plugins/backend/vulkan/runtime/include/metaflux/backend/vulkan_memory.hpp` (`96d3a55`; range fixtures)
- In-flight teardown guards -> `plugins/backend/vulkan/runtime/tests/memory_test.cpp` (`96d3a55`; lifetime regressions)

### medium roasts

- none.

### dark roasts

- none.

## session-only

- Physical Vulkan allocation and external-handle import - reason: no qualified physical Vulkan device is available in this host-independent checkpoint

## Handoff

```sh
nix develop .#vulkan --command ctest --preset vulkan
```

Read W0132, `vulkan-spirv-compute`, and its memory-sync reference before adding
the physical allocation adapter.
