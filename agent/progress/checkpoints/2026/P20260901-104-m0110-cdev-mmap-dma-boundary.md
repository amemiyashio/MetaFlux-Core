---
id: P20260901-104
status: Recorded
captured: 2026-09-01
milestone: M0110
workstream: W0112
branch: main
git_revision: 16707ec
workspace: cdev mmap and DMA capability boundary
---

# M0110 W0112 Cdev Mmap and DMA Capability Boundary

## Outcome

Revision `16707ec` repairs two live cdev integration defects exposed by the
first activated-device run. The kernel now publishes page-aligned paired-ring
sizes, and the cdev client and worker validate the same transport-local size;
this makes the Linux VMA length agree with `remap_vmalloc_range`. Every worker
control fd now negotiates its view and generation before taking a lease, so the
lease-bound payload query is usable by both current-discovery and
expected-value activation paths.

The kernel also rejects registered-memory requests with `-EOPNOTSUPP` before
pinning when the standalone virtual misc cdev has no DMA mask or parent DMA
master. The live qualification therefore reports a capability skip on this
host instead of a generic system error. Session, queue, lease, payload query,
and payload mapping reached the registered-memory boundary successfully.

## Verification evidence

| Gate | Result |
| --- | --- |
| Harness identity | Passed: `Agent Harness (codex) <codex@localhost>` |
| CMake development build | Passed |
| Linux 6.18 Kbuild | Passed; existing compiler-version warning only |
| Focused cdev CTest | Passed: cdev client 1/1 and worker 1/1 |
| Privileged live qualification | Reached registered-memory stage; `driver live` returned 77 with `registered-memory DMA target is unavailable` |
| Full development CTest | Passed: 86/86; live test skipped with module unloaded |
| Agent records before checkpoint | Passed |
| Diff checks | Passed: `git diff --check` |

## Boundary

This checkpoint closes the page-aligned mmap and worker-control negotiation
defects in the current W0112 implementation. It does not close the W0112 Exit
Gate: the host has no DMA-capable cdev parent, so registered-memory/DMA import,
daemon Add/Copy through the live data plane, daemon-controlled replacement and
rebind drain, and Linux 6.12/6.18 KUnit, sanitizer, lockdep, kmemleak, and
fault-injection qualification remain open.

## Cleanup

- Removed: the exact in-repository temporary live qualification binary; the
  temporary module was unloaded through `manage-host-privilege`, leaving no
  `/dev/metaflux0` or `/dev/metafluxctl` nodes active.
- Retained: `16707ec`, the cdev source/tests/docs, and the W0112 open gates.

## roast

### light roasts

- none.

### medium roasts

- Page-aligned cdev queue mapping across kernel, client, worker, and qualification
  -> `kernel/core/metaflux_core_main.c` (`16707ec`; Linux 6.18 Kbuild, focused
  cdev tests, and full CTest)
- Negotiated worker control lease required for lease-bound payload query ->
  `transports/cdev/worker/src/worker.cpp` (`16707ec`; focused worker CTest and
  privileged live path)
- Explicit no-DMA-master capability boundary before registered-memory pinning
  -> `kernel/core/metaflux_core_main.c` (`16707ec`; privileged live result and
  full CTest)

### dark roasts

- none.

## session-only

- The current host exposes only the standalone virtual misc cdev, with no DMA
  mask or parent DMA master; the governed live helper consequently stops at the
  registered-memory capability boundary with exit 77. This is host state, not
  a promoted physical-DMA acceptance claim.

## Unresolved items

- Attach or provide a real DMA-capable cdev provider, then rerun registered
  memory and live Add/Copy qualification.
- Complete daemon generation replacement/rebind drain and prove the active
  enqueue and fd/VMA tombstone requirements.
- Execute the Linux 6.12/6.18 fault, sanitizer, lockdep, kmemleak, and death
  matrix on a configured qualification host.

## Handoff

Resume the current D0029 focus from `16707ec` and P104. Keep Nix-first tool
entry, use `$manage-host-privilege` for module activation, and treat the
registered-memory capability skip as an open host prerequisite rather than a
W0112 Exit Gate result.
