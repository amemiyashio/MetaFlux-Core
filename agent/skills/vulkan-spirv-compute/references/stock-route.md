# Stock Client to Physical AMD Completion

Read the current
[Vulkan work item](../../../plan/milestone-0.2.0.0-pytorch-cuda-compatibility/work/work-item-0.2.0.3-framework-qualification.md)
and [daemon adapter boundary](../../../../services/metafluxd/README.md#optional-vulkan-adapter).
Its fixed-backend route decision closes before qualifying implementation. The
optional adapter and completed component evidence do not supply that decision
or reorder the active CPU-then-AMD Goal.

## Follow the actual admitted path

1. Trace context selection and visible resource success in provider/runtime;
   the context must commit to CPU or Vulkan before resources succeed.
2. Follow neutral artifact/request intake and `MF_RING_OPCODE_MODULE_LOAD` in
   [server.cpp](../../../../services/metafluxd/src/server.cpp). Current module
   intake prepares CPU execution and optionally adds a Vulkan module. Select the
   missing fixed-context ownership/lifetime change explicitly instead of treating
   that optional module as an already-qualified route.
3. Follow `Session::process_launch`: a dispatchable Vulkan module reaches the
   Vulkan route; absence currently reaches the prepared CPU module. This fallback
   is a concrete path to replace under the approved route contract. A failed
   Vulkan launch reports device loss, which is a different branch from absence.
4. Follow `VulkanExecutionRoute::prepare/launch` in
   [vulkan_execution.cpp](../../../../services/metafluxd/src/vulkan_execution.cpp)
   through lowering, binding, queue submission, observed completion and readback.
   Keep copied scalar values, buffer roles and actual launch dimensions valid.

The first qualifying unit is unchanged stock eager add, copy and readback on the
named physical AMD/RADV device. Correlate the client request/module/context with
device identity, GPU submission and completion before returning success. A CPU
result or successful module compilation cannot prove where execution occurred.
Use the pinned `vulkan-runtime` RADV loader/ICD environment; model/software ICD
and physical component tests remain separate evidence.

## Implement the missing owner

Use `lower_kernel` and `emit_actual_spirv` in the
[compiler](../../../../plugins/backend/vulkan/compiler/src/lowering.cpp) for a
missing emitted semantic family. Use the device/pipeline/queue runtime for an
enabled-feature, command or completion defect. Compose
[$mlir-compiler-engineering](../../mlir-compiler-engineering/SKILL.md) skill for conversion
mechanics; do not add a CPU/LLVM detour or operation-specific CPU shortcut.

The backend consumes neutral Graph IR FIFO/dependency/copy/event edges. Compose
[$cuda-driver-abi-compatibility](../../cuda-driver-abi-compatibility/SKILL.md) skill and
[$runtime-contracts-registry](../../runtime-contracts-registry/SKILL.md) skill for CUDA
legacy-default/PTDS translation; Vulkan never interprets those modes. Compose
the matching transport skill for cdev/vfio-user import mechanics and
[$device-lifecycle-resilience](../../device-lifecycle-resilience/SKILL.md) skill for the
cross-transport generation authority.

## Qualify the same path

Check success and unsupported/missing-module behavior, fixed context selection,
readback, stream/event dependencies, allocator/multi-stream lifetime, teardown
and daemon loss for the slice the Exit Gate requires. Test memfd/cdev/guest
paths only where that milestone requires them. Full corpus qualification uses
the same canonical Kernel IR corpus as CPU; a new hand-written shader fixture
is a component test, not stock-client acceptance.

Reuse prepared modules, descriptors, command resources and buffers on warm
launch. The current adapter still allocates descriptors per launch and grows
staging on demand; inspect actual lifetimes before claiming its warm path meets
the allocation-free contract. Use [cache/loss](cache-device-loss.md) for resource
reuse and [benchmarking](benchmarking.md) for measured same-path improvement.
Report the implementation and observed device behavior, with explicit remaining
scope; neither a skipped probe nor generic CTest exit zero closes this gate.
