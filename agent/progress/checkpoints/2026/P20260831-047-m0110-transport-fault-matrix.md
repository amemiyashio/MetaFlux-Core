---
id: P20260831-047
status: Recorded
captured: 2026-08-31
milestone: M0110
workstream: W0114
branch: main
git_revision: 700b7c8
workspace: bounded cdev/vfio-user fault matrix is tested; full qualification remains open
---

# M0110 W0114 Transport Fault Matrix

## Outcome

The first W0114 fault-qualification stage is recorded at content revision
`700b7c8`. The cdev worker now returns stable dispositions for unknown COPY
flags, known direct-host flags that this worker does not implement, zero-length
COPY, and invalid payload bases. Completion backpressure leaves the submission
unconsumed and resumes in FIFO order after capacity returns. The vfio-user
server exercises recoverable malformed framing, stale exact unmap, DMA address
overflow, duplicate ranges, and a valid request after malformed input without
reusing a retired mapping.

This is a bounded host-independent/socketpair stage. It does not claim ioctl or
BAR fuzzing, MSI-X behavior, live backend-reference draining, ownership-death
injection, native/compat qualification, or the W0114 ABI-freeze exit gate.

## Verification evidence

| Gate | Result |
|---|---|
| Focused transport fault tests | Passed: cdev worker and vfio-user server 2/2 |
| Full development CTest | Passed: 82/82 |
| Formatting and diff checks | Passed: `clang-format` and `git diff --check` |
| Content identity | Passed: `700b7c8`, Agent Harness (codex) as Author and Committer |
| Agent records | Pending the separate record commit for this checkpoint |

## Boundary

W0114 remains Active. The next increment must add bounded ioctl/BAR and DMA
reference faults, then connect the evidence to the W0112/W0113 data-plane
vertical slices before any transport ABI freeze claim.

## Cleanup

- Removed: none; the external CMake build directory remains ignored and owned
  by the build workflow.
- Retained: cdev/vfio-user fault tests, source disposition checks, and this
  checkpoint.

## roast

### light roasts

- Bounded transport fault dispositions -> `transports/cdev/worker` and
  `transports/vfio-user/server` (content `700b7c8`; focused and full dev CTest)

### medium roasts

- none.

### dark roasts

- none.

## session-only

- Live QEMU/libvfio-user, physical MSI-X, and kernel fault injection are not
  available on this host - reason: this checkpoint records only deterministic
  userspace and socketpair behavior.

## Handoff

Resume S0114 from this checkpoint, run
`nix develop . --command ctest --preset dev`, then read W0114 and the Linux UAPI,
vfio-user, PCI, and performance skills before adding ownership or kernel
qualification.
