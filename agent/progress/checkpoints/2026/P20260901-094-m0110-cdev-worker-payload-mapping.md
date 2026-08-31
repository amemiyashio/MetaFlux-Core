---
id: P20260901-094
status: Recorded
captured: 2026-09-01
milestone: M0110
workstream: W0112
branch: main
git_revision: fcfaa8d
workspace: current cdev worker discovery and payload mapping
---

# M0110 W0112 Current Cdev Worker Payload Mapping Checkpoint

## Outcome

`CdevWorkerSession::open_current()` now negotiates the current cdev view and
generation on the same `/dev/metafluxctl` fd that receives the worker lease.
The session can map the exact page-aligned payload arena allocated by the data
file through that leased control fd, while the data fd remains the payload
owner. Move and close paths release the payload VMA before the queue VMA and
control fd, preserving the existing offline tombstone reference graph.

This is an internal worker/kernel boundary inside W0112. It does not claim
daemon-side live lease/import wiring, unmodified Add/Copy through a loaded
`/dev/metafluxN`, generation replacement isolation, or Linux 6.12/6.18 fault
qualification.

## Verification evidence

| Gate | Result |
| --- | --- |
| Development build | Passed with `METAFLUX_DAEMON_CDEV_BACKEND=1` |
| Full development CTest | Passed: 85/85 |
| Linux 6.18 kernel module Kbuild | Passed with `/usr/bin/gcc`; compiler-version warning only |
| Agent records before record commit | Passed |
| Live device nodes | Not available: `/dev/metafluxctl` and `/dev/metaflux0` are absent |

## Cleanup

- Removed: none.
- Retained: only the current worker/kernel source, canonical W0112 plan,
  progress projection, and this compact checkpoint; build output remains
  outside the repository.

## roast

### light roasts

- none.

### medium roasts

- Same-fd current cdev discovery and lease-bound payload mapping ->
  `transports/cdev/worker/include/metaflux/transport/cdev_worker.hpp`
  (revision `fcfaa8d`; full CTest 85/85; Linux 6.18 Kbuild)

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
