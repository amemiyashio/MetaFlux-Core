---
id: P20260831-021
status: Recorded
captured: 2026-08-31
milestone: M0110
workstream: W0112
branch: main
git_revision: 7ba52c501bbdf597a9cd6ba8538066a85a19d57e
workspace: cdev payload arena and eventfd ownership are implemented; registration, backend wiring, replacement generations, and qualification remain active
---

# M0110 W0112 cdev Payload and Eventfd Stage

## Outcome

The local cdev transport now has a generation-bound driver payload arena and an
explicit event notification ownership boundary. `MF_UAPI_IOCTL_MEMORY_ALLOC`
allocates one page-aligned arena per data-file owner and returns a generated
payload mmap offset. Owner close marks the arena offline; existing VMAs retain a
tombstone and the backing is reclaimed only after their final close. Queue
creation and worker leasing accept one complete caller-owned eventfd pair, hold
kernel `eventfd_ctx` references, and reject a second owner for the generation.
The C17 client exposes eventfd-aware session opening and payload allocation and
close helpers. The generated UAPI records and inherited 64-byte descriptor are
unchanged.

## Verification evidence

| Gate | Result |
|---|---|
| cdev focused tests | Passed: client and worker 2/2 |
| Full development CTest | Passed: 75/75 |
| Transport schema | Passed: 5 definitions / 15 records |
| Target Kbuild | Passed: Linux 6.18.42 GCC built `metaflux_core.ko` with modpost success |
| Agent records before this checkpoint | Passed: 37 sessions / 260 events / 247 Markdown files |
| Commit identity | `Agent Harness (codex)` as Author and Committer for `7ba52c5` |

## Boundary

This is a W0112 implementation stage, not an exit claim. `MEMORY_REGISTER`
remains unsupported pending FOLL_PIN/SG accounting and compatibility shims.
Queue kref/tombstone ownership, daemon-controlled generation replacement,
`mf_backend_api_v1` worker wiring, mapped Add/Copy end to end, and KUnit,
KASAN, KCSAN, lockdep, kmemleak, teardown, and owner-death evidence remain
open. LLVM=1 is still unqualified on the current target configuration.

## Cleanup

- Removed: no source or session-owned disposable files.
- Retained: canonical source and records; Kbuild output remains in the ignored
  kernel build paths owned by the target toolchain.

## roast

### light roasts

- Generation-bound cdev payload arena and client mapping API -> `kernel/core/`,
  `transports/cdev/client/`, and W0112 plan (7ba52c5; CTest 75/75; Kbuild)
- Single-owner eventfd pair and kernel reference lifetime -> `kernel/core/` and
  cdev transport documentation (7ba52c5; Linux 6.18.42 Kbuild)

### medium roasts

- none.

### dark roasts

- none.

## session-only

- LLVM=1 qualification - reason: the current Linux 6.18.42 target Kbuild rejects
  its compiler flags before module compilation; no cross-target conclusion.

## Handoff

Continue W0112 from `7ba52c5`. Keep `MEMORY_REGISTER` and backend integration
explicitly pending until their pinning, ABI, and fault evidence exists. Run the
cdev focused tests, full development CTest, and single-target Kbuild after each
next coherent transport change.
