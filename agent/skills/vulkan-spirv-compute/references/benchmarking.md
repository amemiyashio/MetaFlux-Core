# Vulkan Path and Throughput Measurement

Choose the requested hypothesis before measuring: dispatch overhead, copy
bandwidth, kernel throughput, synchronization or cold/warm preparation. Hold the
device, enabled target profile, memory tier, workload, shape, dependencies and
completion semantics equal between candidate and direct/reference baseline.
Attribute time to preparation, submission, generated kernel, copies and waits.

Use the pinned physical RADV environment and record device/driver identities,
build/harness identity, clock assumptions, memory tier, input sizes, warmup,
samples and raw distributions. Keep model, software ICD, physical component and
stock-client rows distinct. A direct FMA fixture or faster suite is not PyTorch
throughput. Archive reproducible evidence in its existing owner/output paths.

The [FMA harness](../../../../tests/performance/vulkan_fma_profile.cpp) is useful
for a specific shader-throughput question. Compare independent chain depth per
lane when investigating low instruction-level parallelism; the earlier
RDNA3-class low-ILP versus chain-parallel observation is a workload/device
observation, not a universal tuning order or promised multiplier. Host-visible
staging can make elementwise streaming bandwidth-bound before shader throughput;
measure its traffic before changing arithmetic. Record chain depth and memory
tier with the result.

Anchor device timestamp windows to observed host completion, keep anchors
monotonic and respect timestamp period/valid-bit/wrap semantics. Do not compare
raw device and host clock values as if they shared an origin. Separate queue
latency from device elapsed execution and end-to-end request completion.

For a warm-path claim, trace the same production call after preparation. Prove
the absence of compiler/validator, shader/pipeline creation, Vulkan allocation
and MetaFlux heap allocation; an allocation-free direct component fixture does
not establish that property in `VulkanExecutionRoute::launch`. Retain resource
generations, dependency ordering, completion and loss handling while moving
invariant work to preparation. Validate correctness before accepting timing.

Use distributions and a declared uncertainty/stopping method, preserve outliers
and failures, and apply only the active provisional/binding budget. Current
physical AMD evidence does not promote deferred NVIDIA/dual-driver gates.
