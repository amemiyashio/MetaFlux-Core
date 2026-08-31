---
status: Active
updated: 2026-08-31
milestone: M0110
workstream: W0112
checkpoint: P20260831-061
---

# Current Progress

Completed milestone: [M0100](../plan/M0100-core-foundation/plan.md), delivery
`0.1.0.0`, product release `v0.1.0`. All workstreams W0101-W0106 are Complete.
Product and delivery identities follow [D0024](../memory/decisions-index.md).
Both foundation and completion sessions are terminal.

Repository collaboration now has an implicit post-delivery convergence
boundary at content revision `faea90189f2b34fb53667e20f843b9bd737c87c6`.
The workflow inventories one exact committed/staged/unstaged/untracked change
set, repairs only integration-owned compatible gaps, routes another active
session through transient guidance, and routes breaking replacements through
semantic-change governance. It creates no snapshot or review archive. This
governance addition initially left product work queued. M0110 is now Active;
W0111 has an implemented candidate contract and remains Active pending the
local/guest negotiation slices. W0112 is Active with its first local cdev stage
recorded at [P20260830-016](checkpoints/2026/P20260830-016-m0110-local-cdev.md).
W0113 is Active with its static vfio-user control-plane stage recorded at
[P20260830-017](checkpoints/2026/P20260830-017-m0110-static-vfio-user.md).
M0120 is now Active for lifecycle implementation. W0121's model stage is
recorded at [P20260830-018](checkpoints/2026/P20260830-018-m0120-lifecycle-model.md).
W0122's runtime-owned coordinator and bounded transport mirror stage is recorded
at [P20260831-019](checkpoints/2026/P20260831-019-m0120-lifecycle-coordinator.md).
The concrete C++ cdev worker and vfio-user server mirrors are now wired to the
coordinator and recorded at [P20260831-020](checkpoints/2026/P20260831-020-m0120-existing-transport-mirrors.md).
W0112's generation-bound payload arena and eventfd ownership stage is recorded
at [P20260831-021](checkpoints/2026/P20260831-021-m0110-cdev-payload-eventfd.md).
W0122's memfd worker lifecycle mirror stage is recorded at
[P20260831-022](checkpoints/2026/P20260831-022-m0120-memfd-lifecycle-mirror.md).
W0122's typed external-event normalizer stage is recorded at
[P20260831-023](checkpoints/2026/P20260831-023-m0120-lifecycle-normalizer.md).
W0122's QMP command/event correlation fixture is recorded at
[P20260831-024](checkpoints/2026/P20260831-024-m0120-qmp-lifecycle-correlation.md).
W0122's stateless runtime event ingress is recorded at
[P20260831-025](checkpoints/2026/P20260831-025-m0120-lifecycle-event-ingress.md).
W0112's bounded registered-memory pin/SG stage is recorded at
[P20260831-026](checkpoints/2026/P20260831-026-m0110-registered-memory.md).
W0112's checked worker-side backend COPY dispatch seam is recorded at
[P20260831-027](checkpoints/2026/P20260831-027-m0110-cdev-backend-dispatch.md).
The CPU backend transport-facing COPY subset and mapped-payload integration are
recorded at [P20260831-035](checkpoints/2026/P20260831-035-m0110-cpu-backend-copy.md).
W0112's queue VMA tombstone backing reaping correction is recorded at
[P20260831-036](checkpoints/2026/P20260831-036-m0110-queue-vma-reap.md).
W0112's owner-death generation tombstone transition is recorded at
[P20260831-037](checkpoints/2026/P20260831-037-m0110-owner-death-tombstone.md).
W0112's queue root/owner/lease/VMA/active-operation kref graph is recorded at
[P20260831-038](checkpoints/2026/P20260831-038-m0110-queue-krefs.md).
W0112's payload root/owner/VMA/active-allocation-operation kref graph is
recorded at [P20260831-039](checkpoints/2026/P20260831-039-m0110-payload-krefs.md).
W0113's compile-checked static guest PCI resource binder is recorded at
[P20260831-040](checkpoints/2026/P20260831-040-m0110-static-guest-pci.md).
M0130 is now Active; W0131's Vulkan capability ABI, opt-in tool epoch, and
truthful host probe are recorded at
[P20260831-041](checkpoints/2026/P20260831-041-m0130-vulkan-capability.md).
W0131's packed BDA argument and external-memory 0.x profiles are recorded at
[P20260831-042](checkpoints/2026/P20260831-042-m0130-vulkan-abi-profiles.md).
W0132's generation-bound staging suballocation and timeline admission model are
recorded at [P20260831-043](checkpoints/2026/P20260831-043-m0130-vulkan-memory-model.md).
W0133's target profile/module preflight diagnostics are recorded at
[P20260831-044](checkpoints/2026/P20260831-044-m0130-vulkan-target-preflight.md).
W0134's stream/dependency planner is recorded at
[P20260831-045](checkpoints/2026/P20260831-045-m0130-vulkan-stream-graph.md).
W0135's deterministic cache identity and bounded catalog model are recorded at
[P20260831-046](checkpoints/2026/P20260831-046-m0130-vulkan-cache-model.md).
W0122's QMP completion-to-ingress helper is recorded at
[P20260831-028](checkpoints/2026/P20260831-028-m0120-qmp-ingress.md).
W0122's vfio-user disconnect handoff is recorded at
[P20260831-029](checkpoints/2026/P20260831-029-m0120-vfio-disconnect-ingress.md).
W0122's automatic vfio-user EOF handoff is recorded at
[P20260831-030](checkpoints/2026/P20260831-030-m0120-vfio-process-ingress.md).
W0121's provider-view invariant hardening is recorded at
[P20260831-031](checkpoints/2026/P20260831-031-m0120-provider-view-invariants.md).
W0121's bounded loss-fence and telemetry-bank publication race model is recorded
at [P20260831-032](checkpoints/2026/P20260831-032-m0120-fence-telemetry-races.md).
W0121's runtime telemetry producer fence/admission guards and marker-complete
recovery target-bank validation are recorded at
[P20260831-033](checkpoints/2026/P20260831-033-m0120-runtime-telemetry-fence-guards.md).
W0122's snapshot-bound producer metadata capture for QMP and vfio-user
disconnect paths is recorded at
[P20260831-034](checkpoints/2026/P20260831-034-m0120-snapshot-bound-event-metadata.md).

