# MetaFlux Project Work Sessions

This directory contains compact repository work ledgers. A session records its
objective, material decisions and results, durable changes, cleanup, and resume
point. Git owns source history; sessions do not archive worktree snapshots,
build directories, dependency stores, downloads, or routine command output.

It does not archive conversations, user profiles, personal preferences, or work
from another project. Every retained event must materially change a decision,
verification claim, or handoff. Failed routes are deleted at the session
boundary unless one concise lesson is needed to prevent repetition.

Sessions use the date hierarchy
`sessions/YYYY/MM/S<delivery>-YYYYMMDD-NNN-slug/`. The semantic prefix is
derived from `session.json.delivery`; the date and sequence make repeated work
instances unique. The directory contains `session.json`, `events.jsonl`,
`summary.md`, `notes.md`, and an `outputs/` directory. IDs and event sequence
numbers are immutable after a session reaches a terminal status. D0024 was the
first repository-wide migration from the earlier date-only IDs. A later
decision-authorized synchronization follows D0025 and lists each terminal
session surface in an `Active` SC already committed to `HEAD`; factual evidence
remains locked. Outside that workflow, append a correction instead of changing
a terminal ledger. While work is in progress, the ledger may be curated and
resequenced; Git retains its earlier forms.

An `in_progress` session has `ended_at: null`. Complete, blocked, and abandoned
sessions record the date or timestamp when work stopped.

## Temporary guidance inbox

An active session may also contain a temporary `guidance/` inbox. A specialist
publishes bounded direction as `GNNN-<slug>.ready.md` (optionally with a
candidate `.patch` attachment) and does not directly edit product source while
acting in that role. The session owner follows
[`session-guidance`](../skills/session-guidance/SKILL.md), validates the proposal,
and records a material `adopted`, `adapted`, `rejected`, or `deferred`
disposition using the existing event types. Exactly one disposition event
carries both `guidance_id` and `disposition`; `deferred` also carries
`deferred_to`. A duplicate or obsolete packet is resolved as no-material
without an event.

Guidance packets move through `draft`, `ready`, and `processing` only while the
target session is `in_progress`. They are transient coordination inputs, not
session evidence: remove the packet and attachments after resolution, and
retain only a compact result or an independently justified durable record.
Do not stage a raw inbox file; the pre-commit gate rejects added, modified,
copied, renamed, or type-changed `guidance/` paths while allowing their staged
deletion. Terminal sessions must contain no file or symbolic link under
`guidance/`.

At checkpoint or close, invoke `$roast` explicitly. The summary classifies each
materially promoted claim into one semantic-transformation depth and keeps a
compact `claim -> canonical owner (evidence)` row. The independent
`session-only` section records bounded local-retention reasons; neither section
copies canonical content.

