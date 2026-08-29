# MetaFlux Milestone Plans

This directory contains durable execution plans for MetaFlux releases. Stable
milestone IDs are independent of product versions: changing a release assignment
does not rename a milestone or invalidate links.

| Milestone | Release | Status | Goal |
| --- | --- | --- | --- |
| [M0001](M0001-core-foundation/plan.md) | v0.1 | Active | CPU-backed CUDA/NVML core foundation |
| [M0002](M0002-kernel-guest-transport/plan.md) | v0.2.1 | Queued | Local cdev and static guest transport |
| [M0003](M0003-vpci-lifecycle/plan.md) | v0.2.2 | Queued | Lifecycle and experimental vPCI presentation |
| [M0004](M0004-vulkan-backend/plan.md) | v0.2.3 | Queued | Vulkan execution backend |

Cross-release research: [PyTorch compatibility](pytorch-compatibility-roadmap.md)
tracks optional baseline and frontier client probes without changing a milestone.

Each milestone owns one `plan.md` and a `work/` directory. The plan defines the
release outcome, scope, dependencies, global acceptance criteria, unresolved
decisions, and Definition of Done. Work documents define independently
verifiable, multi-change execution slices. PR-sized tasks remain in the issue/PR
system rather than becoming permanent repository documents.

Milestone directory names use `MNNNN-durable-slug`. The numeric ID is the link
and dependency identity; neither it nor the slug embeds the assigned release.

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
