# Session Summary

## Objective and outcome

Own the current-epoch M0110/W0112 product focus after destructive SC0007
settlement. The session begins from the current W0112 plan, current source, and
current checkpoints only; no liquidated session detail is an execution input.
The product Exit Gate remains open.

## Durable changes

- `agent/progress/focus.json`: product focus transferred atomically to this
  schema version 2, D0029 owner.
- `agent/progress/current.md`: current W0112 boundary and next actions replace
  the completed governance migration sequence.
- `services/metafluxd/server.cpp`: embedded daemon object-table memory objects
  now bind to persistent full-range CPU backend handles; resolver imports have
  explicit retain/release accounting, object retirement drains active
  references, and daemon teardown reclaims remaining handles.
- `transports/cdev/worker/include/metaflux/transport/cdev_worker.hpp` and
  `transports/cdev/worker/src/worker.cpp`: the worker can negotiate the current
  view and generation on the same control fd as its lease, map the exact
  data-owner payload arena through that lease, and release payload and queue
  VMAs before lease revocation.
- `kernel/core/metaflux_core_main.c`: a valid worker lease fd may map the online
  payload arena while the data fd remains its owner; the existing kref/VMA
  tombstone graph is preserved.
- `contracts/uapi/linux/v1/schema/uapi.json` and its manifest closure:
  `MF_UAPI_IOCTL_MEMORY_QUERY` reuses the fixed memory record to publish the
  current payload generation, exact size, and mmap offset to a negotiated
  worker lease. `map_current_payload()` consumes it without a local size
  convention.
- The base transport manifest and lifecycle extension manifest were synchronized
  to the new UAPI digest; the lifecycle model gate remains valid after the
  contract update.
- `kernel/core/metaflux_core_main.c`: the lease and negotiation checks for
  `MF_UAPI_IOCTL_MEMORY_QUERY` now execute under `mf_cdev_lock`, matching the
  release path that clears those fields and removing a KCSAN-visible read/write
  window.
- `kernel/core/metaflux_core_main.c`: all cdev ioctl, mmap, and poll reads of
  mutable per-file negotiation, lease, queue, and registered-memory
  authorization state now execute under `mf_cdev_lock`, matching close and
  teardown updates while preserving errno and resource-unwind behavior.
- `0404481`: the D0030 provider path now
  attempts cdev first, binds its data queue to the matching Unix session,
  view, and generation before committing initialization, and limits memfd
  fallback to the recorded pre-success unsupported cases.
- `services/metafluxd/server.cpp`: a bound cdev worker now consumes the leased
  queue beside the Unix control worker, imports canonical KIR modules into the
  CPU backend, resolves daemon object-table COPY and primary-entry LAUNCH
  descriptors, and retains backend memory references for the operation lifetime.
- `transports/cdev/worker/`: launch resolutions now carry bounded backend
  memory references with synchronous and asynchronous cleanup; the worker
  regression asserts balanced launch reference retention.
- `transports/cdev/README.md`, `kernel/core/README.md`, and the W0112 plan:
  record the new mapping boundary and keep daemon live lease/import,
  generation replacement, and kernel qualification explicitly open.
- `kernel/tests/kselftest/cdev_qualification.c`: an executable live gate now
  covers the generated ioctl/mmap ABI, eventfd worker lease, payload query and
  mapping, non-aligned long-term registration/unregister, malformed and stale
  requests, unknown ioctl rejection, and owner-close VMA tombstones. CTest
  records missing device nodes as a skip rather than a pass.
- The current source audit confirms the live product seam is still unconnected:
  `Session::serve()` selects only `EmbeddedCpuWorker`, standalone cdev region
  descriptors require a daemon object table that the cdev client does not
  create or bind, and the CPU backend import helper consumes host virtual
  ranges rather than kernel registered-memory handles. The open M0110 provider
  cdev selection/fallback decision is therefore a hard gate before provider
  wiring.

## Verification

| Command/gate | Result |
| --- | --- |
| D0029 handoff record gate | Passed before the atomic owner transfer commit |
| Current focus projection | Passed for M0110/W0112 and its canonical Exit Gate |
| Daemon cdev backend content commit | Passed: `0bba77f87c8f0737513a6873eb8325af54ff4852` |
| Worker/kernel payload mapping content commit | Passed: `fcfaa8d` |
| Lease-bound payload query content commit | Passed: `5050915992771444398879212e37529b44b704ce` |
| Manifest closure checkpoint commit | Passed: `cd8d06d` |
| Query lease-check serialization content commit | Passed: `c9682ff` |
| Per-file cdev state serialization content commit | Passed: `780222c` |
| Development build | Passed with `METAFLUX_DAEMON_CDEV_BACKEND=1` |
| Focused cdev/daemon CTest | Passed: 6/6 |
| Full development CTest | Passed: 85/85 |
| Linux 6.18 kernel module Kbuild | Passed with `/usr/bin/gcc`; compiler-version warning only |
| Cdev worker launch reference regression | Passed: 1/1 |
| Provider-only cdev-disabled build and CTest | Passed: build and 40/40 |
| Full development CTest after provider/daemon binding | Passed: 85/85 |
| Full development CTest after live harness | Passed: 85 executed; live cdev qualification skipped because `/dev/metaflux0` is absent |
| Linux 6.18 core Kbuild in repository Nix environment | Passed: compile, modpost, and BTF; compiler-version warning only |
| Schema and lifecycle manifest closure | Passed: schema validator and lifecycle model checks |
| Agent records | Passed before checkpoint-record commit |

