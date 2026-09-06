# MetaFlux Milestone Plans

This directory contains durable execution plans for MetaFlux releases.
Milestone and work-item identities derive from the four-part delivery coordinate defined by the
[release-versioning policy](../../docs/release-versioning.md); they are not
arbitrary serials.

| Milestone | Delivery | Release | Status | Goal |
| --- | --- | --- | --- | --- |
| [milestone-0.1.0.0](milestone-0.1.0.0-core-foundation/plan.md) | 0.1.0.0 | v0.1.0 | Complete | CPU-backed CUDA/NVML core foundation |
| [milestone-0.1.1.0](milestone-0.1.1.0-kernel-guest-transport/plan.md) | 0.1.1.0 | v0.1.1 | Complete | Local cdev and static guest transport |
| [milestone-0.1.2.0](milestone-0.1.2.0-vpci-lifecycle/plan.md) | 0.1.2.0 | v0.1.2 | Complete | Lifecycle and experimental vPCI presentation |
| [milestone-0.1.3.0](milestone-0.1.3.0-vulkan-backend/plan.md) | 0.1.3.0 | v0.1.3 | Complete | Vulkan execution backend |
| [milestone-0.2.0.0](milestone-0.2.0.0-pytorch-cuda-compatibility/plan.md) | 0.2.0.0 | v0.2.0 | Queued | PyTorch CUDA compatibility (the reserved NixOS expansion moves to v0.3.0) |
| [milestone-1.0.0.0](milestone-1.0.0.0-stable-qualification/plan.md) | 1.0.0.0 | v1.0.0 | Queued | Stable compatibility contract and reproducible v1.0.0 release |
| [milestone-2.0.0.0](milestone-2.0.0.0-physical-hardware-qualification/plan.md) | 2.0.0.0 | v2.0.0 | Queued | Intel host, physical NVIDIA binding, and dual-driver Vulkan qualification (decision-0040) |

Cross-release research: [PyTorch compatibility](pytorch-compatibility-roadmap.md)
tracks optional baseline and frontier client probes. Its promotion vehicle is
[milestone-0.2.0.0](milestone-0.2.0.0-pytorch-cuda-compatibility/plan.md);
probe-only evidence stays diagnostic until a milestone work item records it as
qualification evidence.

Each milestone owns one `plan.md` and a `work/` directory. The plan defines the
release outcome, scope, dependencies, global acceptance criteria, unresolved
decisions, and Definition of Done. Work documents define independently
verifiable, multi-change execution slices. PR-sized tasks remain in the issue/PR
system rather than becoming permanent repository documents.

Milestone directory names use `milestone-MAJOR.MINOR.PATCH.0-durable-slug`;
work files use `work-item-MAJOR.MINOR.PATCH.WORK-durable-slug`. The dotted body
is copied from explicit `delivery` metadata and checked against the product
release and parent milestone.

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
