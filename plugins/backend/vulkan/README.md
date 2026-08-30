# Vulkan Backend

The first M0130 stage is a capability-only probe. The C ABI record in
`contracts/plugin/backend/v1/include/metaflux/backend/vulkan.h` contains fixed-width
API, queue, subgroup, memory-tier, UUID, and target-environment fields; Vulkan
handles and C++ objects remain private to the implementation.

`metaflux_vulkan_backend` creates a Vulkan 1.3 instance, selects the first
physical device with a compute queue and the required timeline semaphore,
Synchronization2, and buffer-device-address features, then serializes the
queried profile into a deterministic target environment and SHA-256 digest.
Only a device with both device-local and host-visible heaps advertises the
baseline staging tier. The probe destroys all Vulkan handles before returning.

Build this optional component with the pinned tool shell and an explicit SDK
output:

```sh
vulkan_sdk="$(nix build --no-link --print-out-paths \
  '.#packages.x86_64-linux.vulkan-tools')"
nix develop .#vulkan --command cmake -S . \
  -B ../.metaflux-build/MetaFlux-Core/vulkan \
  -G Ninja -DMETAFLUX_BUILD_VULKAN_BACKEND=ON \
  -DMETAFLUX_VULKAN_SDK_DIR="$vulkan_sdk" \
  -DMETAFLUX_BUILD_TESTS=ON
nix develop .#vulkan --command cmake --build \
  ../.metaflux-build/MetaFlux-Core/vulkan
```

The capability CTest accepts an unavailable or incompatible host as a skipped
local probe. A successful AMD-host probe is provisioning and single-driver
evidence only; it does not close the W0131 dual-driver or M0130 release gates.
No Vulkan execution, SPIR-V lowering, external-memory import, cache, or device
loss behavior is claimed by this stage.

The backend contract also includes a target-digest-bound packed argument block
(`vulkan_arguments.h`) and an external-memory 0.x profile
(`vulkan_memory.h`). The argument validator accepts only known scalar or
generation-bound device-address entries. Tier 3 staging is the baseline; direct
OPAQUE_FD or DMA-BUF import is advertised only when a future device probe proves
the matching handle and synchronization capabilities.
