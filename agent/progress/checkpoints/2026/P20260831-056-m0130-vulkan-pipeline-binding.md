---
id: P20260831-056
status: Recorded
captured: 2026-08-31
milestone: M0130
workstream: W0135
branch: main
git_revision: c37d312
workspace: generation-scoped cache pipeline binding is verified; Vulkan object and warm-launch qualification remain open
---

# M0130 W0135 Vulkan Pipeline Binding Checkpoint

## Outcome

The host-independent cache repository now records a generation-scoped pipeline
binding boundary at content revision `c37d312`. Acquisition requires a
validated resident device-bound hit, pins one active binding per cache key, and
rejects duplicate or stale generations. Release requires the matching
generation and is the prerequisite for device invalidation to remove the entry;
generic `unpin` cannot bypass an active pipeline binding.

This checkpoint records cache lifecycle evidence only. It does not claim actual
`VkPipeline` or opaque `VkPipelineCache` creation, compiler-free warm launch,
physical Vulkan execution, or driver-family qualification.

## Verification evidence

| Gate | Result |
|---|---|
| Pipeline binding regression | Passed: invalid/resident-miss acquisition, first/duplicate/stale generation handling, release ownership, generic-unpin guard, and invalidation ordering |
| Focused cache CTest | Passed: `metaflux.backend.vulkan-cache-model` |
| Full Vulkan CTest | Passed: 88/88 |
| Formatting and diff checks | Passed: `clang-format` and `git diff --check` |
| Content identity | Passed: `c37d312`, Agent Harness (codex) as Author and Committer |
| Agent records | Pending the separate record commit for this checkpoint |

## Boundary

W0135 remains Active. The next stage must bind the repository contract to a real
Vulkan pipeline identity, verify every target/driver/ABI mutation produces a
miss, and trace warm launch for compiler, validator, shader-module, pipeline,
Vulkan allocation, and MetaFlux-owned allocation exclusions. Physical driver
qualification remains outside this host's evidence.

## Cleanup

- Removed: none; temporary cache directories self-cleaned by the regression.
- Retained: pipeline binding source/tests and plan updates in Git; no build,
  download, source snapshot, or ordinary log artifact was retained.

## roast

### light roasts

- Generation-scoped pipeline binding -> `plugins/backend/vulkan/runtime/src/cache.cpp` (`c37d312`; full Vulkan CTest 88/88)
- Binding ownership and stale-generation status -> `plugins/backend/vulkan/runtime/include/metaflux/backend/vulkan_cache.hpp` (`c37d312`; cache regression)
- Release-before-invalidation rule -> `plugins/backend/vulkan/README.md` (`c37d312`; invalidation ordering regression)

### medium roasts

- none.

### dark roasts

- none.

## session-only

- Actual Vulkan pipeline objects, warm-launch exclusion trace, and physical
  driver qualification - reason: this host-independent checkpoint has no
  corresponding Vulkan device or driver evidence

## Handoff

Resume W0135 with:

```sh
nix develop .#vulkan --command ctest --preset vulkan
```

Read W0135 and the Vulkan cache/device-loss reference before adding real
pipeline ownership or warm-launch tracing.
