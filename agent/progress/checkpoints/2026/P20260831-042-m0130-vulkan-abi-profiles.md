---
id: P20260831-042
status: Recorded
captured: 2026-08-31
milestone: M0130
workstream: W0131
branch: main
git_revision: 21ed444
workspace: Vulkan packed argument and external-memory 0.x profiles are fixed and tested; device-family qualification and execution remain open
---

# M0130 W0131 Vulkan ABI Profiles

## Outcome

The second W0131 stage is recorded at content revision `21ed444`. The backend
now exposes a target-digest-bound packed argument block with scalar and
generation-bound device-address entries, plus an external-memory 0.x profile
that distinguishes staging from direct OPAQUE_FD or DMA-BUF import. C fixtures
cover header sizes, target mismatch, generation/range rejection, and the rule
that direct-import flags cannot appear on the staging tier.

This remains a pre-execution contract. It does not claim a Vulkan allocation,
external handle import, synchronization interop, lowering, pipeline creation,
or physical driver-family qualification.

## Verification evidence

| Gate | Result |
|---|---|
| Packed argument ABI | Passed: 64-byte header and 48-byte entries; scalar/device-address positive and negative fixtures |
| External-memory ABI | Passed: 96-byte profile; staging and direct-import tier fixtures |
| Full Vulkan CTest | Passed: 83/83 after configure/build with `.#vulkan` |
| Repository gates | Passed: `python3 tools/check-agent-records.py .` and `git diff --check` |
| Content identity | Passed: `21ed444`, Agent Harness (codex) as Author and Committer |

## Boundary

W0131 remains Active. Exact feature/limit minimums and two independent Vulkan
driver families are unresolved. The next work moves to W0132 device-memory
bring-up or W0133 target-constrained lowering, and must keep direct memory
tiers capability-gated and staging as the only baseline.

## Cleanup

- Removed: none; the external CMake build directory remains ignored and owned
  by the build workflow.
- Retained: ABI headers, C fixtures, documentation, W0131 plan, and this
  checkpoint.

## roast

### light roasts

- Packed BDA argument layout -> `contracts/plugin/backend/v1/include/metaflux/backend/vulkan_arguments.h` (content `21ed444`; 64/48-byte layout and negative fixture)
- External-memory tier profile -> `contracts/plugin/backend/v1/include/metaflux/backend/vulkan_memory.h` (content `21ed444`; staging/direct-import fixture)

### medium roasts

- none.

### dark roasts

- none.

## session-only

- Direct-import profiles are contract fixtures only - reason: no matching
  physical Vulkan ICD or cross-driver synchronization evidence exists on the
  current AMD host.

## Handoff

Resume S0131 from this checkpoint. Read W0132/W0133 and the Vulkan plus
runtime-contract skills before implementing allocations or lowering; run
`nix develop .#vulkan --command ctest --preset vulkan` as the regression gate.
