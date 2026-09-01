---
id: P20260901-103
status: Recorded
captured: 2026-09-01
milestone: M0110
workstream: W0112
branch: main
git_revision: 3a79e8e
workspace: cdev backend generation isolation
---

# M0110 W0112 Cdev Backend Generation Isolation

## Outcome

Revision `3a79e8e` closes a host-independent worker isolation hole in W0112.
Every `CdevBackendBinding` carries its device generation, and the daemon binds
`kDeviceGeneration` explicitly. After a lifecycle commit changes the worker
view generation, a valid old binding is stale for new COPY and LAUNCH requests
and cannot be used for dispatch or lease acquisition. A pending operation keeps
its captured binding until completion or lifecycle drain, so this guard does not
invalidate the operation already in flight.

## Verification evidence

| Gate | Result |
| --- | --- |
| Harness identity | Passed: `Agent Harness (codex) <codex@localhost>` |
| Focused cdev worker CTest | Passed: 1/1 |
| Serial development build | Passed |
| Full development CTest | Passed: 86/86; live cdev test explicitly skipped |
| Agent records before content commit | Passed |
| Diff checks | Passed: `git diff --check` |

## Boundary

This is a host-independent implementation and fixture correction inside the
existing W0112 lifecycle/backend authority. It does not close daemon-controlled
generation replacement and rebind drain, live `/dev/metafluxN` Add/Copy,
kernel registered-memory/DMA import, or Linux 6.12/6.18 sanitizer, lockdep, and
kmemleak qualification.

## Cleanup

- Removed: temporary worker-test diagnostic markers; no debug source remains.
- Retained: current W0112 source, regression test, plan, P103, and the live
  qualification requirement.

## roast

### light roasts

- none.

### medium roasts

- Worker backend binding generation isolation ->
  `transports/cdev/worker/src/worker.cpp` (`3a79e8e`; focused worker 1/1 and
  full CTest 86/86)
- W0112 replacement-generation boundary update ->
  `agent/plan/M0110-kernel-guest-transport/work/W0112-local-cdev.md` (`P103`;
  daemon rebind, live cdev, and kernel fault evidence remain open)

### dark roasts

- none.

## session-only

- `/dev/metafluxctl` and `/dev/metaflux0` are absent on this host, so live cdev
  acceptance remains an environment-dependent next action rather than a
  promoted product claim.

## Unresolved items

- W0112 still needs activated-device Add/Copy and registered-memory/DMA import.
- Daemon rebind and physical replacement drain remain open after the worker-side
  generation guard.
- Linux 6.12/6.18 KUnit, KASAN, KCSAN, lockdep, kmemleak, and fault-injection
  evidence remains unexecuted on this host.

## Handoff

Resume from current focus and P103. Read the W0112 Exit Gate and use
`$manage-host-privilege` for module/device activation; keep all tool entry
Nix-first through the repository environment.
