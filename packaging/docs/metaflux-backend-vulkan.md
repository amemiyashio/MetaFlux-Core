# metaflux-backend-vulkan packaging

Packaging-owned artifact notes for the Vulkan execution backend package.

## Artifact

- Package id: `metaflux-backend-vulkan`
- Payload: Vulkan backend shared library, SPIR-V lowering worker inputs that
  ship with the complete runtime, and frozen backend ABI headers under
  `contracts/plugin/backend/v1/include/metaflux/backend/`
- Sibling qualification gates: `vulkan`, `vulkan-cache`, `vulkan-local-transport`,
  `vulkan-guest-transport` (tests-owned)

## Policy

- Generic artifacts must require no `/nix/store` runtime path
- Does not install vendor CUDA/NVML libraries or overwrite vendor nodes
- May coexist with CPU backend and idle when no Vulkan ICD is present
- External-memory extension freeze remains gated on dual-driver validation and
  packaging soak rows in work-item-0.1.3.6

## Staging posture

Complete release packaging continues to flow through `packaging/build.py` once
the Vulkan backend is enabled in a generic-release build tree. These notes
record package ownership and coexistence rules before those release rows run.
Live install/upgrade/coexistence/uninstall remains host qualification.
