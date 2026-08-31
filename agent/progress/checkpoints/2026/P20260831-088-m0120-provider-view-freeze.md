---
id: P20260831-088
status: Recorded
captured: 2026-08-31
milestone: M0120
workstream: W0122
branch: main
git_revision: a2c7a95889f1996246237eb77e1555f4b35d36a9
workspace: provider enumeration freeze
---

# M0120 W0122 Provider View Freeze Checkpoint

## Outcome

The shared client Registry handle now validates a nonzero
`process_view_revision` in both the registry header and view-control record and
captures that revision at attach. CUDA and NVML retain the captured revision as
part of their initialization-epoch membership snapshot. An attached provider
keeps its count and ordinal membership while a test-owned backing view advances
its revision; after a zero-to-one reinitialization, the provider captures the
new revision.

## Verification evidence

| Gate | Result |
|---|---|
| Focused provider-view regression | Passed: `integration.runtime-fastpath-registry`, `unit.provider.cuda-semantics`, and `unit.provider.nvml-semantics` 3/3 |
| Full Vulkan runtime CTest | Passed: 91/91 under `.#vulkan-runtime` |
| Build | Passed: fastpath, CUDA/NVML providers and test targets |
| Diff checks | Passed: `git diff --check` |
| Records | Passed: `python3 tools/check-agent-records.py .` before checkpoint commit |
| Content identity | `a2c7a95889f1996246237eb77e1555f4b35d36a9`; Agent Harness (codex) is Author and Committer |

## Boundary

This checkpoint closes the host-independent provider-view freeze item in W0122.
It does not wire every reset/disconnect/restart producer, inject failures at
all transport boundaries, claim live QMP/kernel qualification, or promote
physical NVIDIA/Intel evidence.

## Cleanup

- Removed: none; no session-owned disposable route or repository-local build
  output was created.
- Retained: external build trees under their existing owner and foreign
  guidance packets in the S01322/S01323 inboxes.

## roast

### light roasts

- Captured and validated process-view revision ->
  `runtime/client/fastpath/include/metaflux/client/fastpath.h` and
  `runtime/client/fastpath/src/fastpath.c` (`a2c7a95`, focused and full CTest)

### medium roasts

- W0122 provider enumeration freeze boundary ->
  `agent/plan/M0120-vpci-lifecycle/work/W0122-existing-transports.md` (P088)

### dark roasts

- none.

## session-only

- Test-owned revision mutation models later publication without claiming a live
  daemon/provider replacement or physical-driver qualification.

## Handoff

Resume W0122 from P088 and `a2c7a95889f1996246237eb77e1555f4b35d36a9`. The next
bounded unit is reset/restart producer wiring or staged fault injection; keep
provider membership tied to its captured revision and preserve the M0110 root.