Repository-wide replacements of established meaning follow
[D0025](../memory/decisions-index.md). SC0001 remains Applied for that governance
migration; SC0002 replaces only its pre-D0026 promotion-model consequence.
SC0005 is Applied for the M0100 closure-record correction. None is an active
history-edit permit.

[D0026](../memory/decisions-index.md) defines the Verified replacement: only
materially promoted durable claims receive one light, medium, or dark semantic
transformation depth, while `session-only` remains an independent disposition
and `$roast` is explicit-only. Applied SC0002 binds the 68-surface migration to
content revision `991e5327c8a3b1f5d05112f895011f8d83f0bff0`: 58 surfaces were
migrated, three obsolete skill-package paths were removed, and seven evidence
files were retained byte-for-byte. Guidance G002/G004 were adopted by their
target-session owners and both inboxes are empty.

[D0027](../memory/decisions-index.md) and Applied SC0003 now bind Intel x86_64
support qualification plus physical NVIDIA binding-performance promotion to
M1000 / `v1.0.0`. Native NixOS VM/package qualification remains in the
unallocated `v0.2.0` line. G003/G005 were adopted once by their target owners
and their transient packets were removed.

Agent-created Git commits now use the active harness as both Author and
Committer through the `start-work` helper. Revision `ded1dad` proves the Codex
path end to end while leaving the repository-local human identity unchanged;
`record-session` routes content, checkpoint, and closing-record commits through
the same command-local mechanism. Five later M0100 closure commits intended the
`zcode` harness subject but their immutable Git objects record
`amamiya <amamiya@localhost>` for both roles. Applied SC0005 and P013 preserve
the intended and actual identities separately instead of rewriting history.

## Current Boundary

The M0100 core implementation and its generic release route have recorded
passing evidence. D0024's breaking semantic-identity migration is complete in
content revisions `78fc9d8`, `8b79bb0`, and `209caee`: current and historical
M/W/S records, entry points, validators, build targets, and version output now
use one derived coordinate scheme. Historical results remain evidence only for
the revisions and invocations they name.

The following items are not `v0.1.0` blockers:

- Intel x86_64 support qualification (D0027, superseding D0023 only for its
  future destination).
- Physical NVIDIA H2D/D2H and passthrough evidence that promotes provisional
  performance budgets to binding.
- Native NixOS VM/package qualification.
- Vulkan driver-family qualification and the remaining M0130 execution path.

The host-independent W0134 command-resource ownership and completion-gated
recycling model is recorded; actual Vulkan queue submission remains open.
The composed `QueueSubmissionLedger` admission boundary is now recorded at
[P20260831-058](checkpoints/2026/P20260831-058-m0130-vulkan-submit-admission.md);
resource/plan transactionality, completion validation, and generation reset are
covered, while Vulkan object ownership and `vkQueueSubmit2` remain open.

W0133's host-independent SPIR-V reflection and packed-BDA admission contract
is now recorded at [P20260831-053](checkpoints/2026/P20260831-053-m0130-vulkan-module-reflection.md);
actual Kernel IR to MLIR conversion, binary emission/validation, pipeline
creation, and driver execution remain open.

W0132's host-independent staging memory visibility contract is now recorded at
[P20260831-054](checkpoints/2026/P20260831-054-m0130-vulkan-memory-visibility.md);
physical Vulkan allocation, mapped flush/invalidate, external-handle import,
and driver qualification remain open.

W0123's Coordinator control-plane serialization and concurrent replay/observer
evidence is now recorded at
[P20260831-055](checkpoints/2026/P20260831-055-m0120-lifecycle-concurrent-authority.md);
live memfd/cdev/guest-QMP qualification and kernel fault evidence remain open.

W0135's generation-scoped cache pipeline binding boundary is now recorded at
[P20260831-056](checkpoints/2026/P20260831-056-m0130-vulkan-pipeline-binding.md);
actual Vulkan pipeline ownership, identity-mutation misses, warm-launch
exclusion traces, and physical driver qualification remain open.

W0135's complete identity-mutation miss matrix and host-independent warm trace
 admission contract are now recorded at
[P20260831-057](checkpoints/2026/P20260831-057-m0130-vulkan-warm-path.md);
real ICD trace capture, pipeline creation, and physical warm-launch
qualification remain open.

