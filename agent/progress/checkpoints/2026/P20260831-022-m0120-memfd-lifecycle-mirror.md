---
id: P20260831-022
status: Recorded
captured: 2026-08-31
milestone: M0120
workstream: W0122
branch: main
git_revision: 1dbc571f5e44500d1d5c14492fab503683634b7
workspace: memfd client ownership and worker lifecycle mirror are implemented; production wiring, source normalization, provider freeze, fault injection, and qualification remain active
---

# M0120 W0122 memfd Lifecycle Mirror

## Outcome

The M0100 memfd fallback now has an explicit transport boundary in the build:
the existing C17 `runtime/client/fastpath` is registered as the client half and
`transports/memfd/worker/` supplies a C++ lifecycle adapter. The adapter consumes
only coordinator-issued identity, generation, and epoch candidates; it rejects
stale submissions, blocks admission during quiesce, requires all in-flight work
to drain before commit, and leaves removed or lost generations unusable. Failed
drain restores the previous local state without reusing the consumed candidate.
No ring, UAPI, or provider contract was duplicated or changed.

## Verification evidence

| Gate | Result |
|---|---|
| `metaflux.transport.memfd-worker` | Passed |
| Full development CTest | Passed: 76/76 |
| Transport schema | Passed: 5 definitions / 15 records |
| Component graph | Passed: 19 components / 22 edges |
| Skill routing | Passed: 89 cases |
| Commit identity | `Agent Harness (codex)` as Author and Committer for `1dbc571` |

## Boundary

This is an adapter stage, not lifecycle completion. Production memfd worker
attachment, QMP/disconnect/restart request normalization, provider enumeration
freeze, failure injection at staging/commit/DMA/completion/teardown, repeated
cycle qualification, and lifecycle ABI freeze remain open under W0122/W0123.

## Cleanup

- Removed: none; no session-owned disposable artifact was created.
- Retained: canonical source, plan, test, and compact session records; external
  CMake/Ninja output remains under its owning build path.

## roast

### light roasts

- Memfd worker lifecycle mirror and generation-bound drain behavior ->
  `transports/memfd/worker/` (content revision `1dbc571`; focused test and full
  development CTest 76/76)
- Memfd client/worker component registration -> `transports/memfd/` and CMake
  component graph (content revision `1dbc571`; 19 components / 22 edges)

### medium roasts

- none.

### dark roasts

- none.

## session-only

- none.

## Handoff

Continue S0122/W0122 from `1dbc571`. Keep generation and epoch publication in
the runtime Coordinator, treat the memfd adapter as a local mirror, and run the
memfd focused test plus full development CTest after the next adapter change.
