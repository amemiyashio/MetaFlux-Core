# Prepared Modules and Actual Dispatch

Read `MF_RING_OPCODE_MODULE_LOAD` and `Session::process_launch` in
[server.cpp](../../../../services/metafluxd/src/server.cpp),
`CpuExecutionEngine::prepare` and `PreparedModule::launch` in
[execution.cpp](../../../../services/metafluxd/src/execution.cpp), and the
[Vulkan adapter](../../../../services/metafluxd/src/vulkan_execution.cpp) when
changing that route. Trace one accepted request to its actual executor, then
implement the missing adapter or dispatch behavior without adding tensor math
to the daemon.

The current CPU path enters the generic interpreter or loaded compiled entry.
Do not treat older prose about operation-specific daemon CPU branches as proof
that such branches still exist. The current optional Vulkan path prepares CPU
state first and attaches a Vulkan module opportunistically. A missing dispatchable
Vulkan module still leaves a CPU path. This is implementation evidence of a gap,
not permission to qualify it as a fixed GPU context.

The [CPU work item](../../../plan/milestone-0.2.0.0-pytorch-cuda-compatibility/work/work-item-0.2.0.2-torch-kernel-intake.md)
owns the finite corpus and required modes. The
[Vulkan work item](../../../plan/milestone-0.2.0.0-pytorch-cuda-compatibility/work/work-item-0.2.0.3-framework-qualification.md)
owns the prior route decision, CPU dependency and physical execution gate.
When assigned that route implementation, establish one context selection before
resource success, propagate its backend identity to buffers/modules/queues, and
reject unsupported GPU work without per-kernel substitution. Readiness or
enumeration does not close those prerequisites.

## Warm execution

The service currently constructs argument vectors during `process_launch`.
Compiled launch and the Vulkan adapter also have their own preparation/storage
costs. An executor-only allocation audit cannot prove the service path is free
of allocation. Remove or prepare the particular repeated work with explicit
concurrency and lifetime ownership; do not simply share a mutable argument
vector across requests. Keep diagnostics opt-in and avoid production-path
hashing or tracing introduced solely to demonstrate an optimization.

Interpreter, cold JIT, warm JIT and AOT retain different module preparation
semantics. Warm JIT/AOT misses do not launch a compiler. Persistent key/storage
changes use the cache expert; process failures use the worker expert. Keep
CPU helper identity and target environment binding at preparation/loading.

Use [execution-mode tests](../../../../services/metafluxd/tests/execution_mode_test.cpp)
for CPU configuration, preparation and execution modes. Use
[service client integration](../../../../services/metafluxd/tests/client.c)
and the affected stock-client case for service routing and real request
completion. GPU qualification additionally needs the named physical AMD device,
GPU submission, completion and readback. Compare end-to-end performance on the
same client workload and route, separating cold preparation, warm dispatch,
execution, copies and synchronization. Structural removal of work and measured
latency are separate results.