W0134's host-independent queue-submission admission boundary is now recorded at
[P20260831-058](checkpoints/2026/P20260831-058-m0130-vulkan-submit-admission.md);
real command-buffer/pipeline ownership, `vkQueueSubmit2` timeline submission,
provider/runtime composition, and physical driver qualification remain open.

W0131's on-demand Vulkan runtime profile is now recorded at
[P20260831-059](checkpoints/2026/P20260831-059-m0130-vulkan-runtime-tools.md);
`.#vulkan-runtime` provides pinned Mesa ICD and validation layers for host
smoke/probe execution, while the two-driver-family matrix and physical NVIDIA
qualification remain open.

W0132's private Vulkan device context is now recorded at
[P20260831-060](checkpoints/2026/P20260831-060-m0130-vulkan-device-context.md).
It binds a successful capability profile to an exact Vulkan 1.3 physical device,
compute queue, enabled baseline feature chain, and timeline semaphore, and its
empty `vkQueueSubmit2` signals/waits/polls pass on the current AMD RADV host.
Physical allocation, mapped visibility, external-handle import, command-buffer
and pipeline composition, device-loss injection, and driver-family
qualification remain open.

Intel x86_64 support qualification and physical NVIDIA binding-performance
promotion belong to M1000 / `v1.0.0`. Native NixOS VM/package qualification
remains the unallocated `v0.2.0` support expansion. M0100 keeps the measured
performance targets provisional and uses AMD x86_64 as its reference host.

## Recorded M0100 Evidence

| Gate | Recorded result |
| --- | --- |
| Integration CTest | 64/64 passed, including component boundaries, registry recovery, PTX interpreter/compiled differentials, provider ABI, daemon integration, release assertions, and million-noop stress |
| Recovery stress | 50/50 ordinary and 20/20 ASan passed |
| CUDA Add/Copy | Interpreter, cold JIT, warm JIT, and AOT passed |
| Stock `nvidia-smi` / NVML | Supported views and CUDA/NVML identity parity passed |
| Modes and coexistence | Managed, passthrough, auto/fail-open, managed-only, namespace isolation, and recursion prevention passed |
| Optimization and hardening | PGO USE build (135 commands), O2/O3 comparison (28 commands), and ASan/UBSan hardening (121 commands) passed |
| Generic target build | Ubuntu 20.04 SDK route produced `metafluxd` at `GLIBC_2.29`, providers at `GLIBC_2.17` / `GLIBC_2.14`, with no Nix path or RPATH |
| D0012 provider matrix | One recorded run passed 8/8 rows across four distributions and two formats |
| D0012 complete matrix | Two clean same-revision runs passed 8/8 rows, including packaged CUDA Add/Copy |
| Reproducibility | Independent DEB, RPM, and tar builds matched byte-for-byte |
| Signed target SDK provenance | Passed for snapshot `20260820T000000Z`, two signed releases, three indexes, and ten packages |
| D0024 migration verification | Dev build and CTest 63/63; Agent records 111/111; guidance 17/17; routing 68 cases and 23/23 self-tests; 18 skills valid |
| D0025 migration verification | Architecture CTest 6/6; Agent records 136/136; semantic edits 21/21; guidance 17/17; routing 81 cases and 34/34 self-tests; two workflow skills valid |
| Git source identity | Build manifests record 40-hex commit, tree, and clean status |
| Reproducibility (independent rebuilds) | Two independent builds from same revision produce byte-for-byte identical metafluxd, libcuda.so, libnvidia-ml.so, and manifests |
| W0102 stress coverage | All 15 sub-items covered; focused ordinary + sanitizer gates pass |
| Complete D0012 matrix (G006 revision) | 8/8 pass with CUDA Add/Copy acceptance on new revision |
| D0026 migration verification | Architecture CTest 6/6; Agent records 163/163 plus repository 27 sessions/210 events/205 Markdown; semantic edits 21/21; guidance 20/20; routing 82 cases and 34/34 self-tests; roast package and independent A-E forward review passed |
| D0027 migration verification | Architecture CTest 6/6; Agent validator 163/163 plus repository 28 sessions/215 events/213 Markdown at record closure; semantic edits 21/21; guidance 20/20; protected evidence and residual scans passed |
| Agent harness commit identity | Isolated forward test 7/7; both workflow skills valid; real content commit `ded1dad` records Codex as Author and Committer while local Git configuration remains `amamiya` |
| M0100 closure consistency | Record correction `1812617`; candidate-index gates `4d1ff2b` / `9586b45`; Agent records 31 sessions / 242 events / 226 Markdown; self-test 169/169; architecture 6/6; semantic edits 21/21 |
| Collaborator change convergence | Content `faea901`; inventory 15/15; routing 89 cases and 34/34 self-tests; final dev CTest 65/65; two independent reviews converged |

The generic release entry point is checked in at
`tools/build-generic-release.sh` with the CMake-owned Ubuntu 20.04 target tuple.
The package builder enforces the glibc 2.31 ceiling, system dependency closure,
and absence of `/nix/store`, RPATH, and RUNPATH references. The offline D0012
matrix owns fresh install, real upgrade, removal, coexistence, and packaged
Add/Copy evidence for Ubuntu 20.04.6, Ubuntu 22.04.5, Ubuntu 24.04.4, and Rocky
Linux 9.8.

