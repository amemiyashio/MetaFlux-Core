---
id: P20260901-097
status: Recorded
captured: 2026-09-01
milestone: M0110
workstream: W0112
branch: main
git_revision: c9682ff
workspace: cdev payload query lease-state serialization
---

# M0110 W0112 Cdev Payload Query Lease-State Serialization Checkpoint

## Outcome

`MF_UAPI_IOCTL_MEMORY_QUERY` now checks the worker lease and negotiation state
under `mf_cdev_lock`, the same lock used by `mf_cdev_release()` when those
fields are cleared. The query path therefore does not read mutable per-file
state outside the cdev lifetime lock before inspecting the online payload.

This is a concurrency refinement inside W0112. It does not claim daemon-side
live cdev lease/import wiring, unmodified Add/Copy through a loaded
`/dev/metafluxN`, generation replacement isolation, or Linux 6.12/6.18 fault
qualification.

## Verification evidence

| Gate | Result |
| --- | --- |
| Development build | Passed with the repository development shell |
| Full development CTest | Passed: 85/85 |
| Linux 6.18 kernel module Kbuild | Passed with `/usr/bin/gcc`; compiler-version warning only |
| Agent records before record commit | Passed |
| Live device nodes | Not available: `/dev/metafluxctl` and `/dev/metaflux0` are absent |

## Cleanup

- Removed: none.
- Retained: current cdev kernel source, canonical W0112 plan, progress
  projection, and compact checkpoints; build output remains outside the
  repository.

## roast

### light roasts

- none.

### medium roasts

- Query lease-state checks serialized with cdev release ->
  `kernel/core/metaflux_core_main.c` (revision `c9682ff`; full CTest 85/85;
  Linux 6.18 Kbuild)

### dark roasts

- none.

## session-only

- Local absence of `/dev/metafluxctl` and `/dev/metaflux0` - reason: live cdev
  daemon attachment remains an external host qualification step.

## Handoff

Continue as `S0112-20260901-001-m0110-w0112-current-epoch`. The next bounded
W0112 unit is daemon-side live cdev lease and registered-memory/backend
attachment; keep live-device and kernel fault claims separate from this
concurrency checkpoint.
