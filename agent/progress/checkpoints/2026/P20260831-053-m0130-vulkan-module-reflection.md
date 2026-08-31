---
id: P20260831-053
status: Recorded
captured: 2026-08-31
milestone: M0130
workstream: W0133
branch: main
git_revision: d25617a
workspace: Host-independent SPIR-V reflection and packed-BDA verification is recorded; MLIR emission remains open
---

# M0130/W0133 SPIR-V Module Reflection Checkpoint

## Outcome

The target-preflight boundary now has a host-independent reflection contract.
`SpirvReflection` observations are checked for a valid compute entry point,
exact target digest/workgroup/feature/address-space parity, required Vulkan
builtins, Workgroup storage parity, and the canonical versioned packed-BDA
argument block before any future shader-module creation. BDA-intent modules also
require storage-buffer address-space evidence.

This checkpoint records contract and fixture evidence only. It does not claim
Kernel IR to MLIR conversion, actual SPIR-V emission, `spirv-val`, pipeline
creation, physical queue submission, or driver-family execution.

## Changes

- Added `SpirvReflection` and `validate_spirv_reflection` to the Vulkan target
  contract.
- Added stable positive and negative reflection fixtures plus the BDA storage
  address-space regression.
- Updated the W0133 plan and Vulkan README with the admission boundary and
  remaining compiler/module gates.

## Verification

| Gate | Result |
| --- | --- |
| Target reflection/preflight fixtures | Positive and negative entry/model, digest, builtin, argument-size, Workgroup-storage, BDA-space, and existing preflight paths passed |
| Focused CTest | `metaflux.backend.vulkan-target-preflight` passed |
| Full Vulkan CTest | 88/88 passed |
| Formatting and patch hygiene | `clang-format` and `git diff --check` passed |
| Product identity | `d25617a`; Author and Committer are `Agent Harness (codex)` |
| Agent records | Recorded separately after this checkpoint |

## Boundary

W0133 remains Active. The next slice is target-constrained Kernel IR to MLIR
SPIR-V lowering, actual module emission, exact binary validation, and
differential semantic fixtures. Reflection must remain the first module
admission boundary. Physical Vulkan execution and driver qualification remain
outside the current host's evidence.

## Cleanup

No product or build artifacts were removed; no source snapshot was added.
The session retains only source, tests, plan, and this checkpoint.

## roast

### light roasts

- Reflection target parity -> `plugins/backend/vulkan/runtime/src/target.cpp` (`d25617a`; target CTest)
- Packed-BDA block verification -> `plugins/backend/vulkan/runtime/tests/target_test.cpp` (`d25617a`; positive/negative fixtures)
- BDA storage-space requirement -> `plugins/backend/vulkan/runtime/src/target.cpp` (`d25617a`; module regression)

### medium roasts

- none.

### dark roasts

- none.

## session-only

- Physical module creation, MLIR conversion, binary validation, pipeline submission, and driver qualification - reason: these gates require a future compiler/module producer or physical Vulkan device and are outside this host-independent checkpoint

## Handoff

```sh
nix develop .#vulkan --command ctest --preset vulkan
```

Read the W0133 plan and target-preflight/reflection contract before editing.