## Recorded M0110 Stage Evidence

| Gate | Recorded result |
| --- | --- |
| W0112 transport schema and component graph | Schema validator passed 4 definitions/12 records; graph passed 15 components/18 edges |
| W0112 focused transport tests | C17/C++20 schema fixtures and cdev client/worker tests passed 5/5 |
| W0112 kernel compile | Linux 6.18.42 default GCC built `metaflux_core.ko` with modpost success |
| W0112 registered-memory stage | `4465732`; one generation-bound range uses `FOLL_LONGTERM`/`FOLL_WRITE` pinning, memlock accounting, SG construction, partial unwind, dirty-unpin, explicit unregister, and owner-close revocation; focused cdev/lifecycle tests 4/4, full CTest 79/79, and Linux 6.18.42 GCC Kbuild passed |
| W0112 backend dispatch seam | Worker-side `CdevBackendBinding` validates `mf_backend_api_v1` size/capability/handles, translates payload COPY to `mf_backend_copy_v1`, maps backend statuses, and rejects malformed bound APIs without fallback; the CPU backend now exposes synchronous canonical-KIR module load/submit with a memory-handle argument block and Add regression. cdev descriptor-to-launch wiring, backend DMA mapping, and daemon replacement remain open |
| W0112 current boundary | Paired rings, negotiation, mapping, wait/poll, VMA ref tracking, exclusive lease, generation-bound payload arena, owner-death offline queue/payload tombstones, queue root/owner/lease/VMA/active-operation krefs, payload root/owner/VMA/active-allocation-operation krefs, eventfd ownership, bounded registered-memory lifetime, checked backend COPY dispatch, and CPU backend synchronous Add/Copy submit are implemented; cdev descriptor launch wiring, registered-memory DMA mapping, daemon replacement, and fault qualification remain open |
| W0113 transport schema and component graph | Schema validator passed 5 definitions/15 records; graph passed 17 components/18 edges |
| W0113 focused transport tests | Schema, cdev, guest, and server tests passed 7/7; full dev CTest passed 72/72 |
| W0113 static vfio-user control fixture | Generated GET_INFO reply, static BAR0/BAR2/BAR4 profile, generation/epoch DMA map ledger, overlap and reset rejection, and `No_reply` unmap passed |
| W0113 static guest PCI binder | `cb118f1`; Linux 6.18.42 GCC Kbuild built `metaflux_pci.ko`, validating CI VID/DID/class and BAR0/BAR2/BAR4 sizes, mapping BAR0/BAR2, reserving two MSI-X vectors, and reversing teardown |
| W0113 current boundary | The static PCI resource binder is recorded; pinned QEMU/libvfio-user, BAR doorbell/MSI-X steady state, guest rings/DMA lifetime, Add/Copy path, drain/tombstone faults, and package qualification remain open |
| W0114 bounded transport fault matrix | `700b7c8`; cdev COPY disposition checks and completion backpressure/FIFO retry, plus vfio-user malformed framing, stale unmap, duplicate-range, and DMA-overflow regressions; focused transport tests 2/2 and full dev CTest 82/82 |
| W0114 current boundary | Malformed and recoverable userspace/socketpair faults are recorded; kernel ioctl/BAR fuzzing, MSI-X, live DMA/backend references, ownership-death injection, native/compat negotiation, and the base ABI freeze remain open |
| W0121 lifecycle model | Extension manifest imports the frozen M0110 root by hash; bounded checker passed 949 states/4,012 transitions/326 complete sequences and 15 direct boundary checks, including CUDA/NVML membership, loss, and reinitialization invariants |
| W0121 fence/telemetry publication model | Separate bounded branch passed 337 states/565 transitions and 6 direct checks for loss-fence precedence, even-latch bank publication, stale `ONLINE` rejection, and bounded reader retry |
| W0121 lifecycle regression | Lifecycle CTest and tampered-manifest self-test passed 2/2; full dev CTest passed 79/79 |
| W0121 runtime telemetry producer guards | Legacy and recovery mappings reject stale/future fence sequences with `MF_SHARED_RETRY`, reject loss with `MF_SHARED_DEVICE_LOST`, and preserve the old bank across marker-complete owner-death recovery; focused registry/recovery CTest passed 2/2 |
| W0121 current boundary | Concrete memfd/cdev/vfio-user mirror stages, typed source mapping, QMP correlation, runtime ingress, provider-view invariants, bounded fence/telemetry publication model, and runtime telemetry producer fence/admission guards are recorded; live provider hooks, live QMP/vPCI integration, three-transport cycle qualification, and lifecycle ABI freeze remain open |

## Recorded M0120 W0122 Evidence

