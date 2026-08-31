---
id: P20260901-098
status: Recorded
captured: 2026-09-01
milestone: M0110
workstream: W0112
branch: main
git_revision: 780222c
workspace: cdev per-file state serialization
---

# M0110 W0112 Cdev Per-File State Serialization Checkpoint

## Outcome

All cdev ioctl, mmap, and poll paths now read mutable per-file negotiation,
lease, queue, and registered-memory authorization state under
`mf_cdev_lock`, the same lock used by close and teardown. Generation checks are
also evaluated under that lock at the publication boundary. Long pin/map
operations continue to release resources outside the lock, and rejection
paths preserve the existing errno and unwind behavior.

This is a concurrency hardening boundary inside W0112. It does not claim
daemon-side live cdev lease/import wiring, unmodified Add/Copy through a loaded
`/dev/metafluxN`, generation replacement isolation, or Linux 6.12/6.18 fault
qualification.

## Verification evidence

| Gate | Result |
| --- | --- |
| Development build | Passed with the repository development shell |
| Full development CTest | Passed: 85/85 |
| Linux 6.18 kernel module Kbuild | Passed with `/usr/bin/gcc`; compiler-version warning only |
| Diff checks | Passed: `git diff --check` |
| Agent records before record commit | Passed |
| Live device nodes | Not available: `/dev/metafluxctl` and `/dev/metaflux0` are absent |

## Cleanup

- Removed: none.
- Retained: current cdev kernel source, canonical W0112 plan, progress
  projection, and this compact checkpoint; build output remains outside the
  repository.

## roast

### light roasts

- none.

### medium roasts

- Cdev per-file authorization checks serialized with close and teardown ->
  `kernel/core/metaflux_core_main.c` (`780222c`; full CTest 85/85, Linux 6.18
  Kbuild)

### dark roasts

- none.

## session-only

- Local absence of `/dev/metafluxctl` and `/dev/metaflux0` - reason: live cdev
  daemon attachment and fault qualification remain external host steps.

## Handoff

Continue as `S0112-20260901-001-m0110-w0112-current-epoch`. The next bounded
W0112 unit is daemon-side live cdev lease and registered-memory/backend
attachment; keep live-device and kernel fault claims separate from this
concurrency checkpoint.
