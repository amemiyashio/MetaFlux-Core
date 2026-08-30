---
id: P20260831-027
status: Recorded
captured: 2026-08-31
milestone: M0110
workstream: W0112
branch: main
git_revision: 9dc324d0a4620f16b1592acc7bd5f9296d36f800
workspace: cdev worker has a checked backend COPY dispatch seam; production memory import and Add/Copy remain active
---

# M0110 W0112 Backend Dispatch Seam

## Outcome

The local cdev worker now accepts an explicit `CdevBackendBinding` and routes a
payload COPY through the sized `mf_backend_api_v1` table. It validates the ABI
table size, `MF_BACKEND_CAP_COPY`, nonzero opaque handles, and checked
base-relative offsets. Backend statuses map to the existing shared status
vocabulary. A malformed or unsupported bound API completes with
`MF_SHARED_NOT_SUPPORTED`; an unbound worker retains the local fixture
`memmove` path.

## Verification evidence

| Gate | Result |
|---|---|
| cdev worker target | Built with the Nix development toolchain |
| cdev worker regression | Passed: fake backend COPY translation, success, timeout mapping, and unsupported capability |
| Focused CTest | Passed: cdev worker and component graph 2/2 |
| Full development CTest | Passed: 79/79 |
| Transport schema | Passed: 5 definitions / 15 records |
| Agent records before this checkpoint | Passed: 37 sessions / 272 events / 253 Markdown files |
| Commit identity | `Agent Harness (codex)` as Author and Committer for `9dc324d` |

## Boundary

This is a dispatch seam, not the W0112 exit gate. The binding is synchronous,
uses one backend memory handle for both endpoints, and does not import the
driver-owned mapping into the CPU backend. Backend memory import, DMA mapping,
in-flight reference draining, production CPU Add/Copy, queue kref/tombstone
ownership, daemon replacement, and lifecycle/fault qualification remain open.

## Cleanup

- Removed: no session-owned disposable artifact; build outputs remain in the
  ignored external build tree.
- Retained: cdev worker source, component graph rule, focused regression, W0112
  plan, and this compact checkpoint.

## roast

### light roasts

- Worker backend binding and table validation ->
  `transports/cdev/worker/include/metaflux/transport/cdev_worker.hpp` (content
  revision `9dc324d`; cdev worker regression)
- COPY descriptor translation and status mapping ->
  `transports/cdev/worker/src/worker.cpp` (content revision `9dc324d`; full CTest
  79/79)
- Transport-worker dependency boundary -> `tools/check-component-graph.py`
  (component graph CTest)

### medium roasts

- none.

### dark roasts

- none.

## session-only

- Direct execution outside `nix develop` lacks the Nix-provided `libstdc++.so.6`;
  CTest under the declared shell is the valid runtime evidence.
- Repository-wide format target still reports pre-existing violations outside
  this change; the three touched C++ files pass individual clang-format checks.

## Handoff

Continue S0112/W0112 from `9dc324d`. Keep the worker binding as a synchronous,
one-memory-handle seam until the backend import contract and in-flight lifetime
are specified. The next concrete work must connect a real CPU memory object and
Add/Copy path, then add queue krefs, replacement generation ownership, and
fault/teardown qualification without changing the inherited descriptor.