| Gate | Recorded result |
| --- | --- |
| Runtime lifecycle coordinator | `3e89434`; normalized requests, identity/generation high-water, epoch retirement, tombstones, and bounded mirror callbacks |
| Lifecycle focused test | `metaflux.unit.runtime-lifecycle` passed |
| Full development CTest | 75/75 passed |
| W0122 coordinator and transport mirrors | `f5cdee3`; cdev and vfio-user C++ mirrors consume coordinator reset/loss events and reject retired generation/epoch work; focused transport tests 2/2 and full dev CTest 75/75 |
| W0122 memfd lifecycle mirror | `1dbc571`; existing C17 fast path is registered as the client half and a C++ worker mirror stages coordinator-issued identity/generation/epoch, drains in-flight work, and rejects stale submissions; focused test passed and full dev CTest 76/76 |
| W0122 request normalizer | `71956ff`; typed admin/VFIO-user/QMP/disconnect/restart events map to existing lifecycle requests with malformed/unknown rejection; focused normalizer test and full dev CTest 77/77 passed |
| W0122 QMP command/event correlation | `5e1c3bc`; one-pending-command vfio-user fixture requires matching add/delete events, maps failed removal to QMP transport loss, and rejects failed addition; focused QMP/lifecycle regression 3/3 and full dev CTest 78/78 passed |
| W0122 runtime event ingress | `04ecf81`; one stateless runtime entry normalizes external events and submits only accepted requests to Coordinator; malformed/unsupported events leave authority state unchanged; focused dispatch regression and full dev CTest 79/79 passed |
| W0122 QMP producer ingress | `b37e8ba`; QMP completion snapshots the correlated event, maps failed remove to `QmpFailure`, and submits through the stateless ingress; focused QMP/lifecycle dispatch 2/2 and full dev CTest 79/79 passed |
| W0122 vfio-user disconnect ingress | `ee0ecab`; EOF/error handoff marks the local server lost and submits a captured `Disconnect` event through the stateless ingress; focused server/dispatch 2/2 and full dev CTest 79/79 passed |
| W0122 vfio-user process ingress | `699cff8`; `process_once` overload invokes the disconnect handoff only for `Closed`, preserving ordinary message results; focused server/dispatch 2/2 and full dev CTest 79/79 passed |
| W0122 snapshot-bound event metadata | QMP command factory and vfio-user `process_once` capture logical device, daemon, identity, generation, epoch, and deadline from the authority snapshot; stale completion remains `Stale`; focused normalizer/QMP/server tests passed 3/3 |
| W0122 current boundary | Live QMP/socket command transport, reset/restart producer metadata binding, production memfd worker wiring, provider freeze, fault injection, and qualification remain open; QMP and vfio-user disconnect capture are covered by the snapshot-bound helper |

## Recorded M0120 W0123 Evidence

| Gate | Recorded result |
| --- | --- |
| Lifecycle authority capacity | `72025af`; 4,096 request replay records and 2,048 immutable tombstones sized for the W0123 1,000-cycle envelope without eviction |
| Lifecycle core long run | `72025af`; 1,000 reset/remove/add cycles passed, with 2,000 retired generations resolving as `DeviceLost`, exact high-water/epoch checks, all three mirror streams, and replay/conflict checks |
| Focused lifecycle tests | Four lifecycle, long-run, normalizer, and dispatch tests passed |
| Full development CTest | 83/83 passed |
| W0123 authority serialization | `f8a786a`; public Coordinator mutation/read APIs share one reentrant mutex; move construction preserves existing factories |
| W0123 concurrent replay/activity | `f8a786a`; four-way duplicate reset race plus open/mmap/submit-replay/telemetry observers over 128 reset/remove/add cycles passed |
| W0123 current boundary | Core authority long-run evidence is recorded; concurrent memfd/local-cdev/guest-QMP activity, old-object/worker-death faults, kernel sanitizer/fuzz/soak, canonical nodes, and lifecycle ABI freeze remain open |

## Recorded M0130 W0131 Evidence

| Gate | Recorded result |
| --- | --- |
| Vulkan capability ABI | `837619a`; fixed-width C profile with status, API/driver/device identity, queue, subgroup, memory-tier, UUID, target-environment, and digest fields; C layout test passed |
| Vulkan packed arguments and memory profiles | `21ed444`; 64-byte/48-byte target-digest-bound argument layout and 96-byte external-memory 0.x tier profile; positive and negative C fixtures passed |
| Vulkan host probe | Optional C++20 probe requires Vulkan 1.3 compute, timeline semaphores, Synchronization2, buffer device address, and a compute queue; unavailable host reports `no-device` without qualification |
| Vulkan tool epoch | `toolchains/vulkan-1.json` and `.#vulkan` expose Vulkan headers/loader/tools, glslang, and SPIR-V Tools at the locked nixpkgs versions |
| Vulkan runtime smoke profile | `836c6e3`/`6c7c3ff`; `vulkan-runtime-1.json`, `vulkan-runtime.nix`, and `.#vulkan-runtime` expose Mesa 26.1.8 plus Khronos validation layers for local RADV/lavapipe smoke without changing the lean tool shell |
| Vulkan CTest | Full `vulkan` preset passed 81/81, including ABI and capability regressions |
| W0131 current boundary | Exact feature/limit minimums, two driver families, packed BDA/external-memory fixtures, lowering, execution, caches, and lifecycle integration remain open |

## Recorded M0130 W0132 Evidence

