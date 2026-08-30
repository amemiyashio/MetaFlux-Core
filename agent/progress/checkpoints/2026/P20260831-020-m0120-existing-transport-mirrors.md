---
id: P20260831-020
status: Recorded
captured: 2026-08-31
milestone: M0120
workstream: W0122
branch: main
git_revision: f5cdee3e4d8bef41da3d67672057b3fb3fddac3d
workspace: concrete C++ cdev and vfio-user lifecycle mirrors are wired; memfd, QMP, provider, fault, and qualification gates remain active
---

# M0120 W0122 Existing Transport Mirrors

## Outcome

The existing C++ cdev worker and vfio-user server now consume the normalized
runtime lifecycle coordinator boundary. The cdev worker stops ordinary queue
consumption during quiesce, drains already-published descriptors, advances only
to a coordinator-issued generation, and completes work after loss with
`MF_SHARED_DEVICE_LOST`. The vfio-user server gates control and DMA admission,
requires an empty mapping ledger before reset, advances device generation and
mapping epoch together, and rejects retired pairs with
`MF_SHARED_STALE_HANDLE`.

The generated transport records, frozen M0110 descriptors, base ioctl/mmap UAPI,
BAR profile, and vfio-user wire messages are unchanged. Socket disconnects mark
the local vfio-user server lost; normalized source submission remains owned by
the coordinator caller.

## Verification evidence

| Gate | Result |
|---|---|
| cdev lifecycle regression | Passed: `metaflux.transport.cdev-worker` |
| vfio-user lifecycle regression | Passed: `metaflux.transport.vfio-user-server` |
| Full development CTest | Passed: 75/75 |
| Component graph | Passed: 17 components / 20 edges |
| Agent records before record update | Passed: `python3 tools/check-agent-records.py .` |
| Commit identity | `Agent Harness (codex)` as Author and Committer for `f5cdee3` |

## Boundary

This is an adapter-consumption stage, not lifecycle completion. The concrete
memfd path, QMP/vPCI reset and hotplug correlation, daemon disconnect/restart
normalization, provider enumeration freeze, fault injection at staging/commit/
DMA/completion/teardown, 1,000-cycle qualification, and lifecycle ABI freeze
remain open under W0122/W0121.

## Cleanup

- Removed: none; no session-owned disposable artifact was created.
- Retained: canonical source, plan, test, and compact session records; external
  CMake/Ninja build output remains under its owning build path.

## Handoff

Resume from W0122 with `metaflux.unit.runtime-lifecycle` and the two transport
tests as the focused baseline. Keep generation/epoch publication in the
coordinator and preserve the M0110 descriptor/UAPI/BAR boundary.
