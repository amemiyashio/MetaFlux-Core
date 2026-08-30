# MetaFlux Milestone Plans

This directory contains durable execution plans for MetaFlux releases. M and W
identities derive from the four-part delivery coordinate defined by the
[release-versioning policy](../../docs/release-versioning.md); they are not
arbitrary serials.

| Milestone | Delivery | Release | Status | Goal |
| --- | --- | --- | --- | --- |
| [M0100](M0100-core-foundation/plan.md) | 0.1.0.0 | v0.1.0 | Complete | CPU-backed CUDA/NVML core foundation |
| [M0110](M0110-kernel-guest-transport/plan.md) | 0.1.1.0 | v0.1.1 | Active | Local cdev and static guest transport |
| [M0120](M0120-vpci-lifecycle/plan.md) | 0.1.2.0 | v0.1.2 | Queued | Lifecycle and experimental vPCI presentation |
| [M0130](M0130-vulkan-backend/plan.md) | 0.1.3.0 | v0.1.3 | Queued | Vulkan execution backend |
| [M1000](M1000-stable-qualification/plan.md) | 1.0.0.0 | v1.0.0 | Queued | Intel host support, physical NVIDIA binding, and stable release qualification |

Cross-release research: [PyTorch compatibility](pytorch-compatibility-roadmap.md)
tracks optional baseline and frontier client probes without changing a milestone.

Each milestone owns one `plan.md` and a `work/` directory. The plan defines the
release outcome, scope, dependencies, global acceptance criteria, unresolved
decisions, and Definition of Done. Work documents define independently
verifiable, multi-change execution slices. PR-sized tasks remain in the issue/PR
system rather than becoming permanent repository documents.

Milestone directory names use `M<compact>-durable-slug`; work files use
`W<compact>-durable-slug`. The compact body is derived from explicit `delivery`
metadata and is checked against the product release and parent milestone.

Allowed status values are `Draft`, `Queued`, `Active`, `Blocked`, `Complete`, and
`Superseded`. Completed milestones remain at their original paths so historical
links stay valid.

Architecture facts and decision rationale belong under
[`docs/architecture/`](../../docs/architecture/README.md). Plans link those facts
and record only milestone-specific consequences and evidence gates. Public byte
layouts and function contracts remain under `contracts/`; subsystem READMEs own
current source and dependency boundaries.

Optional cross-release ecosystem roadmaps may be added only when one component
spans several milestones. They link milestones by stable ID and do not duplicate
architecture, acceptance criteria, or work checklists.
