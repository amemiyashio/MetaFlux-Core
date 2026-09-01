---
id: P20260901-105
status: Recorded
captured: 2026-09-01
milestone: M0110
workstream: W0112
branch: main
git_revision: 75284e1
workspace: cdev generation-bound backend replacement and rebind drain
---

# M0110 W0112 Cdev Generation-Bound Rebind Drain

## Outcome

Revision `75284e1` closes the worker-side replacement boundary left by P103.
`CdevWorker::bind_backend` now accepts a backend only while the lifecycle mirror
is online and the binding names the committed worker generation. A stale or
pre-commit binding is rejected without replacing the active binding.

When a binding is replaced, the worker keeps the captured binding, operation
lease, and memory references for an asynchronous request until the request
finishes, is cancelled, or is rejected during pending progress. An optional
retire callback is invoked only after those references drain. Lifecycle commit
clears and retires the previous current binding before exposing the new
generation, forcing an explicit generation-matched rebind. Rebinding a pending
binding back to the current owner does not retire that still-live binding.

The public ring, ioctl, mmap, completion, and copy-accounting records are
unchanged. This is a worker-side lifecycle and backend ownership correction;
daemon/provider authority and physical replacement remain separate gates.

## Verification evidence

| Gate | Result |
| --- | --- |
| Harness identity | Passed: `Agent Harness (codex) <codex@localhost>` |
| CMake development build | Passed |
| Focused cdev worker CTest | Passed: 1/1, including replacement and rebind-back drain assertions |
| Full development CTest | Passed: 86/86 on the same source family; live cdev test skipped by existing host capability gate |
| Agent records before product commit | Passed: schema v2, D0029, 6 sessions, 30 events, 273 Markdown files, 82 liquidated tombstones |
| Diff checks | Passed: `git diff --check` |
| Repository-wide format target | Existing baseline violations remain across unrelated files; no unrelated formatting migration was included |

## Boundary

This checkpoint closes worker-side generation validation, binding replacement,
pending-operation retirement ordering, and lifecycle-commit rebind invalidation.
It does not close daemon/provider-controlled physical replacement, live
registered-memory/DMA import, live daemon Add/Copy through `/dev/metafluxN`, or
the Linux 6.12/6.18 KUnit, sanitizer, lockdep, kmemleak, and fault-injection
matrix required by the W0112 Exit Gate.

## Cleanup

- Removed: temporary worker-test phase diagnostics and the exact temporary live
  qualification binary from the prior checkpoint.
- Retained: `75284e1`, the source/tests/docs, and the open W0112 qualification
  requirements.

## roast

### light roasts

- none.

### medium roasts

- Online committed-generation acceptance and stale rebind rejection ->
  `transports/cdev/worker/src/worker.cpp` (`75284e1`; public declaration and
  focused cdev worker CTest are evidence)
- Captured backend, lease, and memory-reference drain before owner retirement ->
  `transports/cdev/worker/src/worker.cpp` (`75284e1`; asynchronous completion,
  timeout, and rebind-back regression paths)
- Lifecycle commit invalidation and explicit new-generation rebind ->
  `transports/cdev/worker/src/worker.cpp` (`75284e1`; reset and
  stale-generation regression paths are evidence)

### dark roasts

- none.

## session-only

- The repository-wide formatting target reports pre-existing violations in
  unrelated files and is retained only as a verification caveat; it is not a
  product acceptance claim.

## Unresolved items

- Attach or provide a real DMA-capable cdev provider, then rerun registered
  memory and live daemon Add/Copy qualification.
- Connect daemon/provider authoritative replacement to the worker binding and
  prove physical-generation replacement, fd/VMA tombstones, and fault paths.
- Execute the Linux 6.12/6.18 fault, sanitizer, lockdep, kmemleak, and death
  matrix on a configured qualification host.

## Handoff

Resume the current D0029 focus from `75284e1` and P105. Keep Nix-first tool
entry, use `$manage-host-privilege` for any module or other privileged driver
operation, and treat worker-side rebind closure as source evidence rather than
live physical-device qualification.
