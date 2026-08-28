---
status: Current
updated: 2026-08-28
---

# Glossary

| Term | Working meaning | Canonical context |
| --- | --- | --- |
| Compatibility provider | Application-facing implementation of an existing ecosystem ABI or management API | [plugins](../../plugins/README.md) |
| Execution backend | Compiler/runtime implementation for a concrete compute target behind `mf_backend_api_v1` | [backend contract](../../contracts/plugin/backend/v1/README.md) |
| Client fast path | Small C17 application-side code used by providers for negotiated mappings and steady-state operations | [runtime](../../runtime/README.md) |
| Client protocol | Ecosystem-neutral provider/runtime negotiation version and encoded control records | [client protocol](../../contracts/protocol/client/v1/README.md) |
| Backend plugin ABI | Versioned in-process C function table used by daemon/workers to call a backend | [backend contract](../../contracts/plugin/backend/v1/README.md) |
| Logical device | Stable MetaFlux-visible compute identity presented consistently through enabled interfaces | [M0001-W02](../plan/M0001-core-foundation/work/W02-contracts-runtime.md) |
| Registry view | One process-scoped selection policy and `registry_view_id`, with provider-specific generation-bound membership revisions captured at permitted initialization boundaries | [runtime](../../runtime/README.md) |
| Generation | Monotonic incarnation of a logical device; stale objects retain their old generation and fail deterministically | [M0003](../plan/M0003-vpci-lifecycle/plan.md) |
| Epoch | Persistent monotonic retirement counter advanced by checked addition exactly once per committed retirement; exhaustion never wraps | [M0003-W01](../plan/M0003-vpci-lifecycle/work/W01-lifecycle-model.md) |
| Lifecycle sequence | View-scoped no-wrap serial published through a FIFO range gate; only the head range writes, unused suffixes retire permanently, and `UINT64_MAX` closes the process mapping | [M0001-W02](../plan/M0001-core-foundation/work/W02-contracts-runtime.md) |
| Worker lease | Exclusive authority for one data-plane worker to own a backend instance and consume queues for a generation | [control/data plane](../../docs/architecture/control-and-data-plane.md) |
| Control plane | Registry, policy, generation, negotiation, and lifecycle authority outside steady-state submissions | [control/data plane](../../docs/architecture/control-and-data-plane.md) |
| Data plane | Shared queues, completions, mappings, and bulk data path from client/guest to the leased worker | [control/data plane](../../docs/architecture/control-and-data-plane.md) |
| AOT | Compilation completed before first execution and loaded from a compatible artifact/cache entry | [M0001-W03](../plan/M0001-core-foundation/work/W03-compiler-cpu.md) |
| JIT | Compilation performed on demand after a cache miss; cache-hit execution avoids the compiler service | [M0001-W03](../plan/M0001-core-foundation/work/W03-compiler-cpu.md) |
| Compiler epoch | Pinned compiler stack and build identity that namespaces generated artifacts | [epoch descriptor](../../toolchains/compiler-epoch-1.json) |
| vPCI | Virtual PCI presentation and transport mechanism; not vendor-private GPU emulation | [M0003](../plan/M0003-vpci-lifecycle/plan.md) |
| Fixture | Buildable boundary proof used during bootstrap; it does not imply functional ecosystem compatibility | [repository overview](../../README.md) |