| Gate | Recorded result |
| --- | --- |
| W0132 staging ledger | `67eaf28`; capability-backed first-fit suballocation with power-of-two alignment, non-overlap, release/reuse, and generation-bound validation |
| W0132 timeline model | `67eaf28`; monotonic submit/complete/wait admission with stale-generation and future-value rejection |
| W0132 CTest | Full `vulkan` preset passed 84/84, including `metaflux.backend.vulkan-memory` |
| W0132 visibility ledger | `96d3a55`; generation-bound host/device dirty ranges, atom-size range checks, explicit flush/invalidate, timeline admission, and in-flight teardown guards |
| W0132 CTest after visibility stage | Full `vulkan` preset passed 88/88, including coherent/non-coherent and partial-range visibility fixtures |
| W0132 device context | `8afbe2a`; private profile-matched Vulkan 1.3 instance/device/compute queue/timeline context with generation-checked empty `vkQueueSubmit2` signals, waits, polls, and reset |
| W0132 CTest after device-context stage | Lean Vulkan preset passed 89/89; explicit RADV runtime device CTest passed 1/1 with queue-family=0 and timeline=2 |
| W0132 current boundary | Physical `VkDeviceMemory`, mapped Vulkan flush/invalidate, external-handle import, command-buffer/pipeline composition, cross-process synchronization, lifecycle drain, and driver qualification remain open |

## Recorded M0130 W0133 Evidence

| Gate | Recorded result |
| --- | --- |
| W0133 target preflight | `86441c7`; queried profile, required features, target digest, workgroup limits, known address spaces, BDA intent, and subgroup constraints are validated before module creation |
| W0133 diagnostics | Stable statuses cover invalid profile/module, unsupported features/semantics, target mismatch, and limit exceeded; positive and negative fixtures pass |
| W0133 CTest | Full `vulkan` preset passed 85/85, including `metaflux.backend.vulkan-target-preflight` |
| W0133 reflection contract | `d25617a`; compute entry/model, target digest, workgroup, feature/address-space parity, LocalInvocationId, Workgroup storage, and packed-BDA size are checked before shader-module creation |
| W0133 CTest after reflection stage | Full `vulkan` preset passed 88/88, including target-preflight reflection and BDA negatives |
| W0133 current boundary | Kernel IR to MLIR SPIR-V conversion/emission, actual `spirv-val`, pipeline creation, execution, and dual-driver differential evidence remain open |

## Recorded M0130 W0134 Evidence

| Gate | Recorded result |
| --- | --- |
| W0134 stream graph | `c3b8c5e`; monotonic plans preserve per-stream FIFO, keep independent streams unordered, and require explicit cross-stream waits |
| W0134 visibility | Copy plans require transfer masks; launch plans require compute masks; invalid masks and unknown operations are rejected |
| W0134 CTest | Full `vulkan` preset passed 86/86, including `metaflux.backend.vulkan-stream-graph` |
| W0134 command-resource pool | `d9e1ef9`; finite generation-bound resources use monotonic identities and completion timelines; recycling waits for observed completion and reconfigure rejects in-flight resources |
| W0134 CTest after command-resource stage | Full `vulkan` preset passed 88/88, including `metaflux.backend.vulkan-stream-graph` |
| W0134 queue-submission ledger | `e272a1d`; `QueueSubmissionLedger` composes graph/resource admission, cancels rejected plans, assigns monotonic generation-bound completion values, and resets both authorities only after in-flight work drains |
| W0134 CTest after queue-admission stage | Focused stream-graph CTest passed 1/1; full `vulkan` preset passed 88/88 |
| W0134 current boundary | `vkQueueSubmit2`, pipeline creation, timeline semaphore wiring, composed provider/runtime dependencies, validation, and driver-family execution remain open |

## Recorded M0130 W0135 Evidence

| Gate | Recorded result |
| --- | --- |
| W0135 cache identity | `073376d`; deterministic portable/device keys include compiler/lowering/tool epochs, target and specialization digests, ABI fields, and device/driver UUIDs |
| W0135 catalog | `073376d`; bounded publication, hit/miss, corrupt unpinned removal, live-reference pinning, LRU eviction, and quota exhaustion are tested |
| W0135 CTest | Full `vulkan` preset passed 87/87, including `metaflux.backend.vulkan-cache-model` |
| W0135 filesystem publication | `4959da6`; portable/device-bound envelopes use complete-key, mode, length, and digest validation with fsync/atomic rename; malformed entries are removed and device invalidation is explicit |
| W0135 CTest after filesystem stage | Full `vulkan` preset passed 88/88, including filesystem corruption and invalidation regressions |
| W0135 persistent repository | `e1288c1`; resident-first lookup hydrates validated disk entries, catalog admission protects pinned/quota files, and serialized pin/unpin/device invalidation keep both layers consistent |
| W0135 CTest after repository stage | Full `vulkan` preset passed 88/88, including cross-instance hydration, quota, pin, and invalidation regressions |
| W0135 cross-process coordination | `246f84b`; stable per-key `flock` plus second lookup coalesces concurrent producers across processes; lock timeout is explicit `io-error` |
| W0135 CTest after stampede stage | Cache CTest passed five repeat runs; full `vulkan` preset passed 88/88 |
| W0135 pipeline binding | `c37d312`; resident device-bound hits have one generation-scoped pipeline binding, stale/duplicate generations are rejected, and release gates device invalidation |
| W0135 CTest after pipeline binding | Full `vulkan` preset passed 88/88, including `metaflux.backend.vulkan-cache-model` |
| W0135 identity mutation and warm trace | `5b1a304`; all portable/device-bound identity fields miss when changed, and the exact four-event warm allowlist rejects producer/validation/module/pipeline/allocation events |
| W0135 CTest after warm trace stage | Focused cache CTest passed 1/1; full `vulkan` preset passed 88/88 |
| W0135 current boundary | Opaque `VkPipelineCache`, actual pipeline ownership, ICD warm trace, and physical driver qualification remain open |

