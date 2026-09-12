---
name: runtime-contracts-registry
description: Implement or review neutral client/kernel requests, registry views, shared layouts, schema projections and backend C ABI. Own cross-consumer contracts and lifetime; leave ecosystem API meaning, target execution and transport mechanics with their experts.
---

# Runtime Contracts and Registry

Start from one requested producer-to-consumer transition. Name the crossing
boundary and find its canonical definition under [contracts](../../../contracts/README.md),
the [client fast path](../../../runtime/client/fastpath/src/fastpath.c), and
the [daemon consumer](../../../services/metafluxd/src/server.cpp). Read the
affected work item's Exit Gate, including work-item-0.2.0.2 for framework-kernel
requests. Reuse an unchanged contract when missing behavior belongs only in a
consumer; implement canonical fields, generated projections and affected
consumers together when the crossing itself changes.

Select the complete topic needed for the transition. For implementation use
the shared [implementation guidance](../review/references/implementation-guidance.md);
explicit analysis or review-only requests stay read-only.

| Task | Read |
| --- | --- |
| Select a contract zone, change schema or generated consumers | [Contract zones and schema](references/contract-zones-and-schema.md) |
| Register a neutral kernel request and retain its module/source | [Kernel-request lifetime](references/kernel-request-lifetime.md) |
| Device identity, provider join/freeze, enumeration or fresh mapping | [Registry views](references/registry-views.md) |
| Registry record fields, bounded storage, tags or reclamation | [Registry records](references/registry-records.md) |
| Handle read brackets, stateful admission, policy update or loss races | [Registry admission](references/registry-admission.md) |
| Lifecycle ranges, FIFO publication, recovery or terminal close | [Registry publication](references/registry-publication.md) |
| Shared telemetry banks, reader freshness or publisher death | [Registry telemetry](references/registry-telemetry.md) |
| Queue layout, atomics, negotiation, wakeups or backend C ABI | [Shared fast path and ABI](references/shared-fast-path-and-abi.md) |

Essential invariants:

- One canonical owner defines each contract and its generated consumers.
  Encoded fields stay fixed-width and ecosystem-neutral; do not copy schemas
  privately or place CUDA/NVML/PTX/compiler/backend types on neutral wires.
- Keep client-protocol, Kernel IR and backend-ABI versions distinct. Specify
  capabilities, extension/reserved-field behavior, ownership and lifetime before
  changing a producer or consumer.
- One negotiated process view owns shared mutable state. Provider membership
  snapshots may differ by initialization revision; live parity needs the common
  `(UUID, generation)`, not ordinal or BDF alone.
- Liveness and policy come from stable lifecycle/admission controls. Telemetry
  never reopens a lost device. Stateful admission requires its shared lease
  commit; read-side validation alone grants no side effect.
- Identity, generations and tagged counters never wrap or alias. Preserve
  reserved terminal capacity, exact recovery, owner-death versus live-deadline
  handling and quarantine before physical storage reuse.
- Keep C17 client paths free of C++/LLVM/backend objects. Account for actual
  copies, registration, locking and dispatch when changing the warm path.

Session-local execution and dispatch use
[$daemon-execution-runtime](../daemon-execution-runtime/SKILL.md) skill; an unchanged
neutral contract does not need a schema rewrite for a consumer implementation.
Compose [$cuda-driver-abi-compatibility](../cuda-driver-abi-compatibility/SKILL.md) skill
or [$nvml-telemetry-compatibility](../nvml-telemetry-compatibility/SKILL.md) skill for
API mapping, [$device-lifecycle-resilience](../device-lifecycle-resilience/SKILL.md) skill
for replacement authority, and the transport/backend expert for its mechanics.
PTX source meaning belongs to [$ptx-simt-semantics](../ptx-simt-semantics/SKILL.md) skill.

Return the contract/consumer delta, lifetime or state transition, generated
outputs and selected actual checks. Exercise the affected scenario's layout,
negotiation, race or execution evidence; keep missing harnesses explicit.
Detailed scenarios are conditional on the changed mechanism, not a reason to
repeat every registry stress test before the first implementation edit.
