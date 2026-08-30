---
id: P20260831-025
status: Recorded
captured: 2026-08-31
milestone: M0120
workstream: W0122
branch: main
git_revision: 04ecf81f3be5099451012849b7a064ab2863b9b1
workspace: stateless lifecycle event ingress is implemented; source producers and qualification remain active
---

# M0120 W0122 Lifecycle Event Ingress

## Outcome

Runtime core now exposes one stateless `submit_external_event` ingress. It
normalizes a typed external event and submits only the resulting request to the
Coordinator. Malformed or unsupported events return before authority state is
changed, while accepted events preserve the producer-captured request ID,
daemon incarnation, identity record, generation, epoch, and deadline. The
ingress allocates none of those values and does not own source capture or
provider state.

## Verification evidence

| Gate | Result |
|---|---|
| `metaflux.unit.runtime-lifecycle-dispatch` | Passed |
| Lifecycle/normalizer/dispatch focused selection | Passed: 3/3 |
| Full development CTest | Passed: 79/79 |
| Transport schema | Passed: 5 definitions / 15 records |
| Component graph | Passed: 19 components / 22 edges |
| Skill routing | Passed: 89 cases |
| Agent records | Passed |
| Commit identity | `Agent Harness (codex)` as Author and Committer for `04ecf81` |

## Boundary

This is the common runtime ingress, not a concrete QMP, daemon, cdev, memfd, or
vfio-user producer implementation. Source-specific call-site wiring, live QMP
socket parsing, production memfd worker integration, provider enumeration
freeze, failure injection, repeated-cycle qualification, and lifecycle ABI
freeze remain open under W0122/W0123.

## Cleanup

- Removed: none; no session-owned disposable artifact was created.
- Retained: canonical source, plan, test, and compact session records; external
  CMake/Ninja output remains under its owning build path.

## roast

### light roasts

- Runtime external-event ingress ->
  `runtime/core/include/metaflux/runtime/lifecycle_dispatch.hpp` (content
  revision `04ecf81`; focused dispatch regression and full development CTest
  79/79)
- Authority-preserving malformed-event handling ->
  `runtime/core/src/lifecycle_dispatch.cpp` (invalid/unsupported events leave
  Coordinator state unchanged)

### medium roasts

- none.

### dark roasts

- none.

## session-only

- none.

## Handoff

Continue S0122/W0122 from `04ecf81`. Route each concrete source producer through
`submit_external_event`, preserve its captured identity tuple, and submit only
the normalized request to `Coordinator`; do not move lifecycle authority into a
transport adapter.
