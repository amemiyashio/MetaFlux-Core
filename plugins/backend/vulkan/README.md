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

Before a future SPIR-V module is created, the runtime target preflight checks
the queried profile, required feature bits, target digest, workgroup limits,
address-space flags, and subgroup assumptions. It produces stable diagnostics
for mismatches and unsupported semantics; it does not yet perform MLIR
conversion or SPIR-V validation.

The runtime also contains a host-independent stream graph planner. It assigns
monotonic timeline values, preserves same-stream FIFO, requires cross-stream
waits to be explicit, and validates transfer/compute stage-access masks before
future `vkQueueSubmit2` submission. It does not claim queue submission,
pipeline execution, or device timing.

W0134 also provides a host-independent command-resource pool. A finite pool
assigns each acquired resource a monotonic identity, generation, stream, and
sequence. Submission records a strictly increasing completion timeline; a
resource remains in flight until an observed completion reaches that value.
Recycling never releases an incomplete resource, and generation reconfiguration
is rejected while any resource is acquired or submitted. Handles from a retired
generation are stale and are not reused by identity. The pool is an ownership
and admission model only: Vulkan command buffers, queue submission, pipeline
creation, and driver timing still belong to the later integration stage.

The cache model defines deterministic portable and device-bound identities from
Kernel IR, compiler/lowering/tool epochs, target and specialization digests,
argument/backend ABI, and physical device/driver UUIDs. Its bounded catalog
removes corrupt unpinned entries for rebuild and protects live references from
eviction. `CacheFileStore` persists either key class with a complete envelope,
payload digest, process-unique temporary file, `fsync`, and atomic rename;
truncated or mismatched entries are removed before returning `corrupt`, and a
device-bound key can be explicitly invalidated. Cross-process stampede control,
opaque `VkPipelineCache` data, and warm-launch tracing remain open.

`PersistentCacheRepository` is the current host-independent integration
boundary. It admits a publish against the resident catalog before writing the
durable envelope, so pinned entries and a fully pinned quota never overwrite
their existing file. Reads check the resident entry first; a validated file hit
hydrates the bounded catalog and participates in its LRU policy. Pin/unpin and
device invalidation are serialized with these transitions. Cross-process
single-key misses use `lookup_or_publish`: a stable per-key advisory lock is
held across the second lookup, producer, and publication, and waiters recheck
after release. Pipeline-bound device/driver invalidation still requires the
later Vulkan pipeline stage.
