# Session Summary

## Objective and outcome

Added and converged one runtime-owned immediate producer ingress. It snapshots
the Coordinator tuple once and routes admin reset, VFIO-user reset, disconnect,
and daemon restart through the existing normalizer and authority. Delayed QMP
events retain their pre-captured route; malformed, misrouted, and stale events
do not rebind to replacement identity.

## Durable changes

- `runtime/core/include/metaflux/runtime/lifecycle_dispatch.hpp` and
  `runtime/core/src/lifecycle_dispatch.cpp`: restricted capture-and-submit
  helper composed with the canonical normalized ingress.
- `runtime/core/tests/lifecycle_dispatch.cpp`: supported producer, malformed,
  misrouted QMP, unknown-kind, and stale-observation regressions.
- `runtime/core/README.md`: canonical immediate versus delayed producer route.
- `agent/progress/checkpoints/2026/P20260831-089-m0120-producer-ingress.md`:
  material W0122 handoff at content revision `6152efa`.

## Verification

| Command/gate | Result |
| --- | --- |
| Focused lifecycle dispatch | Passed 1/1: `metaflux.unit.runtime-lifecycle-dispatch` |
| Complete dev build | Passed: `nix develop . --command cmake --build --preset dev -j2` |
| Full dev CTest | Passed 85/85 |
| Convergence inventory | Stable for S0123; foreign Vulkan guidance excluded |
| Repository gates | `git diff --check` and Agent records passed |
| Content identity | `6152efa7d9ca8f0b28e6ec9ed6d9617e8377fb67`; Agent Harness (codex) is Author and Committer |

## Cleanup

- Removed: none; the session guidance inbox was empty and no disposable route
  was created.
- Retained: the shared external dev build tree under its existing owner and two
  foreign guidance packets in the S01322/S01323 inboxes.

## Decisions and experience

- No decision or semantic change. W0122 remains Active; the stable pre-captured
  event API remains authoritative for QMP correlation and delayed completion.

## roast

### light roasts

- Immediate producer ingress contract -> `runtime/core/README.md` (`6152efa`;
  focused 1/1 and full dev CTest 85/85)

### medium roasts

- none.

### dark roasts

- none.

## session-only

- none.

## Unresolved items

- W0122 still needs actual reset/disconnect/restart producer call-site wiring,
  failure injection across staging/commit/DMA/completion/teardown, and live
  qualification.

## Handoff

Resume from P20260831-089 and `6152efa`. Wire one real immediate producer at a
time through `capture_and_submit_external_event`, preserve QMP correlation on
the pre-captured overload, and keep stale observations explicit.
