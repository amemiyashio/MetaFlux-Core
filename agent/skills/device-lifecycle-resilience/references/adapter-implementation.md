# Implement One Lifecycle Transition

Select one event and the first missing owning behavior. The
[canonical state machine](state-machine.md) supplies identity and commit rules;
the mandatory [model command](model-checking.md) precedes adapter implementation.
Reuse established decisions instead of drafting a second transition model.

| Boundary | Source to inspect (repository-relative) |
| --- | --- |
| Normalize external events | `runtime/core/src/lifecycle_normalizer.cpp` |
| Dispatch to the sole coordinator | `runtime/core/src/lifecycle_dispatch.cpp` |
| Acceptance, staged owners and identity commit | `runtime/core/src/lifecycle.cpp` |
| cdev mirror/worker | `transports/cdev/worker/src/worker.cpp` |
| vfio-user and QMP mirrors | `transports/vfio-user/server/src/server.cpp`, `qmp_lifecycle.cpp` |
| vroot config/binding | `linux-kernel-drivers/vroot/metaflux_vroot_main.c` |
| Vulkan loss adapter | `services/metafluxd/src/vulkan_execution.cpp`, `plugins/backend/vulkan/` |
| Authority/adapter evidence | `runtime/core/tests/lifecycle*.cpp`, affected adapter tests and `tests/lifecycle/registration.cmake` |

Confirm the actual caller before editing. A fixture event and production
event producer are different evidence; implement missing producer wiring when
that is the assignment.

## Carry The Change Through Its Owners

1. State event, source, operation, request ID, UUID, expected generation,
   daemon incarnation and deadline. Use the model's guards and terminal outcome.
2. Trace acceptance, off-registry staging, admission stop, drain/isolation and
   the atomic identity transaction. Implement the missing guard, side effect
   or adapter callback plus its failure unwind. Capacity rejection precedes
   side effects; accepted generation candidates stay consumed on later failure.
3. Treat `PRESENT`, `QUIESCING`, `DRAINING` and `RESETTING` as progress states,
   not provider-enumerable identity. Reset/recover commit old retirement,
   epoch advancement and candidate installation together. Adapters mirror it.
4. Preserve old-generation fd/VMA/DMA/queue/event/module/pipeline/handle
   tombstones; a late completion never resolves through a reused slot or UUID.
5. Deliver existing loss/addition events to the runtime process view. If
   membership, ordering or freeze changes, compose
   [$runtime-contracts-registry](../../runtime-contracts-registry/SKILL.md) skill
   and read [view races](view-races.md). Do not duplicate its authority.
6. Select a deterministic failure point that exercises the changed boundary
   and implement its guard/unwind before repeating the check. Then parent
   review selects the required adapter/integration qualification.

Return the transition's actual before/after behavior, model input identity,
adapter result, timeout/stale behavior and any missing real producer. Model
success, optional adapter code and external completion are separate facts.