| Session | Date | Fidelity | Status | Summary |
| --- | --- | --- | --- | --- |
| [S0100-20260827-001-metaflux-bootstrap](2026/08/S0100-20260827-001-metaflux-bootstrap/summary.md) | 2026-08-27 | Reconstructed | Complete | MetaFlux planning, bootstrap, architecture review, and build hardening |
| [S0100-20260828-001-spec-consistency](2026/08/S0100-20260828-001-spec-consistency/summary.md) | 2026-08-28 | Exact | Complete | Five specification self-review amendments, D0008/D0009, first commits |
| [S0100-20260828-002-layout-convergence](2026/08/S0100-20260828-002-layout-convergence/summary.md) | 2026-08-28 | Exact | Complete | Mesa/Wine-patterned layout convergence, D0010/D0011, component graph gate |
| [S0100-20260828-003-agent-record-convergence](2026/08/S0100-20260828-003-agent-record-convergence/summary.md) | 2026-08-28 | Exact | Complete | Machine-enforced record-loop rules: index completeness, open-decisions ledger, knowledge-summary gate, staleness warnings, session scaffolder |
| [S0100-20260828-004-record-gate-hardening](2026/08/S0100-20260828-004-record-gate-hardening/summary.md) | 2026-08-28 | Exact | Complete | Record gate hardening: current-progress freshness, status-drift warnings, and the fifteen-case validator self-test |
| [S0100-20260828-005-skills-layer](2026/08/S0100-20260828-005-skills-layer/summary.md) | 2026-08-28 | Exact | Complete | Expert-skills layer under agent/skills with three seeds and validator-enforced form |
| [S0100-20260828-006-agent-guidance-hardening](2026/08/S0100-20260828-006-agent-guidance-hardening/summary.md) | 2026-08-28 | Exact | Complete | Guidance hardening: root AGENTS.md, start-work skill, pre-commit session-coverage gate |
| [S0100-20260828-007-claude-bridge](2026/08/S0100-20260828-007-claude-bridge/summary.md) | 2026-08-28 | Exact | Complete | Repository-local Claude Code bridge: @AGENTS.md import, edit-time guards, drift-checked by the validator |
| [S0101-20260828-008-codex-entry-points](2026/08/S0101-20260828-008-codex-entry-points/summary.md) | 2026-08-28 | Exact | Complete | Codex-native AGENTS.md verified and repository entry points enforced by an isolated Nix check |
| [S0101-20260828-009-implementation-readiness-skill](2026/08/S0101-20260828-009-implementation-readiness-skill/summary.md) | 2026-08-28 | Exact | Complete | Codex-native skill packages, enforced discovery compatibility, and implementation-readiness expert guidance |
| [S0100-20260828-010-domain-expert-skills](2026/08/S0100-20260828-010-domain-expert-skills/summary.md) | 2026-08-28 | Exact | Complete | Ten Codex domain experts, source-backed references, composition routing, and trigger evaluations for M0100-M0130 |
| [S0100-20260828-011-skill-design-convergence](2026/08/S0100-20260828-011-skill-design-convergence/summary.md) | 2026-08-28 | Exact | Complete | Sixteen bounded expert skills, runtime-contract ownership, structured routing gates, and lifecycle publication invariants |
| [S0101-20260828-012-ubuntu-2004-glibc-floor](2026/08/S0101-20260828-012-ubuntu-2004-glibc-floor/summary.md) | 2026-08-28 | Exact | Complete | Ubuntu 20.04 LTS and glibc 2.31 fixed as the W0101 userspace release floor |
| [S0100-20260828-013-m0100-foundation](2026/08/S0100-20260828-013-m0100-foundation/summary.md) | 2026-08-28 | Exact | Complete | M0100 registry, compiler/CPU, daemon, and CUDA/NVML foundation implementation |
| [S0101-20260829-001-toolchain-boundary-correction](2026/08/S0101-20260829-001-toolchain-boundary-correction/summary.md) | 2026-08-29 | Exact | Complete | D0022 tool boundary, skill governance, session cleanup, and duplicate-source reclamation |
| [S0100-20260829-002-cleanup-stale-routes](2026/08/S0100-20260829-002-cleanup-stale-routes/summary.md) | 2026-08-29 | Exact | Complete | Remaining D0022 residue corrected; exact temporary and dead-store routes removed |
| [S0101-20260829-003-timezone-mirror-priority](2026/08/S0101-20260829-003-timezone-mirror-priority/summary.md) | 2026-08-29 | Exact | Complete | Explicit current-timezone mirror priority in manage-toolchain routing |
| [S0100-20260829-004-implementation-session-current-standard](2026/08/S0100-20260829-004-implementation-session-current-standard/summary.md) | 2026-08-29 | Exact | Complete | Curated S0100-20260828-013-m0100-foundation ledger and enforced active/terminal session lifecycle semantics |
| [S0100-20260829-005-m0100-vertical-slice](2026/08/S0100-20260829-005-m0100-vertical-slice/summary.md) | 2026-08-29 | Exact | Complete | M0100 CPU-backed vertical slice and Ubuntu 20.04/glibc 2.31 package enforcement |
| [S0100-20260829-006-stage-breakthrough-commits](2026/08/S0100-20260829-006-stage-breakthrough-commits/summary.md) | 2026-08-29 | Exact | Complete | Verified stage-breakthrough trigger with separate content and record commits |
| [S0100-20260829-007-pytorch-cuda-test-tool](2026/08/S0100-20260829-007-pytorch-cuda-test-tool/summary.md) | 2026-08-29 | Exact | Complete | Isolated PyTorch cu126 baseline, cu132 frontier, and staged CUDA gap probe |
| [S0100-20260830-001-m0100-completion-sprint](2026/08/S0100-20260830-001-m0100-completion-sprint/summary.md) | 2026-08-30 | Exact | Complete | M0100 release qualification, G001-G010 guidance processing, D0012 matrix, signed provenance, and M0100 closure |
| [S0101-20260830-002-ubuntu-target-sdk-guide](2026/08/S0101-20260830-002-ubuntu-target-sdk-guide/summary.md) | 2026-08-30 | Exact | Complete | Explicit Ubuntu 20.04 target SDK construction, consumption, and release-gap guide |
| [S0100-20260830-003-session-guidance-loop](2026/08/S0100-20260830-003-session-guidance-loop/summary.md) | 2026-08-30 | Exact | Complete | Session-local specialist guidance with validated disposition and transient cleanup |
| [S0100-20260830-004-semantic-version-line](2026/08/S0100-20260830-004-semantic-version-line/summary.md) | 2026-08-30 | Exact | Complete | Product SemVer, derived M/W/S identities, 25-session migration, and its capture-time v0.2.0 qualification boundary later split by D0027/SC0003 |
| [S0100-20260830-005-semantic-change-distillation](2026/08/S0100-20260830-005-semantic-change-distillation/summary.md) | 2026-08-30 | Exact | Complete | D0025 governance and SC0001 evidence-preserving prior promotion-model migration applied |
| [S0100-20260830-006-project-knowledge-roast](2026/08/S0100-20260830-006-project-knowledge-roast/summary.md) | 2026-08-30 | Exact | Complete | D0026/SC0002 migration to explicit-only roast semantics and independent session-only disposition |
| [S1000-20260830-007-v100-qualification-boundary](2026/08/S1000-20260830-007-v100-qualification-boundary/summary.md) | 2026-08-30 | Exact | Complete | Applied D0027/SC0003 and moved Intel plus physical NVIDIA binding qualification to v1.0.0 |
| [S0100-20260830-008-agent-harness-commit-identity](2026/08/S0100-20260830-008-agent-harness-commit-identity/summary.md) | 2026-08-30 | Exact | Complete | Initial static Codex/Claude Code harness identity, later superseded by D0028/SC0004 agent-provided subject derivation |
| [S0100-20260830-009-automatic-harness-identity](2026/08/S0100-20260830-009-automatic-harness-identity/summary.md) | 2026-08-30 | Exact | Complete | D0028/SC0004 agent self-declaration replaced fixed mappings and the rejected process-inference route |
| [S0100-20260830-010-m0100-closure-consistency](2026/08/S0100-20260830-010-m0100-closure-consistency/summary.md) | 2026-08-30 | Exact | Complete | Applied SC0005, corrected M0100 closure records, and hardened candidate-index session gates |
| [S0110-20260830-011-converge-project-changes](2026/08/S0110-20260830-011-converge-project-changes/summary.md) | 2026-08-30 | Exact | Complete | Implicit collaborator-delivery convergence, stable Git inventory, ownership routing, and machine-checked fallback |
| [S0111-20260830-012-m0110-abi-contract](2026/08/S0111-20260830-012-m0110-abi-contract/summary.md) | 2026-08-30 | Exact | Complete | W0111 candidate transport ABI schema and deterministic C/C++ projections |
| [S0112-20260830-013-m0110-local-cdev](2026/08/S0112-20260830-013-m0110-local-cdev/summary.md) | 2026-08-30 | Exact | In progress | W0112 cdev client/worker, queue and payload root/owner/VMA/active-operation krefs, owner-death tombstone, registered-memory stage, real CPU backend COPY binding, and GCC Kbuild; Add/launch and fault gates remain |
| [S0113-20260830-014-m0110-static-vfio-user](2026/08/S0113-20260830-014-m0110-static-vfio-user/summary.md) | 2026-08-30 | Exact | In progress | W0113 generated vfio-user control plane, static BAR profile, and compile-checked guest PCI resource binder; data-plane gates remain |
| [S0121-20260830-015-m0120-lifecycle-model](2026/08/S0121-20260830-015-m0120-lifecycle-model/summary.md) | 2026-08-30 | Exact | In progress | W0121 lifecycle extension, generation/epoch and fence/telemetry model checker with CUDA/NVML invariants, runtime telemetry producer guards, marker recovery containment, and 79/79 dev regression; live provider/adapters and qualification remain |
| [S0122-20260831-001-m0120-existing-transport-lifecycle](2026/08/S0122-20260831-001-m0120-existing-transport-lifecycle/summary.md) | 2026-08-31 | Exact | In progress | W0122 coordinator, memfd/cdev/vfio-user mirrors, typed normalizer, QMP/disconnect process ingress fixtures, and runtime ingress; live producer/socket wiring, provider, and qualification gates remain |
| [S0131-20260831-002-vulkan-capability-abi](2026/08/S0131-20260831-002-vulkan-capability-abi/summary.md) | 2026-08-31 | Exact | In progress | Vulkan capability ABI, packed argument and external-memory profiles, deterministic target identity, opt-in tool epoch, and truthful no-device host probe; execution and driver qualification remain open |
| [S0132-20260831-003-vulkan-device-memory](2026/08/S0132-20260831-003-vulkan-device-memory/summary.md) | 2026-08-31 | Exact | In progress | W0132 generation-bound staging ledger and timeline admission model; physical Vulkan allocation and driver qualification remain open |
| [S0133-20260831-004-vulkan-spirv-lowering](2026/08/S0133-20260831-004-vulkan-spirv-lowering/summary.md) | 2026-08-31 | Exact | In progress | W0133 target profile/module preflight diagnostics; MLIR conversion and SPIR-V validation remain open |
| [S0134-20260831-005-vulkan-execution-streams](2026/08/S0134-20260831-005-vulkan-execution-streams/summary.md) | 2026-08-31 | Exact | In progress | W0134 stream/dependency planner with FIFO, explicit waits, visibility masks, and generation guards; Vulkan submission remains open |
| [S0135-20260831-006-vulkan-cache-warm-path](2026/08/S0135-20260831-006-vulkan-cache-warm-path/summary.md) | 2026-08-31 | Exact | In progress | W0135 deterministic portable/device cache identities and bounded catalog; Vulkan pipeline and warm-launch qualification remain open |
| [S0114-20260831-007-transport-fault-qualification](2026/08/S0114-20260831-007-transport-fault-qualification/summary.md) | 2026-08-31 | Exact | In progress | W0114 bounded cdev/vfio-user fault matrix; malformed input, COPY dispositions, backpressure/FIFO retry, stale unmap, and DMA overflow are covered while full qualification remains open |
| [S0123-20260831-008-lifecycle-core-qualification](2026/08/S0123-20260831-008-lifecycle-core-qualification/summary.md) | 2026-08-31 | Exact | In progress | W0123 host-independent 1,000-cycle lifecycle authority qualification; concurrent transport and kernel gates remain open |
| [S0135-20260831-009-vulkan-cache-filesystem](2026/08/S0135-20260831-009-vulkan-cache-filesystem/summary.md) | 2026-08-31 | Exact | In progress | W0135 atomic Vulkan cache file store with envelope validation, corruption cleanup, and device invalidation; pipeline integration remains open |
| [S0134-20260831-010-vulkan-command-recycling](2026/08/S0134-20260831-010-vulkan-command-recycling/summary.md) | 2026-08-31 | Exact | In progress | W0134 host-independent command-resource pool with completion-gated recycling and generation guards; queue submission remains open |
| [S0135-20260831-011-vulkan-cache-catalog](2026/08/S0135-20260831-011-vulkan-cache-catalog/summary.md) | 2026-08-31 | Exact | In progress | W0135 persistent cache repository with disk hydration, pin/quota authority, and device invalidation; cross-process pipeline integration remains open |
| [S0135-20260831-012-vulkan-cache-stampede](2026/08/S0135-20260831-012-vulkan-cache-stampede/summary.md) | 2026-08-31 | Exact | In progress | W0135 cross-process single-key cache miss coalescing; pipeline-bound invalidation and warm launch remain open |
| [S0133-20260831-013-vulkan-module-reflection](2026/08/S0133-20260831-013-vulkan-module-reflection/summary.md) | 2026-08-31 | Exact | In progress | W0133 host-independent SPIR-V reflection and packed-BDA verifier; MLIR emission and module validation remain open |
| [S0132-20260831-014-vulkan-memory-visibility](2026/08/S0132-20260831-014-vulkan-memory-visibility/summary.md) | 2026-08-31 | Exact | In progress | TODO: one-line summary |
| [S0123-20260831-015-lifecycle-concurrent-authority](2026/08/S0123-20260831-015-lifecycle-concurrent-authority/summary.md) | 2026-08-31 | Exact | In progress | TODO: one-line summary |
| [S0135-20260831-016-vulkan-pipeline-binding](2026/08/S0135-20260831-016-vulkan-pipeline-binding/summary.md) | 2026-08-31 | Exact | Complete | W0135 generation-scoped cache pipeline binding; actual Vulkan pipeline and warm-launch qualification remain open |
| [S0135-20260831-017-vulkan-warm-path](2026/08/S0135-20260831-017-vulkan-warm-path/summary.md) | 2026-08-31 | Exact | Complete | W0135 identity mutation miss matrix and host-independent warm trace admission; real ICD trace and physical qualification remain open |
| [S0134-20260831-018-vulkan-submit-admission](2026/08/S0134-20260831-018-vulkan-submit-admission/summary.md) | 2026-08-31 | Exact | Complete | W0134 transactional queue-submission ledger with generation-bound completion and completion-gated recycling; real Vulkan queue adapter remains open |
| [S0131-20260831-019-vulkan-runtime-tools](2026/08/S0131-20260831-019-vulkan-runtime-tools/summary.md) | 2026-08-31 | Exact | Complete | W0131 on-demand Mesa Vulkan ICD and validation-layer runtime profile; physical NVIDIA and dual-driver qualification remain open |
| [S0132-20260831-020-vulkan-device-context](2026/08/S0132-20260831-020-vulkan-device-context/summary.md) | 2026-08-31 | Exact | Complete | W0132 private Vulkan 1.3 device/queue/timeline context with generation-checked RADV smoke; staging allocation is deferred to the existing W0132 boundary |
| [S0112-20260831-021-cuda-cdev-add-copy](2026/08/S0112-20260831-021-cuda-cdev-add-copy/summary.md) | 2026-08-31 | Exact | In progress | W0112 CPU backend, cdev launch and real region COPY through shared-identity resolver/lease; registered-memory DMA and replacement remain open |
| [S0113-20260831-022-vfio-user-guest-ring](2026/08/S0113-20260831-022-vfio-user-guest-ring/summary.md) | 2026-08-31 | Exact | In progress | W0113 host-independent paired guest-ring adapter with BAR2 doorbell callback; live QEMU/PCI qualification remains open |
| [S0111-20260831-023-vfio-user-negotiation](2026/08/S0111-20260831-023-vfio-user-negotiation/summary.md) | 2026-08-31 | Exact | In progress | W0111 vfio-user transport ABI 0.x negotiation with generated guest/server codec and feature selection; live transport activation remains open |
| [S0112-20260831-024-cdev-async-lease](2026/08/S0112-20260831-024-cdev-async-lease/summary.md) | 2026-08-31 | Exact | In progress | W0112 cdev async completion retains backend lease through event completion and ring backpressure; production DMA, replacement, and fault qualification remain open |
| [S0113-20260831-025-cdev-dma-map](2026/08/S0113-20260831-025-cdev-dma-map/summary.md) | 2026-08-31 | Exact | In progress | W0112 registered-memory SG is direction-mapped through the cdev DMA device with reverse-order teardown; backend import, references, replacement, and physical qualification remain open |
| [S0112-20260831-026-cdev-backend-reference](2026/08/S0112-20260831-026-cdev-backend-reference/summary.md) | 2026-08-31 | Exact | In progress | W0112 pending operations retain the exact backend binding through replacement and completion; backend import and daemon generation drain remain open |
| [S0113-20260831-027-cdev-memory-reference](2026/08/S0113-20260831-027-cdev-memory-reference/summary.md) | 2026-08-31 | Exact | In progress | W0112 region COPY retains resolver-owned source/destination memory references through completion and backpressure; production import remains open |
| [S0114-20260831-028-cdev-lifecycle-cancel](2026/08/S0114-20260831-028-cdev-lifecycle-cancel/summary.md) | 2026-08-31 | Exact | In progress | W0112 capability-gated pending cancellation maps transport loss to device-lost completion; production generation drain remains open |
| [S0115-20260831-029-cdev-lifecycle-drain-cancel](2026/08/S0115-20260831-029-cdev-lifecycle-drain-cancel/summary.md) | 2026-08-31 | Exact | In progress | W0112 lifecycle drain processes pending backend work before retirement; non-cancellable quiesce remains rejected |
| [S0116-20260831-030-cdev-multi-region](2026/08/S0116-20260831-030-cdev-multi-region/summary.md) | 2026-08-31 | Exact | In progress | W0112 kernel-private four-slot registered-memory table with unique generation-bound handles, aggregate quota, and per-owner teardown; production backend import remains open |
| [S0117-20260831-031-cdev-memory-reference](2026/08/S0117-20260831-031-cdev-memory-reference/summary.md) | 2026-08-31 | Exact | In progress | W0112 cdev direct backend-memory reference retained through sync/async completion, cancellation, and backpressure; production registered-memory import remains open |
| [S0118-20260831-032-cdev-memory-import](2026/08/S0118-20260831-032-cdev-memory-import/summary.md) | 2026-08-31 | Exact | In progress | W0112 resolver-side registered-memory importer exercised with the real CPU backend; daemon object-table activation remains open |
| [S0119-20260831-033-cdev-worker-lease](2026/08/S0119-20260831-033-cdev-worker-lease/summary.md) | 2026-08-31 | Exact | In progress | W0112 generation/view-bound cdev worker lease activation, paired queue mapping, and deterministic UAPI status handling; daemon object-table activation remains open |
| [S01110-20260831-034-cdev-object-table-resolver](2026/08/S01110-20260831-034-cdev-object-table-resolver/summary.md) | 2026-08-31 | Exact | In progress | W0112 daemon-compatible cdev object-table resolver validates exact COPY blocks and imports checked subranges; live daemon activation and replacement remain open |
| [S01111-20260831-035-cdev-generation-control-plane](2026/08/S01111-20260831-035-cdev-generation-control-plane/summary.md) | 2026-08-31 | Exact | Complete | Audited fixed cdev identity; deferred replacement control to M0120 |
| [S01112-20260831-036-cdev-daemon-object-activation](2026/08/S01112-20260831-036-cdev-daemon-object-activation/summary.md) | 2026-08-31 | Exact | In progress | Daemon object-table region COPY plus queue-only cdev worker boundary; live lease and registered-memory attachment remain open |
| [S01322-20260831-037-vulkan-staging-allocation](2026/08/S01322-20260831-037-vulkan-staging-allocation/summary.md) | 2026-08-31 | Exact | In progress | W0132 physical host-visible Vulkan staging allocation, mapping, and flush/invalidate range boundary on AMD/RADV; backend admission and device-local copy remain open |
| [S01323-20260831-038-vulkan-device-local-copy](2026/08/S01323-20260831-038-vulkan-device-local-copy/summary.md) | 2026-08-31 | Exact | In progress | W0132 source-local Tier 3 host-to-device/device-to-host copy through the generation-bound Vulkan timeline on AMD/RADV; backend admission and external memory remain open |
| [S0110-20260831-039-execution-priority-convergence](2026/08/S0110-20260831-039-execution-priority-convergence/summary.md) | 2026-08-31 | Exact | Complete | Preserved valid Vulkan evidence and restored M0110/M0120 non-NVIDIA execution priority |
| [S0122-20260831-040-live-qmp-socket-transport](2026/08/S0122-20260831-040-live-qmp-socket-transport/summary.md) | 2026-08-31 | Exact | Complete | W0122 bounded QMP Unix-stream command/event transport; producer wiring and qualification remain open |
| [S0123-20260831-041-lifecycle-node-policy](2026/08/S0123-20260831-041-lifecycle-node-policy/summary.md) | 2026-08-31 | Exact | Complete | W0123 canonical cdev udev/node policy, Daemon installation, and package lifecycle assertions; live/kernel qualification remains open |
| [S0122-20260831-042-qmp-socket-lifecycle-bridge](2026/08/S0122-20260831-042-qmp-socket-lifecycle-bridge/summary.md) | 2026-08-31 | Exact | Complete | W0122 QMP socket-to-Coordinator completion bridge with separate transport/lifecycle outcomes and pending/error containment |

## Fidelity and retention

Future project sessions use the same schema. Allowed event types are `objective`,
`decision`, `tool_call`, `tool_result`, and `work_note`. Set `fidelity` to
`exact` when project events are captured as they occur, or `reconstructed` when
an older project state is rebuilt from repository evidence. Reconstruction gaps
use a `work_note` with `omitted: true` and a concrete `reason`.

Inline `content` is UTF-8 and limited to 65,536 bytes. A larger output is stored
as `outputs/NNNN.txt` only when an exact acceptance claim depends on it and no
canonical artifact owns those bytes. Ordinary build/test logs are summarized
and removed. Output references carry exact byte counts and SHA-256 values.

Secrets, credentials, and unrelated environment data are excluded entirely.
Do not preserve even a redacted credential-bearing command; record a sanitized
project action or reference a safe repository script instead.

Run the record validator from the repository root:

```sh
python3 tools/check-agent-records.py .
```
