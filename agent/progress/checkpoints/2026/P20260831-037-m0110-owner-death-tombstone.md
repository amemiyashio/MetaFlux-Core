---
id: P20260831-037
status: Recorded
captured: 2026-08-31
milestone: M0110
workstream: W0112
branch: main
git_revision: 4a502b5
workspace: cdev queue and worker owner close now transitions the current generation to an offline tombstone and wakes waiters; replacement generations remain open
---

# M0110 W0112 Owner-Death Tombstone

## Outcome

The cdev broker now treats queue-owner and worker-lease close as terminal loss
of the current generation. Under the cdev lock, the first closing owner marks
the queue offline and wakes waiters; existing VMAs remain valid tombstones until
their final close, while new negotiation, queue creation, allocation, and lease
operations are rejected by the existing offline checks. The generation is not
reused until daemon-controlled replacement is implemented.

## Verification evidence

| Gate | Result |
|---|---|
| Target Kbuild | Passed: Linux 6.18.42 GCC built `metaflux_core.ko` and modpost completed |
| Full development CTest | Passed: 79/79 through `nix develop . --command ctest --preset dev` |
| Formatting and diff checks | Passed: `git diff --check` |
| Commit identity | `Agent Harness (codex)` as Author and Committer for content `4a502b5` |

## Boundary

This checkpoint closes the owner-death transition for the current static cdev
fixture. It does not claim a dynamic replacement generation, queue krefs or
backend reference draining, production Add/launch, or KUnit/KASAN/KCSAN/lockdep/
kmemleak qualification.

## Cleanup

- Removed: none; ignored Kbuild output remains under the external build owner.
- Retained: owner-death transition, transport documentation, and this checkpoint.

## roast

### light roasts

- Owner-close generation tombstone -> `kernel/core/metaflux_core_main.c`
  (content `4a502b5`; Linux 6.18.42 GCC Kbuild and full CTest 79/79)
- Transport owner-death rule -> `transports/cdev/README.md` (content `4a502b5`)

### medium roasts

- none.

### dark roasts

- none.

## session-only

- The target kernel emitted a compiler minor-version mismatch warning; this is
  local build evidence and does not change the supported toolchain policy.

## Handoff

Resume S0112/W0112 with queue object references, backend in-flight draining,
and daemon-controlled generation replacement. Keep the existing offline checks
and VMA tombstones; do not infer replacement safety from owner-death marking.
