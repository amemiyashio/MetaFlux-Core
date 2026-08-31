---
id: P20260901-095
status: Recorded
captured: 2026-09-01
milestone: M0110
workstream: W0112
branch: main
git_revision: 5050915992771444398879212e37529b44b704ce
workspace: lease-bound current cdev payload query
---

# M0110 W0112 Lease-Bound Cdev Payload Query Checkpoint

## Outcome

The current Linux UAPI now includes `MF_UAPI_IOCTL_MEMORY_QUERY`, reusing the
fixed `mf_uapi_memory_v0` record. A negotiated worker lease can query the
online data-owner payload's generation, exact page-aligned byte count, and
payload mmap offset. `CdevWorkerSession::map_current_payload()` consumes this
response and maps through the lease fd; kernel query and mmap paths recheck
the owner and current generation under the cdev lock.

This is an internal contract boundary inside W0112. It does not claim daemon
object-table wiring, unmodified Add/Copy through a loaded `/dev/metafluxN`,
generation replacement isolation, or Linux 6.12/6.18 fault qualification.

## Verification evidence

| Gate | Result |
| --- | --- |
| Transport schema validator | Passed: 5 definitions, 15 records |
| Lifecycle model and selftest | Passed: 5/5 focused tests |
| Full development CTest | Passed: 85/85 |
| Linux 6.18 kernel module Kbuild | Passed with `/usr/bin/gcc`; compiler-version warning only |
| Agent records before record commit | Passed |
| Live device nodes | Not available: `/dev/metafluxctl` and `/dev/metaflux0` are absent |

## Cleanup

- Removed: none.
- Retained: current UAPI/kernel/worker source, manifest closure, canonical
  W0112 plan, progress projection, and this compact checkpoint; build output
  remains outside the repository.

## roast

### light roasts

- none.

### medium roasts

- Lease-bound current payload query and exact-size worker mapping ->
  `contracts/uapi/linux/v1/schema/uapi.json`
  (revision `5050915992771444398879212e37529b44b704ce`; schema/lifecycle checks,
  full CTest 85/85, Linux 6.18 Kbuild)

### dark roasts

- none.

## session-only

- Local absence of `/dev/metafluxctl` and `/dev/metaflux0` - reason: this host
  cannot provide live cdev attach evidence for the next daemon integration
  unit.

## Handoff

Continue as `S0112-20260901-001-m0110-w0112-current-epoch`. The next bounded
W0112 unit is daemon-side live cdev lease and registered-memory/backend
attachment; keep live-device and kernel fault claims separate from this
host-independent boundary.
