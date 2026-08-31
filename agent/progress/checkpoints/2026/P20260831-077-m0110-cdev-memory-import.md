---
id: P20260831-077
status: Recorded
captured: 2026-08-31
milestone: M0110
workstream: W0112
branch: main
git_revision: c40c0e3
workspace: cdev resolver-side registered-memory import seam
---

# M0110 W0112 cdev Registered-Memory Import Checkpoint

## Outcome

The cdev worker contract now exposes `CdevBackendMemoryImporter` for the
resolver/object-table owner. After validating an object generation and range,
the resolver can turn caller-owned registered memory into a backend handle with
a complete retain/release reference. The real CPU backend importer is exercised
for two region COPY ranges, and the worker keeps both references balanced through
dispatch and completion. `mf_backend_api_v1` and the Linux UAPI are unchanged.

## Verification evidence

| Gate | Result |
|---|---|
| Focused cdev/component tests | Passed: 2/2 |
| Full development CTest | Passed: 84/84 |
| Diff checks | Passed: `git diff --check` |
| Content identity | `c40c0e3`, Agent Harness (codex) as Author and Committer |

## Boundary

This proves the host-independent resolver-to-backend import seam and reference
handoff. It does not wire `metafluxd`'s live object table to `/dev/metafluxN`
registered handles, implement daemon generation replacement, qualify kernel
fault/sanitizer paths, or provide physical CUDA/NVIDIA evidence.

## Cleanup

- Removed: no session-owned disposable artifacts; ignored build outputs remain under external build owners.
- Retained: importer contract, CPU regression, transport documentation, and compact session records.

## roast

### light roasts

- Resolver-side registered-range importer contract -> `transports/cdev/worker/include/metaflux/transport/cdev_worker.hpp` (`c40c0e3`; focused/full CTest)

### medium roasts

- W0112 registered-memory import boundary -> `agent/plan/M0110-kernel-guest-transport/work/W0112-local-cdev.md` (P077; daemon object-table activation remains open)

### dark roasts

- none.

## session-only

- CPU fixture imports caller-owned arrays directly - reason: live cdev registered-memory and physical-device qualification require the kernel/device environment.

## Handoff

Resume W0112 from `c40c0e3` and P077. Read the cdev resolver/importer contract
and daemon object ownership rules, then wire live registered-memory handles and
generation replacement without changing frozen ABI records.
