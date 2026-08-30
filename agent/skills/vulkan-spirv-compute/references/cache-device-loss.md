# Cache and Device Loss

## Cache layers

Portable entries store canonical SPIR-V, reflection, and argument metadata keyed
by Kernel IR/schema, compiler/lowering epochs, tool/pass versions, exact target
digest, FP/argument/specialization state, backend/helper ABI, and content.

Device-bound pipeline entries add vendor/device IDs, device and driver UUIDs and
versions, enabled feature/limit digest, pipeline cache UUID, and relevant pipeline
state. Publish atomically with integrity metadata; hold references while in use;
never evict a live pipeline; remove corrupt/incompatible entries and rebuild.

Warm launch loads a compatible resident/prebuilt pipeline and allocates no
MetaFlux-owned heap object. It invokes no MLIR, SPIR-V compiler, validator,
shader-module creation, pipeline creation, or Vulkan allocation. A changed driver,
device, target, argument ABI, pipeline UUID, compiler epoch, or pass pipeline
must miss.

## Device loss

On `VK_ERROR_DEVICE_LOST` or an equivalent fatal worker condition:

1. atomically stop new backend admission and snapshot the cause;
2. revoke the worker lease and prevent old completions from publishing;
3. mark all BDA, memory, queue, command, semaphore, pipeline, and cache-residency
   objects old-generation and reject new references;
4. notify the M0120 lifecycle authority, which publishes public `LOST` by its
   deadline;
5. isolate non-cancellable driver work behind tombstones and tear down the worker
   process/resources without reusing the Vulkan device/context;
6. recover only through a new lifecycle generation and fresh capability profile.

Inject loss during import, compile, pipeline creation, submit, copy, timeline
wait, cache publication/load, and teardown. Verify no stale completion, address,
pipeline, or cache residency crosses into the replacement generation.
