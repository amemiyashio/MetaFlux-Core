---
name: daemon-execution-runtime
description: Implement or review metafluxd session execution, object retention, backend selection, dispatch, completion and teardown. Use for the real client-to-backend service path; neutral schemas, kernel mathematics and global device generations have separate owners.
---

# Daemon Execution Runtime

Start at the requested control/ring opcode and its handler in
[server.cpp](../../../services/metafluxd/src/server.cpp). Trace the session's
object admission, prepared module, arguments, selected executor and completion.
For implementation, change the first missing service behavior and its unwind
path together. An explanation or benchmark request preserves that scope.

The service owns resource and dispatch mechanics, not tensor algorithms.
Reuse existing contracts and backend APIs; source and actual execution outrank
older descriptions of daemon shortcuts. Read the assigned work item's Exit
Gate before selecting the affected checks.

| Task | Read |
| --- | --- |
| Object creation, release, quotas, arguments, cancellation or drain | [Session lifetime](references/session-lifetime.md) |
| CPU/Vulkan selection, module preparation, completion or warm dispatch | [Execution dispatch](references/execution-dispatch.md) |

Preserve these boundaries:

- Peer identity comes from kernel credentials. Admission reserves resources
  before success; failure and teardown return each reservation exactly once.
- Keep object ID, type, generation, context, access and bounds checks through
  completion. A drain deadline does not permit reclaiming a still-running
  compiled entry's storage.
- Control traffic materializes resources; shared rings carry repeated data-plane
  operations. Avoid new warm allocation, compilation, registration, global
  locking or extra dispatch; prove the specific removed work on the actual path.
- Backend selection belongs to a context before resource success. The optional
  Vulkan adapter is not qualified fixed-backend routing. Follow the active
  dependency and route-decision prerequisites before implementing that route.
- Publish success only after the selected backend has completed the request.
  Preserve typed failures and correlate request/module/executor evidence.
  A matching tensor, module load or compiler counter alone proves less.

Compose [$runtime-contracts-registry](../runtime-contracts-registry/SKILL.md) skill
only when neutral bytes, registry or shared contracts change; compose
[$device-lifecycle-resilience](../device-lifecycle-resilience/SKILL.md) skill for
global generation replacement. Use [$cpu-backend-performance](../cpu-backend-performance/SKILL.md) skill
or [$vulkan-spirv-compute](../vulkan-spirv-compute/SKILL.md) skill for target execution,
[$compiler-worker-isolation](../compiler-worker-isolation/SKILL.md) skill for compiler
processes, and [$compiler-artifact-cache](../compiler-artifact-cache/SKILL.md) skill
for persistent storage. Client profile admission belongs to
[$pytorch-cuda-profile](../pytorch-cuda-profile/SKILL.md) skill.

Return the implemented transition, retained/released resources, error behavior
and actual completion evidence to [$review](../review/SKILL.md) skill. Select the
affected service test and client route once for formal evaluation; do not repeat
unrelated fault suites while the implementation remains missing.