## Versioned Next Work

1. M0100, its foundation session, and its completion session are terminal; the
   Applied SC0005 record correction does not reopen product scope or lifecycle.
2. M0110 is Active. W0111's schema stage is recorded in
   [S0111](../sessions/2026/08/S0111-20260830-012-m0110-abi-contract/summary.md),
   and its remaining negotiation gate continues with W0112/W0113. W0113's
   generated control-plane and static PCI binder stages are recorded, while both
   transport workstreams remain Active until their data-plane and fault gates
   pass. Start each
   transport workstream through its own active session and matching runtime,
   Linux UAPI, vfio-user, PCI, and performance skills.
3. New work uses an explicit four-part delivery coordinate and the derived
   M/W/S identity; no pre-D0024 alias is accepted.
4. After any durable collaborator delivery, converge its exact change set
   before the next work unit; checkpoint or close is the missed-boundary
   fallback.
5. Schedule Intel x86_64 support and physical NVIDIA binding performance under
   M1000 / `v1.0.0`; keep native NixOS VM/package qualification in the
   unallocated `v0.2.0` expansion. Do not reopen M0100 for any of them.
6. M0120/W0122 is Active. The coordinator, memfd/cdev/vfio-user mirrors, typed
   request normalizer, QMP completion ingress helper, vfio-user disconnect
   handoff, automatic process ingress, and runtime ingress are recorded, but
   they do not satisfy the lifecycle Definition of Done; continue with live
   QMP/socket command transport, reset/restart producer metadata binding,
   production memfd integration, provider freeze, and qualification while
   preserving the M0110 root. Resume from
   [P20260831-030](checkpoints/2026/P20260831-030-m0120-vfio-process-ingress.md).
7. M0120/W0121's bounded model remains Active. Provider-view invariants and
   CUDA/NVML reinitialization checks are recorded at
   [P20260831-031](checkpoints/2026/P20260831-031-m0120-provider-view-invariants.md),
   the loss-fence/telemetry publication race model is recorded at
   [P20260831-032](checkpoints/2026/P20260831-032-m0120-fence-telemetry-races.md),
   and runtime telemetry producer fence/admission guards plus marker recovery
   validation are recorded at
   [P20260831-033](checkpoints/2026/P20260831-033-m0120-runtime-telemetry-fence-guards.md);
   continue with live provider hooks, transport integration, fault
   qualification, and lifecycle ABI freeze.
8. M0110/W0112 remains Active after the bounded registered-memory stage at
   [P20260831-026](checkpoints/2026/P20260831-026-m0110-registered-memory.md) and
   the backend dispatch seam at
   [P20260831-027](checkpoints/2026/P20260831-027-m0110-cdev-backend-dispatch.md).
   The CPU backend's synchronous COPY subset now consumes an imported mapped
   payload and the payload kref stage at
   [P20260831-039](checkpoints/2026/P20260831-039-m0110-payload-krefs.md);
   continue with cdev descriptor-to-launch mapping, generation-bound
   registered-memory/DMA mapping, backend references, replacement generations,
   and lifecycle/fault qualification. The CPU backend's standalone synchronous
   Add/Copy submit is recorded at [P20260831-061](checkpoints/2026/P20260831-061-m0110-cpu-backend-launch.md);
   do not claim the cdev exit gate from that backend test or the mapped COPY
   fixture alone.
9. M0130/W0131 is Active after the capability ABI stages at
   [P20260831-041](checkpoints/2026/P20260831-041-m0130-vulkan-capability.md) and
   [P20260831-042](checkpoints/2026/P20260831-042-m0130-vulkan-abi-profiles.md).
   Continue with the exact target baseline, then move to W0132/W0133 without
   claiming physical driver-family support from the current host's `no-device`
   probe.
10. M0130/W0132 is Active after the host-independent memory model stage at
    [P20260831-043](checkpoints/2026/P20260831-043-m0130-vulkan-memory-model.md).
    Continue with qualified-device allocation and synchronization evidence;
    keep the staging model separate from physical Vulkan qualification.
11. M0130/W0133 is Active after the target-preflight stage at
    [P20260831-044](checkpoints/2026/P20260831-044-m0130-vulkan-target-preflight.md).
    Continue with MLIR/SPIR-V conversion and reflection fixtures only after the
    preflight admission remains the first module boundary.
12. M0130/W0134 is Active after the stream-graph stage at
    [P20260831-045](checkpoints/2026/P20260831-045-m0130-vulkan-stream-graph.md).
    Continue with command resources and actual queue submission only after the
    stream planner remains the dependency admission boundary.
13. M0130/W0135 is Active after the cache-model stage at
    [P20260831-046](checkpoints/2026/P20260831-046-m0130-vulkan-cache-model.md).
    Continue with filesystem/pipeline integration only after preserving the
    deterministic key and live-reference ownership rules.
