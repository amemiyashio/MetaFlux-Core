---
id: P20260831-089
status: Recorded
captured: 2026-08-31
milestone: M0120
workstream: W0122
branch: main
git_revision: 6152efa7d9ca8f0b28e6ec9ed6d9617e8377fb67
workspace: immediate lifecycle producer ingress
---

# M0120 W0122 Immediate Producer Ingress Checkpoint

## Outcome

The runtime now exposes one bounded helper for immediate admin reset, VFIO-user
reset, disconnect, and daemon restart producers. It captures the Coordinator
tuple once and delegates to the canonical normalized ingress. Other known
producer routes and unknown event kinds return `Unsupported` before authority
mutation. A concurrent authority advance remains observable as `Stale`; the
helper never recaptures against replacement identity. Delayed QMP correlation
continues to submit its pre-captured event.

## Verification evidence

| Gate | Result |
|---|---|
| Focused lifecycle dispatch | Passed 1/1: `metaflux.unit.runtime-lifecycle-dispatch` |
| Complete dev build | Passed: `nix develop . --command cmake --build --preset dev -j2` |
| Full dev CTest | Passed 85/85 after the complete build |
| Convergence inventory | Stable S0123 scope; foreign S01322/S01323 guidance excluded |
| Diff and records | `git diff --check` and `python3 tools/check-agent-records.py .` passed before record commit |
| Content identity | `6152efa7d9ca8f0b28e6ec9ed6d9617e8377fb67`; Agent Harness (codex) is Author and Committer |

## Boundary

This checkpoint records the reusable runtime ingress boundary and its
host-independent regressions. It does not prove that a concrete admin,
vfio-user, disconnect, or restart producer is wired in production. Staged
failure injection, live transport activity, kernel/QEMU evidence, and W0122
qualification remain open.

## Cleanup

- Removed: none; no session-owned disposable route or guidance packet existed.
- Retained: the shared external dev build tree under its existing owner and
  foreign guidance packets in the S01322/S01323 inboxes.

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

## Handoff

Resume W0122 from P20260831-089 and `6152efa`. Wire one real immediate producer
at a time, preserve delayed QMP correlation on the pre-captured overload, and
then inject failures at the existing staging, commit, DMA, completion, and
teardown boundaries.
