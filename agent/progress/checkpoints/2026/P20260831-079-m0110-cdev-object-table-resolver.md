---
id: P20260831-079
status: Recorded
captured: 2026-08-31
milestone: M0110
workstream: W0112
branch: main
git_revision: f7f45a70250f6d43b0d23116f2822a9a72003a08
workspace: daemon-compatible cdev object-table resolver
---

# M0110 W0112 cdev Object-Table Resolver Checkpoint

## Outcome

The cdev worker now exposes a daemon-compatible `CdevObjectTableResolver`.
It validates the exact region COPY argument block, asks the daemon-owned table
for generation- and permission-bound argument/memory views, checks both ranges,
and invokes the backend memory importer only on the validated subranges. A
complete lookup-provided backend reference is accepted with its retain/release
pair; a destination reference is released when source resolution fails. The
stable ring descriptor, backend ABI, and Linux UAPI are unchanged.

## Verification evidence

| Gate | Result |
|---|---|
| Focused cdev worker test | Passed: 1/1 |
| Full development CTest | Passed: 84/84 |
| Diff checks | Passed: `git diff --check` |
| Content identity | `f7f45a7`, Agent Harness (codex) as Author and Committer |

## Boundary

This proves the host-independent resolver and CPU backend reference closure.
It does not wire the callback to the live daemon object table or mapped cdev
payload arena, implement generation replacement/drain, qualify kernel fault
paths, or provide physical CUDA/NVIDIA evidence.

## Cleanup

- Removed: no session-owned disposable artifacts; external build outputs remain under their existing build owner.
- Retained: resolver contract, CPU regression, transport documentation, W0112 plan update, and compact session records.

## roast

### light roasts

- Object-table resolver validation and backend import composition -> cdev worker header/source/test (`f7f45a7`; focused/full CTest)

### medium roasts

- W0112 daemon-compatible object-table boundary -> W0112 plan/current progress (`f7f45a7`; P079)

### dark roasts

- none.

## session-only

- The CPU object-table fixture uses caller-owned arrays and an aligned local argument block - reason: no live cdev device or daemon object table is available on this host.

## Handoff

Resume W0112 from `f7f45a7` and P079. Connect `CdevObjectTableResolver` to the
daemon's authoritative table and mapped payload arena, then cover generation
replacement and backend wait policy without changing frozen ABI/UAPI records.
