---
id: P20260831-044
status: Recorded
captured: 2026-08-31
milestone: M0130
workstream: W0133
branch: main
git_revision: 86441c7
workspace: Vulkan target and module preflight diagnostics are tested; MLIR/SPIR-V conversion and execution remain open
---

# M0130 W0133 Vulkan Target Preflight

## Outcome

The first W0133 stage is recorded at content revision `86441c7`. The Vulkan
runtime now validates the W0131 capability profile and exact target digest before
future SPIR-V module creation. It enforces required feature bits, workgroup
dimensions and invocation limits, known address spaces, explicit BDA intent,
and the rule that subgroup width is only accepted when the device proves one
exact width.

This is an admission/preflight layer only. It does not claim MLIR conversion,
SPIR-V validation/reflection, pipeline creation, or execution.

## Verification evidence

| Gate | Result |
|---|---|
| Target preflight | Passed: profile, feature, digest, workgroup, address-space, BDA, and subgroup positive/negative fixtures |
| Vulkan configure/build | Passed with C++20 using `nix develop .#vulkan --command cmake --preset vulkan` |
| Full Vulkan CTest | Passed: 85/85, including `metaflux.backend.vulkan-target-preflight` |
| Repository gates | Passed: `python3 tools/check-agent-records.py .` and `git diff --check` |
| Content identity | Passed: `86441c7`, Agent Harness (codex) as Author and Committer |

## Boundary

W0133 remains Active. The next stage must add actual Kernel IR to MLIR SPIR-V
conversion, target-environment validation/reflection, and differential fixtures.
Unsupported semantics must continue to fail before pipeline creation.

## Cleanup

- Removed: none; the external CMake build directory remains ignored and owned
  by the build workflow.
- Retained: target-preflight source, tests, W0133 plan update, and this
  checkpoint.

## roast

### light roasts

- Target profile/module admission -> `plugins/backend/vulkan/runtime/src/target.cpp` (content `86441c7`; preflight CTest)

### medium roasts

- none.

### dark roasts

- none.

## session-only

- Physical SPIR-V module creation is not qualified on this host - reason: no
  discoverable Vulkan ICD and no MLIR/SPIR-V conversion stage is present yet.

## Handoff

Resume S0133 from this checkpoint and W0133. Run
`nix develop .#vulkan --command ctest --preset vulkan`, then read the MLIR,
PTX-semantics, and Vulkan skills before adding a conversion pass.
