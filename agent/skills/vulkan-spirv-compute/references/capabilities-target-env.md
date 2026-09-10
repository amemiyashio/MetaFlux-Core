# Capabilities and Target Environment

## Physical-device profile

Record loader/ICD and driver identities, API version, vendor/device IDs, device
and driver UUIDs, pipeline cache UUID, device type, queue families/counts,
subgroup properties, memory heaps/types, required extensions, feature chains,
limits, and external handle capabilities.

Select a compute-capable queue profile and enable only the features consumed by
the backend. Baseline milestone-0.1.3.0 requires Vulkan 1.3 compute, timeline semaphore and
Synchronization2 behavior, plus the chosen buffer-device-address contract.
Required and optional capabilities follow the accepted component profile and
the active work item's exact matrix. Existing component capability evidence
does not close a new framework route. Physical dual-driver expansion remains
owned by milestone-2.0.0.0; qualify the named AMD profile independently.

## Target environment

Serialize Vulkan/SPIR-V version, addressing and memory model, capabilities,
extensions, resource limits, subgroup constraints, storage classes, execution
modes, FP policy, and packed-argument/BDA rules into one canonical digest. Feed
that same object to MLIR conversion, SPIR-V validation, reflection, runtime
compatibility, diagnostics, and cache identity.

Never assume subgroup size 32. Warp-dependent operations and unproved
vote/shuffle/reconvergence fail explicitly. Graphics, images/textures, sparse
resources, device groups, ray tracing, presentation, and excluded PCIe features
remain outside this skill's compute profile.

Primary sources:

- [Vulkan devices and queues](https://docs.vulkan.org/spec/latest/chapters/devsandqueues.html)
- [Vulkan SPIR-V environment](https://docs.vulkan.org/spec/latest/appendices/spirvenv.html)
- [Buffer device address guide](https://docs.vulkan.org/guide/latest/buffer_device_address.html)