## Cleanup

- Removed: none.
- Retained: current W0112 source, plan, checkpoints, and live qualification
  requirements; no old-epoch session detail.

## Decisions and experience

- D0029 and Applied SC0007 require all work to use current canonical files and
  the one current focus owner.
- The embedded daemon object table owns one full-range backend memory handle
  per object; operation references are explicit, temporary resolver handles
  are reclaimed after the operation, and retired persistent handles reclaim
  only after active references drain. This does not establish live `/dev`
  cdev import or kernel qualification.
- The worker discovers the current cdev view and generation on the same control
  fd used for its lease, avoiding a discovery/reopen race. The worker lease is
  a second mapping authority for the data-owner payload, while the data fd
  retains ownership and the payload VMA retains the offline tombstone.
- Payload size is queried through the canonical fixed memory record on the
  lease fd; worker mapping does not depend on an environment or local-size
  convention.
- Mutable cdev file authorization state is read under the same lock used by
  close and teardown. Long pin/map operations still unwind outside the lock,
  and commit-time checks prevent an authorization or generation decision from
  crossing the publication boundary.
- D0030 is now implemented at the provider/daemon boundary: Unix control
  operations create and retire objects, while cdev owns steady-state COPY and
  primary-entry LAUNCH only after the daemon has leased and bound the matching
  queue. The CPU backend mapping is an explicit host-memory import fixture;
  it is not evidence of kernel registered-memory DMA.

## roast

### light roasts

- none.

### medium roasts

- Embedded daemon object-table backend handles with operation-reference
  draining -> `services/metafluxd/server.cpp` (`0bba77f`; focused CTest 6/6,
  full CTest 85/85)
- Same-fd current cdev discovery and lease-bound payload mapping ->
  `transports/cdev/worker/include/metaflux/transport/cdev_worker.hpp`
  (`fcfaa8d`; full CTest 85/85; Linux 6.18 Kbuild)
- Lease-bound payload query and exact-size mapping ->
  `contracts/uapi/linux/v1/schema/uapi.json`
  (`5050915992771444398879212e37529b44b704ce`; schema/lifecycle checks, full
  CTest 85/85, Linux 6.18 Kbuild)
- Cdev per-file authorization checks serialized with close/teardown ->
  `kernel/core/metaflux_core_main.c` (`780222c`; full CTest 85/85, Linux 6.18
  Kbuild)
- D0030 cdev-first provider selection and same-session binding ->
  `agent/plan/M0110-kernel-guest-transport/plan.md` (implementation evidence:
  `contracts/protocol/client/v1/include/metaflux/client/protocol.h` and
  `plugins/compat/cuda/abi/driver/src/provider.c`; `0404481`; full CTest 85/85;
  provider-only cdev-disabled CTest 40/40)
- Bound daemon cdev worker with CPU COPY/LAUNCH object-table resolution ->
  `agent/plan/M0110-kernel-guest-transport/work/W0112-local-cdev.md`
  (implementation evidence: `services/metafluxd/server.cpp` and
  `transports/cdev/worker/src/worker.cpp`; `0404481`; focused worker CTest 1/1;
  full CTest 85/85; Linux 6.18 Kbuild)
- Executable live cdev UAPI and VMA qualification gate ->
  `agent/plan/M0110-kernel-guest-transport/work/W0112-local-cdev.md`
  (implementation evidence: `kernel/tests/kselftest/cdev_qualification.c`;
  `95c960d`; 85 executable CTest passes plus one explicit missing-device skip)

### dark roasts

- none.

## session-only

- none.

## Unresolved items

- W0112: live `/dev/metafluxN` Add/Copy, replacement-generation isolation,
  daemon-side lease/import wiring, and Linux 6.12/6.18 fault qualification
  remain open; the worker-side payload-size query and cdev per-file state
  serialization are now available.
- M0100 fallback diagnostics and provider cdev selection are now canonical and
  implemented. The remaining W0112 boundary is live cdev lease/query use,
  registered-memory/DMA backend import, generation replacement, and the
  Linux fault matrix.

## Handoff

Resume from `agent/progress/focus.json`, `agent/progress/current.md`, and the
W0112 Exit Gate. The next unit is daemon-side live cdev lease/import wiring;
do not seek task context in `agent/sessions/liquidated-v1.json` or Git history.
