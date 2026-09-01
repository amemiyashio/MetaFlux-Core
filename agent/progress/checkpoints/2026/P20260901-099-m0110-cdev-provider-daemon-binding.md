---
id: P20260901-099
status: Recorded
captured: 2026-09-01
milestone: M0110
workstream: W0112
branch: main
git_revision: 047ea9428fe28e56ae3dac5bbf6db96499dc070e
workspace: cdev provider and daemon worker binding
---

# M0110 W0112 Cdev Provider And Daemon Worker Binding Checkpoint

## Outcome

The D0030 provider boundary is now implemented in source. A managed local
provider attempts cdev first and commits the cdev transport only after the cdev
view/generation matches the Unix daemon session and the daemon accepts the
binding control record. Unix remains the object/control plane; the leased cdev
queue carries COPY and primary-entry LAUNCH. The daemon binds one cdev worker,
maps the exact data-owner payload reported by `MF_UAPI_IOCTL_MEMORY_QUERY`,
loads canonical KIR into the CPU backend, and resolves object-table memory
references with explicit operation lifetime accounting.

This checkpoint closes provider selection and source-level daemon wiring. It
does not claim live `/dev/metafluxN` Add/Copy, kernel registered-memory DMA
import, generation replacement isolation, or Linux 6.12/6.18 fault
qualification.

## Verification evidence

| Gate | Result |
| --- | --- |
| Focused cdev worker test | Passed: 1/1, including launch memory-reference balance |
| Development build | Passed in the repository Nix development shell |
| Full development CTest | Passed: 85/85 |
| Provider-only cdev-disabled build | Passed |
| Provider-only cdev-disabled CTest | Passed: 40/40 |
| Linux 6.18 core Kbuild | Passed in the repository Nix environment: compile, modpost, BTF |
| Agent records before checkpoint record | Passed |
| Diff checks | Passed: `git diff --check` |
| Live device nodes | Not available: `/dev/metafluxctl` and `/dev/metaflux0` are absent |

## Cleanup

- Removed: none.
- Retained: current W0112 source, canonical plan, progress projection, and
  this compact checkpoint; build output remains outside the repository.

## roast

### light roasts

- none.

### medium roasts

- D0030 cdev-first provider selection and same-session/view/generation binding
  -> `agent/plan/M0110-kernel-guest-transport/plan.md` (implementation
  evidence: `contracts/protocol/client/v1/include/metaflux/client/protocol.h`
  and `plugins/compat/cuda/abi/driver/src/provider.c`; `047ea94`; full CTest
  85/85; provider-only cdev-disabled CTest 40/40)
- Bound daemon cdev worker with CPU COPY/LAUNCH object-table resolution ->
  `agent/plan/M0110-kernel-guest-transport/work/W0112-local-cdev.md`
  (implementation evidence: `services/metafluxd/server.cpp` and
  `transports/cdev/worker/src/worker.cpp`; `047ea94`; focused worker CTest 1/1;
  full CTest 85/85; Linux 6.18 Kbuild)

### dark roasts

- none.

## session-only

- Local absence of `/dev/metafluxctl` and `/dev/metaflux0` - reason: live cdev
  lease/import, generation replacement, and kernel fault qualification require
  an activated Linux device-node environment.

## Handoff

Continue as `S0112-20260901-001-m0110-w0112-current-epoch`. The next bounded
W0112 unit is live cdev Add/Copy qualification, including registered-memory
DMA-backed references, replacement-generation isolation, fd/VMA tombstones,
and the Linux fault matrix.
