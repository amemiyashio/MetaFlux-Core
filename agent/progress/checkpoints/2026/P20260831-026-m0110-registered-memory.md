---
id: P20260831-026
status: Recorded
captured: 2026-08-31
milestone: M0110
workstream: W0112
branch: main
git_revision: 4465732f394562f01c520894110ae3aaff87ba5d
workspace: bounded registered-memory pin/SG lifetime is implemented; backend mapping and ownership qualification remain active
---

# M0110 W0112 Registered Memory Stage

## Outcome

The local cdev transport now implements one generation-bound caller-owned
`MEMORY_REGISTER` range up to 64 MiB. The kernel validates the fixed UAPI record,
charges the current process's memlock quota, pins full pages with
`pin_user_pages_fast()` using `FOLL_LONGTERM` and optional `FOLL_WRITE`, and
builds an SG table. Partial pins unwind exactly once. Explicit unregister and
owner close first remove the live object under the cdev lock, then free the SG
table, dirty-unpin device-written pages, release the memlock charge, and drop the
mm reference outside the lock. The C17 client exposes a generation-aware
registration and close path, while retaining the original `reserved` field as a
source-compatible alias for client metadata.

## Verification evidence

| Gate | Result |
|---|---|
| `metaflux.transport.cdev-client` and `metaflux.transport.cdev-worker` | Passed: 2/2 |
| Lifecycle model and self-test | Passed: 2/2 after synchronizing the base-manifest import hash |
| Focused cdev/lifecycle selection | Passed: 4/4 |
| Full development CTest | Passed: 79/79 |
| Transport schema | Passed: 5 definitions / 15 records |
| Target Kbuild | Passed: Linux 6.18.42 GCC built `metaflux_core.ko` with modpost success |
| Agent records | Passed: 37 sessions / 272 events / 253 Markdown files |
| Commit identity | `Agent Harness (codex)` as Author and Committer for `4465732` |

## Boundary

This is a bounded registration and lifetime stage, not the W0112 exit gate. The
fixture deliberately publishes one global range and no backend `dma_map_sg`,
worker/backend reference, multi-region quota, daemon generation replacement,
queue kref/tombstone implementation, or KUnit/KASAN/KCSAN/lockdep/kmemleak and
owner-death qualification. LLVM=1 remains unqualified on the current target
configuration.

## Cleanup

- Removed: no session-owned disposable artifact; ignored Kbuild outputs remain
  under the target kernel build path.
- Retained: canonical UAPI/schema, kernel/client source, W0112 plan, and compact
  session/checkpoint records.

## roast

### light roasts

- Bounded registered-memory UAPI direction and lifetime ->
  `contracts/uapi/linux/v1/README.md` and `contracts/uapi/linux/v1/schema/uapi.json`
  (content revision `4465732`; schema validator and full CTest)
- FOLL_LONGTERM/FOLL_WRITE pin, SG construction, memlock charge, and exact unwind
  -> `kernel/core/metaflux_core_main.c` (Linux 6.18.42 GCC Kbuild; focused cdev
  regression)
- Generation-aware client registration and unregister ->
  `transports/cdev/client/include/metaflux/transport/cdev.h` and
  `transports/cdev/client/src/cdev.c` (cdev client test; full CTest 79/79)

### medium roasts

- none.

### dark roasts

- none.

## session-only

- LLVM=1 qualification - reason: the current target Kbuild rejects its compiler
  flags before module compilation; no cross-target conclusion is promoted.

## Handoff

Continue S0112/W0112 from `4465732`. Keep the registration fixture bounded to one
generation and one range until backend DMA direction/mapping and in-flight
reference ownership are specified. Next implementation work is queue kref and
tombstone ownership, daemon-controlled generation replacement, backend
`mf_backend_api_v1` Add/Copy wiring, and dynamic teardown/fault qualification.
