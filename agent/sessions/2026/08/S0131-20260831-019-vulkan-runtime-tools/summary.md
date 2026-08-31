# Session Summary

## Objective and outcome

Advanced M0130/W0131 with an on-demand Vulkan runtime tool profile at content
revisions `836c6e3` and `6c7c3ff`. The new `vulkan-runtime` output pins Mesa's
Vulkan ICD and Khronos validation layers and is exposed through `.#vulkan-runtime`.
The existing lean `.#vulkan` tool environment remains unchanged. The profile
enables local RADV/lavapipe smoke and capability probes without changing product
device selection or qualification semantics.

## Durable changes

- `toolchains/vulkan-runtime-1.json`: Mesa and validation-layer identities.
- `nix/toolchains/vulkan-runtime.nix`: checked version materialization and
  manifest output.
- `nix/default.nix`: `vulkan-runtime` package output.
- `nix/shells/default.nix`: isolated `.#vulkan-runtime` shell with runtime and
  layer search paths.
- `toolchains/README.md` and
  `agent/plan/M0130-vulkan-backend/work/W0131-capability-abi.md`: scope and
  qualification boundary.

## Verification

| Command/gate | Result |
| --- | --- |
| `nix eval --raw .#packages.x86_64-linux.vulkan-runtime.name` and `nix flake show .` | Passed; output and shell are exposed |
| `nix build --no-link --print-out-paths .#vulkan-runtime` | Passed; runtime profile materialized |
| `nix develop .#vulkan-runtime --command vulkaninfo --summary` | Passed; AMD RADV API 1.4.354 and Khronos validation layer visible |
| `nix develop .#vulkan-runtime --command ctest --preset vulkan -R ...` | Passed: focused Vulkan checks 3/3 |
| `nix develop .#vulkan-runtime --command ctest --preset vulkan --output-on-failure` | Passed: 88/88 |
| Nix/JSON/record checks | Passed: `nixfmt --check`, JSON parse, `git diff --check`, Agent records |
| Product identity | `836c6e3`, `6c7c3ff`; Agent Harness (codex) as Author and Committer |

## Cleanup

- Removed: none; no session-owned failed route or temporary artifact was retained.
- Retained: no build, download, source snapshot, or ordinary log artifact; the
  external CMake build directory remains owned by the build workflow.

## Decisions and experience

- No open decision was closed. The runtime profile is a tool provisioning
  boundary; device selection and M0130 qualification remain with Vulkan/CTest.

## roast

### light roasts

- On-demand Vulkan runtime profile -> `toolchains/vulkan-runtime-1.json` (`836c6e3`; Nix materialization and version check)
- Runtime shell search-path wiring -> `nix/shells/default.nix` (`836c6e3`; RADV smoke and full CTest)
- W0131 smoke-only qualification boundary -> `toolchains/README.md` (`836c6e3`; explicit no-NVIDIA/dual-driver claim)

### medium roasts

- none.

### dark roasts

- none.

## session-only

- AMD RADV API 1.4.354 smoke output - reason: local host observation demonstrates runtime availability but is not a portable product or dual-driver qualification artifact

## Unresolved items

- W0131 remains Active; select and qualify the exact two driver families and
  feature/limit baseline before freezing the Vulkan capability matrix.
- W0132-W0135 still require physical allocation, queue execution, lowering,
  pipeline, cache, and device-loss evidence.

## Handoff

Use `nix develop .#vulkan-runtime` for local Vulkan smoke/probe work and set
`VK_ICD_FILENAMES` when a specific ICD is required. Keep product CMake/CTest and
qualification ownership outside Nix; read the W0131 plan and Vulkan skill before
changing capability or execution semantics.
