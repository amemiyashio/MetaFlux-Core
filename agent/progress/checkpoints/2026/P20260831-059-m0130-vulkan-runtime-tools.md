---
id: P20260831-059
status: Recorded
captured: 2026-08-31
milestone: M0130
workstream: W0131
branch: main
git_revision: 6c7c3ff
workspace: on-demand Vulkan runtime tool profile for host smoke and capability probes
---

# M0130 W0131 Vulkan Runtime-Tool Checkpoint

## Outcome

W0131 now has a separate on-demand `vulkan-runtime` tool profile. The profile
pins Mesa 26.1.8 and Vulkan validation layers 1.4.341.0, exposes them through
`.#vulkan-runtime`, and leaves the lean `.#vulkan` headers/loader/tools shell
unchanged. The shell supplies runtime library, ICD, and layer search paths while
leaving device selection to the Vulkan loader and caller.

This profile enables local RADV/lavapipe smoke and capability probing. It does
not define product device policy and does not satisfy the physical NVIDIA,
two-driver-family, performance, or release gates.

## Verification evidence

| Gate | Result |
|---|---|
| Manifest and Nix version assertions | Passed: Mesa 26.1.8 and validation layers 1.4.341.0 |
| Runtime package materialization | Passed: `nix build --no-link --print-out-paths .#vulkan-runtime` |
| Loader/ICD/layer smoke | Passed: `vulkaninfo --summary` reports AMD RADV API 1.4.354 and Khronos validation layer |
| Vulkan focused CTest | Passed: capability, target-preflight, and stream-graph 3/3 |
| Full Vulkan CTest | Passed: 88/88 |
| Formatting/record checks | Passed: `nixfmt --check`, JSON parse, `git diff --check`, and Agent records |
| Content identity | Passed: `836c6e3` and `6c7c3ff`, Agent Harness (codex) |
| Agent records | Pending the separate record commit for this checkpoint |

## Boundary

W0131 remains Active. The exact feature/limit baseline and two independent
driver-family matrix remain open. The runtime profile is provisioning and local
smoke support only; CMake, CTest, and qualification retain their existing
ownership.

## Cleanup

- Removed: none; temporary Nix downloads are store-owned and no project copy was retained.
- Retained: toolchain manifests, Nix materialization, and compact records; no
  build tree, source snapshot, or ordinary log artifact was copied into Agent records.

## roast

### light roasts

- Vulkan runtime tool identity -> `toolchains/vulkan-runtime-1.json` (`836c6e3`; Nix version assertion)
- On-demand runtime shell -> `nix/shells/default.nix` (`836c6e3`; loader/ICD/layer smoke)
- Host-smoke qualification boundary -> `toolchains/README.md` (`836c6e3`; explicit NVIDIA/dual-driver exclusion)

### medium roasts

- none.

### dark roasts

- none.

## session-only

- AMD RADV probe output - reason: local host observation confirms the runtime profile resolves an ICD but is not portable or release qualification evidence

## Handoff

Use `nix develop .#vulkan-runtime` for local Vulkan smoke/probe work; select a
specific ICD with `VK_ICD_FILENAMES` when needed. Preserve the lean `.#vulkan`
tool environment and read W0131 plus the Vulkan skill before changing capability
or execution semantics.