14. M0110/W0114 is Active after the bounded transport fault stage at
    [P20260831-047](checkpoints/2026/P20260831-047-m0110-transport-fault-matrix.md).
    Continue with kernel/BAR/DMA ownership injection and native/compat
    qualification; do not claim the transport ABI freeze from userspace
    socketpair coverage alone.
15. M0120/W0123 is Active after the host-independent lifecycle authority run at
    [P20260831-048](checkpoints/2026/P20260831-048-m0120-lifecycle-core-long-run.md).
    Continue by binding the 1,000-cycle core evidence to concurrent
    memfd/cdev/guest-QMP activity, old-object tombstones, worker-death faults,
    and kernel/package qualification; do not claim the three transport suites
    or lifecycle extension freeze from the core fixture alone.
16. M0130/W0135 is Active after filesystem publication at
    [P20260831-049](checkpoints/2026/P20260831-049-m0130-vulkan-cache-filesystem.md).
    Continue by integrating `CacheFileStore` with catalog residency and quota,
    then add cross-process stampede and pipeline-bound device invalidation;
    do not claim Vulkan pipeline or warm-launch qualification from file tests.
17. M0130/W0134 is Active after the host-independent command-resource stage at
    [P20260831-050](checkpoints/2026/P20260831-050-m0130-vulkan-command-recycling.md).
    Continue by binding completion-gated resources to pipeline creation and
    actual `vkQueueSubmit2` timeline submission; do not claim physical Vulkan
    execution or driver qualification from the pool fixture.
18. M0130/W0135 is Active after process-local repository integration at
    [P20260831-051](checkpoints/2026/P20260831-051-m0130-vulkan-cache-catalog.md).
    Continue with cross-process single-key coordination and pipeline-bound
    device/driver invalidation; do not claim warm-launch qualification from
    catalog/file hydration tests.
19. M0130/W0135 is Active after cross-process miss coalescing at
    [P20260831-052](checkpoints/2026/P20260831-052-m0130-vulkan-cache-stampede.md).
    Continue by binding invalidation to a real pipeline identity and proving the
    warm path's compiler/validator/allocation exclusions.
20. M0130/W0133 is Active after the host-independent reflection/packed-BDA stage
    at [P20260831-053](checkpoints/2026/P20260831-053-m0130-vulkan-module-reflection.md).
    Continue with Kernel IR to MLIR SPIR-V lowering and exact binary validation;
    do not claim shader emission or driver qualification from reflection fixtures.
21. M0130/W0132 is Active after the host-independent visibility stage at
    [P20260831-054](checkpoints/2026/P20260831-054-m0130-vulkan-memory-visibility.md).
    Continue by binding the ledger to qualified-device allocation and mapped
    flush/invalidate calls; do not claim physical Vulkan or external-memory
    qualification from the model fixtures.
22. M0120/W0123 is Active after Coordinator control-plane serialization and
    concurrent replay/observer evidence at
    [P20260831-055](checkpoints/2026/P20260831-055-m0120-lifecycle-concurrent-authority.md).
    Continue by binding the envelope to real memfd/cdev/guest-QMP activity,
    worker and process-death faults, and kernel qualification; do not claim the
    lifecycle extension freeze from host-independent API tests.
23. M0130/W0135 is Active after generation-scoped cache pipeline binding at
    [P20260831-056](checkpoints/2026/P20260831-056-m0130-vulkan-pipeline-binding.md).
    Continue with actual Vulkan pipeline identity, key-mutation miss proofs,
    and warm-launch compiler/validator/module/pipeline/allocation exclusion;
    do not claim physical driver qualification from the repository contract.
24. M0130/W0135 is Active after the complete identity-mutation and
    host-independent warm-trace stage at
    [P20260831-057](checkpoints/2026/P20260831-057-m0130-vulkan-warm-path.md).
    Continue with a real ICD trace around `VkPipeline` ownership and preserve
    the explicit physical-driver qualification boundary.
25. M0130/W0134 is Active after the host-independent `QueueSubmissionLedger`
    admission stage at
    [P20260831-058](checkpoints/2026/P20260831-058-m0130-vulkan-submit-admission.md).
    Continue with real command-buffer/pipeline ownership, `vkQueueSubmit2`
    timeline signaling, and M0110 completion integration; preserve the boundary
    against physical Vulkan claims until a qualification host is available.
26. M0130/W0131 is Active after the on-demand `vulkan-runtime` profile at
    [P20260831-059](checkpoints/2026/P20260831-059-m0130-vulkan-runtime-tools.md).
    Use it for local RADV/lavapipe smoke and capability probes; keep the exact
    two-driver-family baseline and physical NVIDIA qualification open.
27. M0130/W0132 is Active after the private Vulkan device-context stage at
    [P20260831-060](checkpoints/2026/P20260831-060-m0130-vulkan-device-context.md).
    Continue by attaching the context to the staging ledger and real
    `VkDeviceMemory` allocation, mapped flush/invalidate, and M0110 completion
    operations; keep external-handle, device-loss, command-buffer, and
    two-driver qualification gates open.

## Tool Boundary

Nix fixes and exposes declared tool versions only. Git owns source identity;
agent-run commits use command-local harness identity through `start-work`, while
human Git configuration remains untouched. CMake/Ninja own builds; CTest and
repository harnesses own validation; `packaging/` owns artifacts; sessions own
compact work records and exact cleanup; host operators own Nix-store retention
and garbage collection (D0022).
