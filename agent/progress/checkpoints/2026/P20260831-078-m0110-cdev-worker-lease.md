---
id: P20260831-078
status: Recorded
captured: 2026-08-31
milestone: M0110
workstream: W0112
branch: main
git_revision: 0b67854
workspace: cdev worker lease activation and queue mapping
---

# M0110 W0112 cdev Worker Lease Checkpoint

## Outcome

The cdev worker now has a `CdevWorkerSession` that opens the control broker,
submits a generation- and `registry_view_id`-bound worker lease, validates the
identity, exact paired-ring layout, and queue metadata, then maps the rings
through the leased control fd. Closing the session unmaps before closing the
fd, so lease release remains explicit. The kernel control fops expose the
queue mmap at offset zero for a valid lease; payload mapping remains a separate
data-plane boundary. The frozen backend ABI and Linux UAPI records are
unchanged.

## Verification evidence

| Gate | Result |
|---|---|
| Focused cdev worker test | Passed: 1/1 |
| Full development CTest | Passed: 84/84 |
| Kernel Kbuild | Passed: `make -C /lib/modules/$(uname -r)/build M=$PWD/kernel/core modules` |
| Diff checks | Passed: `git diff --check` |
| Record checks | Passed: `python3 tools/check-agent-records.py .` |
| Content identity | `0b67854`, Agent Harness (codex) as Author and Committer |

## Boundary

This proves the host-independent worker lease activation and control-fd queue
mapping contract. It does not connect the daemon object table to live cdev
registered-memory handles, implement generation replacement, qualify kernel
fault/sanitizer paths, or provide physical CUDA/NVIDIA evidence.

## Cleanup

- Removed: no session-owned disposable artifacts; ignored build outputs remain under external build owners.
- Retained: worker lease contract, kernel mmap correction, regression, transport docs, and compact records.

## roast

### light roasts

- Generation/view-bound cdev worker lease and paired queue mapping -> `transports/cdev/worker/src/worker.cpp` (`0b67854`; focused/full CTest and Kbuild)

### medium roasts

- W0112 control/data mapping ownership boundary -> `agent/plan/M0110-kernel-guest-transport/work/W0112-local-cdev.md` (P078; daemon object-table activation remains open)

### dark roasts

- none.

## session-only

- `/dev/null` regression exercises deterministic unsupported-ioctl mapping - reason: no live cdev device is present in the current host environment.

## Handoff

Resume W0112 from `0b67854` and P078. Read the cdev worker lease/importer
contracts, then wire daemon object-table registered-memory handles and
generation replacement without changing frozen ABI records.
