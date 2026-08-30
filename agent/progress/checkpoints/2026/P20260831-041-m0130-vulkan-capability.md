---
id: P20260831-041
status: Recorded
captured: 2026-08-31
milestone: M0130
workstream: W0131
branch: main
git_revision: 837619ad4c4e3c24224d6d9d3079d9a87c2bdaf2
workspace: Vulkan capability ABI, opt-in tool epoch, and truthful host probe are implemented; lowering, execution, memory, cache, and driver-family qualification remain open
---

# M0130 W0131 Vulkan Capability ABI

## Outcome

The first M0130 stage is recorded at content revision `837619a`. MetaFlux now
has a fixed-width Vulkan capability C ABI, an optional C++20 probe for the
Vulkan 1.3 compute baseline, deterministic target-environment serialization and
SHA-256 identity, and a named Nix `vulkan-tools` epoch with CMake integration.
The probe reports actual queried features, queues, limits, memory tiers, and
UUIDs without crossing Vulkan handles or C++ types through the backend ABI.

This is a capability-only stage. It does not claim a physical driver family,
dual-driver qualification, packed BDA execution, SPIR-V lowering, external
memory import, pipeline cache behavior, or lifecycle/device-loss integration.

## Verification evidence

| Gate | Result |
|---|---|
| Nix tool materialization | Passed: `metaflux-vulkan-tools-1.4.341.0` at `/nix/store/85p5k5gdff9rfzzjk0pwx3gz9lblnvcd-metaflux-vulkan-tools-1.4.341.0` |
| Vulkan configure/build | Passed: `nix develop .#vulkan --command cmake --preset vulkan` and C++20 build |
| Vulkan CTest | Passed: 81/81, including the C ABI layout and host capability tests |
| Host probe | Passed as `no-device (not qualified on this host)`; current AMD host has no discoverable `libvulkan_radeon.so` |
| Agent records and diff | Passed: `python3 tools/check-agent-records.py .`; `git diff --check` |
| Commit identity | Passed: `Agent Harness (codex)` as Author and Committer for `837619a` |

## Boundary

W0131 and M0130 remain Active. Exact feature/limit minimums and two independent
Vulkan driver families are still undecided. The next bounded work adds packed
BDA and external-memory fixtures or advances W0132 device-memory bring-up while
preserving the M0110/M0120 ownership boundaries.

## Cleanup

- Removed: none; the external CMake build directory remains ignored and owned
  by the build workflow.
- Retained: Vulkan source, C ABI, Nix manifest, CMake presets, documentation,
  plan update, and this checkpoint.

## roast

### light roasts

- Vulkan capability ABI and target identity -> `contracts/plugin/backend/v1/include/metaflux/backend/vulkan.h` and `plugins/backend/vulkan/runtime/src/capability.cpp` (content `837619a`; ABI and capability tests)
- Vulkan tool epoch and named shell -> `toolchains/vulkan-1.json`, `nix/toolchains/vulkan.nix`, and `nix/shells/default.nix` (Nix materialization)
- Opt-in backend boundary -> `plugins/backend/vulkan/README.md`, CMake presets, and W0131 plan (81/81 Vulkan CTest)

### medium roasts

- none.

### dark roasts

- none.

## session-only

- The current AMD host lacks a discoverable Vulkan ICD; local capability probe
  output is retained only as a no-device qualification note, not as a support
  or driver-family claim.

## Handoff

Resume S0131 from this checkpoint and the W0131 plan. Run
`nix develop .#vulkan --command ctest --preset vulkan`, then read the Vulkan,
runtime-contracts, and target-backend skills before selecting W0132/W0133 work.
