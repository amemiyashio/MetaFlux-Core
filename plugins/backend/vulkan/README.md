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

W0133 also defines a host-independent `SpirvReflection` contract for the
post-conversion boundary. Its verifier requires a compute entry point, exact
target digest, workgroup and feature/address-space parity with preflight,
`LocalInvocationId` coverage, Workgroup storage parity, and a packed argument
block whose size follows the versioned 64-byte header plus 48-byte entry layout.
Malformed or mismatched observations are rejected before shader-module creation.
The contract is ready for a future MLIR/SPIR-V reflection producer; it does not
claim module emission or `spirv-val` execution.

The runtime also contains a host-independent stream graph planner. It assigns
monotonic timeline values, preserves same-stream FIFO, requires cross-stream
waits to be explicit, and validates transfer/compute stage-access masks before
future `vkQueueSubmit2` submission. It does not claim queue submission,
pipeline execution, or device timing.

W0132 also includes a host-independent memory visibility ledger for the staging
baseline. It binds each allocation to a generation and submission timeline,
tracks host/device dirty ranges, requires explicit flush before non-coherent
device access and invalidate before host access, rounds ranges to the queried
non-coherent atom size, and rejects in-flight or stale teardown. The runtime now
adds a source-local host-visible `VkBuffer`/`VkDeviceMemory` staging adapter. It
selects a compatible host-visible memory type, prefers host-coherent memory,
maps the allocation, and normalizes non-coherent flush/invalidate ranges. This
is physical allocation and mapping evidence on the current AMD/RADV host only;
the source-local runtime now also records an explicit host-to-device and
device-to-host copy through a device-local buffer and the generation-bound
timeline context. This is a single-device Tier 3 round-trip, not external-handle
import, backend admission, device-loss drain, or driver qualification.

W0134 also provides a host-independent command-resource pool. A finite pool
assigns each acquired resource a monotonic identity, generation, stream, and
sequence. Submission records a strictly increasing completion timeline; a
resource remains in flight until an observed completion reaches that value.
Recycling never releases an incomplete resource, and generation reconfiguration
is rejected while any resource is acquired or submitted. Handles from a retired
generation are stale and are not reused by identity. The pool is an ownership
and admission model only: Vulkan command buffers, queue submission, pipeline
creation, and driver timing still belong to the later integration stage.

The `QueueSubmissionLedger` composes that pool with the stream graph behind one
mutex-protected admission boundary. It acquires a finite resource before graph
validation, cancels the resource when validation rejects the plan, assigns a
monotonic generation-bound completion value only after both admissions succeed,
and recycles only through an observed completion. Reconfiguration resets the
graph and resource generation together and rejects in-flight work. The ledger
returns a complete host-independent plan/resource/completion tuple; it does not
create Vulkan objects or claim `vkQueueSubmit2` execution.

W0132 now adds a private `VulkanDeviceContext` that binds a successful capability
profile to a new Vulkan 1.3 instance, an exact physical-device identity, one
compute queue, and a timeline semaphore. It rechecks the profile's API/driver
versions, UUID, queue family, workgroup limits, memory totals, and required
timeline/Synchronization2/buffer-device-address features before enabling the
logical device. Empty `vkQueueSubmit2` timeline signals, bounded waits, and
counter polling are generation-checked and map device loss or timeout to stable
runtime statuses. Vulkan handles remain source-local C++ state and never cross
the stable backend C ABI. The optional `.#vulkan-runtime` shell can exercise
this boundary on AMD RADV; that smoke is provisioning/single-driver evidence,
not dual-driver, physical NVIDIA, allocation, or release qualification.

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
after release. A resident device-bound hit can now be acquired as one
generation-scoped pipeline binding; duplicate generations report `pinned`,
stale generations report `stale_generation`, and release is required before
device invalidation can remove the entry. This is a lifecycle contract for the
cache repository, not creation or qualification of a `VkPipeline`. The cache
tests mutate every portable and device-bound identity field and require a miss
for each changed key. A host-independent warm-launch trace validator accepts
only cache lookup, pipeline binding, argument binding, and submit in order; it
rejects compiler, validator, module/pipeline creation, and allocation events.
An actual ICD trace is still required for the warm-launch gate.
