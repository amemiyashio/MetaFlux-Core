# Session Summary

## Objective and outcome

W0131 starts M0130 with a capability-only Vulkan 1.3 compute slice. The
repository now has a fixed C ABI profile, an optional host probe that reports
actual queried features and limits, deterministic target-environment identity,
and a named Nix tool epoch. The work item remains Active: this stage does not
advertise a physical driver family, execution, lowering, memory import, cache,
or lifecycle qualification.

## Durable changes

- `contracts/plugin/backend/v1/include/metaflux/backend/vulkan.h` defines the
  fixed-width capability profile and status ABI; the C layout test protects it.
- `plugins/backend/vulkan/runtime/` provides the C++20 Vulkan probe, target
  serialization/digest, and host regression test behind an opt-in CMake target.
- `toolchains/vulkan-1.json`, `nix/toolchains/vulkan.nix`, and the named
  `.#vulkan` shell pin and expose Vulkan headers, loader, `vulkaninfo`,
  `glslangValidator`, and `spirv-val`.
- `CMakePresets.json`, `plugins/backend/vulkan/README.md`, and the W0131 plan
  document the opt-in build and bounded capability stage.

## Verification

| Command/gate | Result |
| --- | --- |
| Nix Vulkan tool materialization | Passed: `metaflux-vulkan-tools-1.4.341.0` at `/nix/store/85p5k5gdff9rfzzjk0pwx3gz9lblnvcd-metaflux-vulkan-tools-1.4.341.0`; current-timezone mirror routing remains toolchain policy |
| Vulkan configure/build | Passed: `nix develop .#vulkan --command cmake --preset vulkan` and C++20 build |
| Vulkan CTest | Passed: 81/81, including `metaflux.contract.backend-vulkan-abi.v1` and `metaflux.backend.vulkan-capability` |
| Host capability probe | Passed as a truthful local skip: `no-device (not qualified on this host)` because the AMD ICD is not discoverable |
| Repository gates | Passed: `python3 tools/check-agent-records.py .` and `git diff --check` |
| Component graph/schema | Passed in the Vulkan CTest preset; no product ABI or lifecycle boundary was changed |

## Cleanup

- Removed: none; build output remains in the external ignored build directory.
- Retained: Vulkan source, contract, toolchain manifest, plan, and test evidence.

## Decisions and experience

- No decision closure was required. The implementation follows the existing
  M0130 Vulkan boundary and keeps Vulkan handles and C++ types behind the
  stable C backend ABI.
- Capability identity is derived from queried API/features/limits/UUIDs and a
  serialized target digest; a missing host ICD is recorded as `no-device`, not
  as a successful qualification.

## roast

### light roasts

- Vulkan capability profile -> `contracts/plugin/backend/v1/include/metaflux/backend/vulkan.h` (content `837619a`; C ABI layout test)
- Vulkan host probe and target identity -> `plugins/backend/vulkan/runtime/src/capability.cpp` (content `837619a`; capability test)
- Vulkan tool epoch -> `toolchains/vulkan-1.json` (content `837619a`; Nix materialization)

### medium roasts

- none.

### dark roasts

- none.

## session-only

- Current AMD host has no discoverable `libvulkan_radeon.so` - reason: retain
  the local no-device probe context without creating a persistent support claim.

## Unresolved items

- W0131 remains Active. Next actions are to close the exact feature/limit
  baseline and driver-family minimums, then add packed BDA/external-memory
  fixtures before moving to W0132/W0133.

## Handoff

Resume with `nix develop .#vulkan --command ctest --preset vulkan`, then read
the M0130 plan, W0131 plan, `plugins/backend/vulkan/README.md`, and the Vulkan
and runtime-contract skills. Do not infer physical-driver qualification from
the local `no-device` probe.
