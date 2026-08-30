---
id: P20260831-033
status: Recorded
captured: 2026-08-31
milestone: M0120
workstream: W0121
branch: main
git_revision: 941f49f
workspace: runtime telemetry producers are bound to lifecycle fence/admission snapshots; marker-complete recovery validates staged target banks; live provider hooks and qualification remain open
---

# M0120 W0121 Runtime Telemetry Fence Guards

## Outcome

W0121 now carries the bounded fence/telemetry publication rules into the
`RegistryView` runtime publication paths. Legacy and recovery producers validate
device identity, lifecycle sequence, stable online state, and `OPEN` admission
before claiming the latch, while the same checks are repeated inside the odd
latch window and after staging the inactive bank. Stale or future rows return
`MF_SHARED_RETRY`; a closed or lost device returns `MF_SHARED_DEVICE_LOST` and
cannot publish an `ONLINE` snapshot.

Marker-complete owner-death recovery now validates the staged target bank before
promoting it. A loss after target-bank staging restores the old controls and
settles the publication as aborted, leaving the stale bank inactive. The shared
ABI is unchanged; the new recovery fault point is test-only.

## Verification evidence

| Gate | Result |
|---|---|
| Focused runtime registry/recovery CTest | Passed: 2/2, including legacy and recovery stale/future sequence, loss, and marker-complete owner-death cases |
| Full development CTest | Passed: 79/79 |
| Formatting and diff checks | Passed: clang-format dry-run and `git diff --check` |
| Commit identity | `Agent Harness (codex)` as Author and Committer for content `941f49f` |

## Boundary

This checkpoint proves producer-side fence/admission validation and recovery
target-bank containment for the in-repository runtime mappings. It does not
claim live CUDA/NVML provider hooks, QMP/socket or vPCI producer wiring,
1,000-cycle fault qualification, or lifecycle ABI freeze. Those remain active
W0121/W0122/W0113 work.

## Cleanup

- Removed: temporary build/test output and fault-injection fixtures.
- Retained: runtime guards, regression tests, and this compact checkpoint.

## roast

### light roasts

- Producer fence/admission guard -> `runtime/core/src/runtime.cpp` (legacy path validates stable online rows before, during, and after bank staging)
- Recovery target-bank guard -> `runtime/core/src/registry_recovery.cpp` (marker-complete owner-death path validates identity/sequence before promotion)
- Regression coverage -> `runtime/core/tests/registry.cpp` and `runtime/core/tests/registry_recovery.cpp` (stale/future/loss and marker recovery assertions)

### medium roasts

- none.

### dark roasts

- none.

## session-only

- none.

## Handoff

Resume S0121/W0121 from this checkpoint. Wire the same fence/admission contract
into live provider call sites and transport producers, then add bounded fault
qualification without changing the frozen M0110 ABI or promoting provisional
hardware performance claims.
