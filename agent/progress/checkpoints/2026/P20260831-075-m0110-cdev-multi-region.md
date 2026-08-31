---
id: P20260831-075
status: Recorded
captured: 2026-08-31
milestone: M0110
workstream: W0112
branch: main
git_revision: a6df246
workspace: bounded cdev registered-memory region table
---

# M0110 W0112 cdev Multi-Region Checkpoint

## Outcome

The local cdev `MEMORY_REGISTER` path now publishes a bounded private table of
four generation-bound regions. Each region owns an independent page-pin, SG
table, DMA mapping, memlock charge, and `mm` reference. Handles are unique per
slot (`3` through `6`), negotiation reports `max_regions=4`, and the module
enforces a 256 MiB aggregate byte quota with `-EBUSY` for a full table or quota
overflow. Explicit unregister, owner close, module exit, and copy-to-user
failure remove slots under the cdev lock and release resources outside the lock
in reverse dependency order.

## Verification evidence

| Gate | Result |
|---|---|
| Target Kbuild | Passed: Linux 6.18.42 GCC built `metaflux_core.ko` with modpost and BTF success |
| Full development CTest | Passed: 84/84 |
| Diff checks | Passed: `git diff --check` |
| Content identity | `a6df246`, Agent Harness (codex) as Author and Committer |

## Boundary

This proves bounded kernel registered-memory table ownership and pin/SG/DMA
teardown. It does not implement production backend memory import, in-flight
device references, daemon generation replacement, kernel sanitizer/fault
qualification, or physical CUDA/NVIDIA qualification.

## Cleanup

- Removed: no session-owned disposable artifacts; ignored Kbuild outputs remain under the target-kernel build owner.
- Retained: multi-region kernel implementation, client constants, transport/kernel documentation, and compact session records.

## roast

### light roasts

- Bounded four-slot registered-memory table with unique handles and aggregate quota -> `kernel/core/metaflux_core_main.c` (`a6df246`; target Kbuild and full CTest)

### medium roasts

- W0112 registered-memory multi-region boundary -> `agent/plan/M0110-kernel-guest-transport/work/W0112-local-cdev.md` (P075; production backend import remains open)

### dark roasts

- none.

## session-only

- Four-slot and aggregate-quota constants are current fixture limits - reason: physical DMA and kernel sanitizer qualification remain environment-dependent.

## Handoff

Resume W0112 from `a6df246` and P075. Read the cdev backend binding and registered-memory reference contracts, then connect production backend memory import and daemon generation replacement without changing the fixed UAPI record.
