# Vulkan Backend

The backend implements a bounded compute target behind the neutral backend C
ABI. Its capability, compiler, memory, queue and cache components have model
tests and direct physical AMD/RADV evidence from
[milestone-0.1.3.0](../../../agent/plan/milestone-0.1.3.0-vulkan-backend/plan.md).
Those completed component results are distinct from the unqualified stock
PyTorch daemon route owned by
[work-item-0.2.0.3](../../../agent/plan/milestone-0.2.0.0-pytorch-cuda-compatibility/work/work-item-0.2.0.3-framework-qualification.md).

## Build and activation

Use the pinned tools; the Vulkan preset exports the SDK through the Nix shell
and builds backend components:

```sh
nix develop .#vulkan --command cmake --preset vulkan
nix develop .#vulkan --command cmake --build --preset vulkan
nix develop .#vulkan --command ctest --preset vulkan
```

The preset does not enable daemon GPU execution. The
[daemon adapter documentation](../../../services/metafluxd/README.md#optional-vulkan-adapter)
owns its separate build/runtime opt-ins and current fallback behavior.
Current adapter behavior has not satisfied the fixed-context backend contract;
it is not a supported policy for substituting CPU results in a Vulkan context.

For physical AMD access use the pinned RADV ICD in the
[`vulkan-runtime` shell](../../../toolchains/README.md), not a host `/usr`
loader mixed with Nix libraries. Model tests and a Mesa software ICD remain
useful separate rows. The capability test may print unavailable/unqualified
and return zero; inspect its actual result. A green generic CTest status alone
does not prove that a physical GPU was exercised.

## Component boundaries

| Boundary | Current implementation | Evidence limit |
| --- | --- | --- |
| Capability/target | Vulkan 1.3 compute queue selection, queried features/limits, UUIDs and deterministic target digest | Physical querying is distinct from logical-device enablement and client execution |
| Kernel IR to SPIR-V | Target preflight, bounded MLIR conversion, emission and reflection | Only accepted forms in compiler tests; not the entire PyTorch CPU corpus |
| Memory | Generation-bound host-visible staging and device-local copy, normalized flush/invalidate | Direct import requires its exact device/handle/synchronization matrix |
| Execution | Device context, compute pipeline, queue submission and timeline completion | Direct component fixtures do not prove stock PyTorch routing |
| Stream/resource models | Neutral FIFO/dependency graph, finite command pool and completion ledger | CUDA default/PTDS translation belongs to provider/runtime qualification |
| Cache | Portable/device keys, integrity envelopes, atomic persistence, pinning, eviction and single-key locking | The qualified component warm trace is not automatically a daemon adapter warm trace |
| Lifecycle | Generation validation, stale-resource rejection and model fault tests | Physical driver-change/loss soak and framework lifecycle remain separately owned |

The C ABI records under `contracts/plugin/backend/v1/` carry fixed-width
capability and target data; Vulkan handles and C++ objects remain private.
`VulkanDeviceContext` rechecks the device identity, properties, limits and
required timeline/Synchronization2/buffer-device-address features before
enabling the logical device.

The packed argument ABI in `vulkan_arguments.h` uses a 64-byte header and
48-byte entries, with target digest, scalar/device-address kinds and generation
checks. Reflection, emitted SPIR-V and runtime bindings must agree before a
shader module is created. The bounded compiler's generated fixtures pass
`spirv-val --target-env vulkan1.3`; unsupported forms fail before emission.
An opcode name or model mapping alone does not prove binary emission.

The external-memory profile in `vulkan_memory.h` remains ABI 0.x. Tier 3
staging is the baseline. OPAQUE_FD, DMA-BUF and external host memory are separate
capability-gated tiers; a memfd or host pointer is not automatically zero-copy.
Model FD/ownership tests do not establish physical import or cross-process
semaphore behavior.

`QueueSubmissionLedger` combines the graph and finite command-resource pool;
the physical executor submits through `vkQueueSubmit2` and recycles resources
only after observed completion. The cache repository binds canonical Kernel IR,
compiler/pipeline/target and argument ABI identities; device entries additionally
bind device/driver identities and pipeline cache UUID. A live pin prevents
eviction, and atomic file publication plus per-key locking prevents incomplete
or duplicate cache publication.

The component pipeline test loads a device-cache payload, reuses its resident
pipeline, submits and observes completion while checking the warm trace and
host allocations. FMA measurements use a directly compiled shader fixture;
their throughput is not PyTorch model performance.

## Qualification ownership

- The completed v0.1.3 work items own the model matrix and named single-device
  component results. Preserve the exact source, invocation and environment
  limitations of those results.
- The PyTorch Vulkan work item owns fixed-backend daemon integration, actual
  physical AMD submit/completion, the shared CPU corpus, stream/event/allocator
  behavior, daemon loss and process-level activation.
- [work-item-2.0.0.3](../../../agent/plan/milestone-2.0.0.0-physical-hardware-qualification/work/work-item-2.0.0.3-dual-driver-physical-qualification.md)
  owns the physical AMD+NVIDIA matrix, driver-change/validation soak and
  external-memory promotion. It does not block the first AMD PyTorch slice.
