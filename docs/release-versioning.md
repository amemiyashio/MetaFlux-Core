---
status: Current
decision: decision-0024
updated: 2026-08-30
---

# Release And Delivery Versioning

MetaFlux uses product SemVer and delivery coordinates for different jobs. The
product version describes artifacts and compatibility. A delivery coordinate
names the milestone or work item that produces evidence for
that product line. Neither namespace is reused for an ABI, protocol, SONAME,
schema, tool, or third-party version.

## Product SemVer

The repository-root [`VERSION`](../VERSION) file is the single source for the
current product artifact version. Product versions have exactly three decimal
components. Documentation and Git tags add `v`; package metadata and program
output use the bare value.

The approved delivery line is:

| Milestone | Product release | Delivery coordinate | Outcome |
| --- | --- | --- | --- |
| [milestone-0.1.0.0](../agent/plan/milestone-0.1.0.0-core-foundation/plan.md) | `v0.1.0` | `0.1.0.0` | CPU-backed CUDA/NVML core foundation |
| [milestone-0.1.1.0](../agent/plan/milestone-0.1.1.0-kernel-guest-transport/plan.md) | `v0.1.1` | `0.1.1.0` | Local cdev and static guest transport |
| [milestone-0.1.2.0](../agent/plan/milestone-0.1.2.0-vpci-lifecycle/plan.md) | `v0.1.2` | `0.1.2.0` | Lifecycle and experimental vPCI presentation |
| [milestone-0.1.3.0](../agent/plan/milestone-0.1.3.0-vulkan-backend/plan.md) | `v0.1.3` | `0.1.3.0` | Vulkan execution backend |
| [milestone-0.3.0.0](../agent/plan/milestone-0.3.0.0-pytorch-cuda-compatibility/plan.md) | `v0.3.0` | `0.3.0.0` | PyTorch CUDA compatibility, after the reserved `v0.2.0` NixOS expansion slot |
| [milestone-1.0.0.0](../agent/plan/milestone-1.0.0.0-stable-qualification/plan.md) | `v1.0.0` | `1.0.0.0` | Stable compatibility contract and reproducible v1.0.0 release |

`v0.2.0` remains an unallocated support-expansion line for native NixOS
VM/package qualification. No milestone record is allocated until that plan is approved;
its milestone scope would compact to `milestone-0.2.0.0`.

decision-0040 assigns Intel x86_64 support qualification and physical NVIDIA
binding-performance qualification to milestone-2.0.0.0 / `v2.0.0`, superseding
decision-0027's `v1.0.0` destination; milestone-1.0.0.0 / `v1.0.0` owns the
stable public compatibility commitment and reproducible release instead.
They remain outside milestone-0.1.0.0 / `v0.1.0`. decision-0027 supersedes decision-0023 only for the future
Intel destination and supersedes decision-0024 only for the former `v0.2.0` assignment
of Intel and physical NVIDIA qualification. decision-0024's SemVer and delivery-identity
rules remain authoritative; decision-0012's native NixOS assignment remains unchanged.

The `v0.1.x` releases form the initial-development line. A later milestone may
add default-off or experimental capability while preserving the established
compatibility path. A stable public API/ABI break or a required support-matrix
expansion advances the product minor version. An unallocated future version is
not a roadmap promise.

## Delivery Coordinates

A delivery coordinate has four non-negative decimal components:

```text
MAJOR.MINOR.PATCH.WORK
```

The first three components equal the owning product release. The fourth
component is `0` for a milestone and the positive local work ordinal for a work
item. The dotted coordinate is authoritative and is never compacted.

Repository plan identities spell their kind in full:

- `milestone-0.1.1.0` names delivery `0.1.1.0`.
- `work-item-0.1.1.1` names delivery `0.1.1.1`.

Every milestone and work-item record carries `delivery` frontmatter. Validators
derive the expected full-word ID from that field, require each work item to
resolve to its owning milestone, and require the plan-index release to equal the
plan frontmatter. Changing an assigned product release creates or supersedes a
record; it does not silently rename product meaning.

Agent execution identity is independent of delivery coordinates. Epoch, Batch,
and Iteration use `epoch-NNNN`, `batch-NNNN`, and `iteration-NNNN` under
decision-0033. Decisions and validated experiences use `decision-NNNN` and
`experience-NNNN`.

## Independent Version Namespaces

These values do not follow the product delivery coordinate and are not renamed
when a product release advances:

- CUDA/NVML provider DSO versions and SONAMEs such as `1.0.0`.
- Contract, protocol, UAPI, schema, and function-table versions such as `v1`.
- Compiler epochs and cache schema versions.
- Linux, glibc, CUDA, LLVM, Python, PyTorch, and distribution versions.
- Decision and experience IDs, which are durable knowledge identities rather
  than product delivery scopes.

This policy follows [Semantic Versioning 2.0.0](https://semver.org/spec/v2.0.0.html)
for product versions. The fourth delivery component is repository trace
metadata and never appears in product package versions.
