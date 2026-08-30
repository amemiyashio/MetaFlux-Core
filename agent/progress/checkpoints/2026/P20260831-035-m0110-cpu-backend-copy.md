---
id: P20260831-035
status: Recorded
captured: 2026-08-31
milestone: M0110
workstream: W0112
branch: main
git_revision: cc24af0
workspace: CPU backend COPY subset and cdev mapped-payload integration are verified; Add/launch, DMA, queue lifetime, and fault qualification remain open
---

# M0110 W0112 CPU Backend COPY

## Outcome

The CPU backend now exposes a bounded transport-facing `mf_backend_api_v1`
subset: one enumerated CPU device, process-local instance/context/queue and
memory handles, caller-owned host-memory import, and synchronous overlap-safe
COPY. The cdev worker regression binds that real table to the generation-bound
mapped payload and verifies byte-exact completion, while invalid completion
events and ranges remain rejected.

The implementation does not claim the complete backend ABI or W0112 exit gate.
Launch/Add, asynchronous events, generation-bound registered-memory import,
backend DMA mapping, queue krefs, daemon replacement, and kernel fault
qualification remain open.

## Verification evidence

| Gate | Result |
|---|---|
| CPU backend ABI smoke | Passed: lifecycle, one-device enumeration, imported ranges, synchronous COPY, invalid event, and out-of-range rejection |
| cdev worker integration | Passed: real CPU API copied the mapped payload through `CdevBackendBinding` and published a successful completion |
| Focused CTest | Passed: `metaflux.transport.cdev-worker` and `metaflux.abi.backend-cpu`, 2/2 |
| Full development CTest | Passed: 79/79 |
| Component graph | Passed: 19 components / 23 dependency edges |
| Formatting and diff checks | Passed: clang-format dry-run and `git diff --check` |

## Boundary

This checkpoint records the CPU backend COPY increment at content revision
`cc24af0`. Imported host ranges are a testable caller-owned bridge to the
mapped payload; the kernel registered-memory fixture still has no backend
`dma_map_sg` or in-flight reference. No Add/launch, asynchronous event,
production daemon worker, or performance-budget claim is made here.

## Cleanup

- Removed: none; build output remains under the external build owner.
- Retained: CPU backend handle table, host-memory import helper, ABI smoke, and
  cdev worker integration test.

## roast

### light roasts

- CPU backend COPY subset -> `plugins/backend/cpu/runtime/src/backend.cpp`
  (content `cc24af0`, backend ABI smoke)
- Real cdev worker binding -> `transports/cdev/worker/tests/worker_test.cpp`
  (content `cc24af0`, cdev worker integration)

### medium roasts

- none.

### dark roasts

- none.

## session-only

- none.

## Handoff

Resume S0112/W0112 from the CPU backend COPY boundary. Read the W0112 plan,
the backend ABI contract, and the cdev worker source. The next product slice
must either extend the binding to the unmodified CPU Add/launch contract or
implement generation-bound registered-memory/DMA references; keep the imported
host range caller-owned and do not infer kernel DMA evidence from this fixture.
