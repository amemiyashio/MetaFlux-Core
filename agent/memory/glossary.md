---
status: Current
updated: 2026-09-10
---

# Glossary

| Term | Working meaning | Canonical context |
| --- | --- | --- |
| Compatibility provider | Application-facing implementation of an existing ecosystem ABI or management API | [plugins](../../plugins/README.md) |
| Execution backend | Compiler/runtime implementation for a concrete compute target behind `mf_backend_api_v1` | [backend contract](../../contracts/plugin/backend/v1/README.md) |
| Client fast path | Small C17 application-side code used by providers for negotiated mappings and steady-state operations | [runtime](../../runtime/README.md) |
| Kernel IR (compute-kernel intermediate representation) | A compute program's parameters, operations, memory accesses, and synchronization, processed by userspace compilers and execution backends | [compiler](../../compiler/README.md) |
| Linux kernel drivers | Operating-system-side device, mapping, queue, and PCI support, with companion models and tests | [Linux drivers](../../linux-kernel-drivers/README.md) |
| Client protocol | Ecosystem-neutral provider/runtime negotiation version and encoded control records | [client protocol](../../contracts/protocol/client/v1/README.md) |
| Backend plugin ABI | Versioned in-process C function table used by daemon/workers to call a backend | [backend contract](../../contracts/plugin/backend/v1/README.md) |
| Logical device | Stable MetaFlux-visible compute identity presented consistently through enabled interfaces | [work-item-0.1.0.2](../plan/milestone-0.1.0.0-core-foundation/work/work-item-0.1.0.2-contracts-runtime.md) |
| Registry view | One process-scoped selection policy and `registry_view_id`, with provider-specific generation-bound membership revisions captured at permitted initialization boundaries | [runtime](../../runtime/README.md) |
| Generation | Monotonic incarnation of a logical device; stale objects retain their old generation and fail deterministically | [milestone-0.1.2.0](../plan/milestone-0.1.2.0-vpci-lifecycle/plan.md) |
| Workflow Epoch | One effective product objective, route, and governance regime; explicit semantic governance advances it | [Agent execution](../../docs/architecture/agent-execution.md) |
| Batch | A bounded collection of dependent work within one workflow Epoch | [Agent execution](../../docs/architecture/agent-execution.md) |
| Iteration | One bounded, reviewed, verified delivery; it may complete a slice of a work item | [Agent execution](../../docs/architecture/agent-execution.md) |
| Device epoch | Persistent monotonic retirement counter advanced by checked addition exactly once per committed retirement; exhaustion never wraps | [work-item-0.1.2.1](../plan/milestone-0.1.2.0-vpci-lifecycle/work/work-item-0.1.2.1-lifecycle-model.md) |
| Lifecycle sequence | View-scoped no-wrap serial published through a FIFO range gate; only the head range writes, unused suffixes retire permanently, and `UINT64_MAX` closes the process mapping | [work-item-0.1.0.2](../plan/milestone-0.1.0.0-core-foundation/work/work-item-0.1.0.2-contracts-runtime.md) |
| Worker lease | Exclusive authority for one data-plane worker to own a backend instance and consume queues for a generation | [control/data plane](../../docs/architecture/control-and-data-plane.md) |
| Control plane | Registry, policy, generation, negotiation, and lifecycle authority outside steady-state submissions | [control/data plane](../../docs/architecture/control-and-data-plane.md) |
| Data plane | Shared queues, completions, mappings, and bulk data path from client/guest to the leased worker | [control/data plane](../../docs/architecture/control-and-data-plane.md) |
| AOT | Compilation completed before first execution and loaded from a compatible artifact/cache entry | [work-item-0.1.0.3](../plan/milestone-0.1.0.0-core-foundation/work/work-item-0.1.0.3-compiler-cpu.md) |
| JIT | Compilation performed on demand after a cache miss; cache-hit execution avoids the compiler service | [work-item-0.1.0.3](../plan/milestone-0.1.0.0-core-foundation/work/work-item-0.1.0.3-compiler-cpu.md) |
| Compiler epoch | Pinned compiler stack and build identity that namespaces generated artifacts | [epoch descriptor](../../toolchains/compiler-epoch-1.json) |
| vPCI | Virtual PCI presentation and transport mechanism; not vendor-private GPU emulation | [milestone-0.1.2.0](../plan/milestone-0.1.2.0-vpci-lifecycle/plan.md) |
| Fixture | Buildable boundary proof used during bootstrap; it does not imply functional ecosystem compatibility | [repository overview](../../README.md) |
