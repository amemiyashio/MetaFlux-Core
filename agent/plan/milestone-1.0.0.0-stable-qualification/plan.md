---
id: milestone-1.0.0.0
delivery: 1.0.0.0
release: v1.0.0
status: Queued
budgets: provisional
depends_on: [milestone-0.1.0.0, milestone-0.1.1.0, milestone-0.1.2.0, milestone-0.1.3.0, milestone-0.2.0.0]
areas: [release]
updated: 2026-09-10
---

# milestone-1.0.0.0: Stable Compatibility Release Qualification

## Outcome

Deliver the `v1.0.0` stable-compatibility release: an explicit public
compatibility surface, a proven upgrade path, reproducible no-Nix packages,
and regression closure for every accepted `v0.x` release, including the
milestone-0.2.0.0 client and execution corpus.

The stable commitment covers only declared and measured client/workload
profiles. A `v1.0.0` label does not imply universal PyTorch, model training,
mixed precision or framework-compiled execution. The stable-surface decision
must name any such supported application scope and its end-to-end evidence.

This milestone remains queued until the milestone-0.2.0.0 PyTorch CUDA
transparent compatibility foundation closes (decisions 0044 and 0046);
passing a bounded operator corpus does not accelerate or imply the stable
compatibility commitment.

decision-0040 moves the Intel x86_64 host qualification and the physical
NVIDIA binding-performance promotion into milestone-2.0.0.0 / `v2.0.0`; this
milestone no longer gates on physical hardware the executing fleet cannot
guarantee. The declared performance budgets stay `provisional` through
`v1.0.0` and become `binding` only through milestone-2.0.0.0.

Native NixOS VM/package qualification remains an independent `v0.3.0` support
expansion. Intel x86_64 means the host CPU, topology, placement, compiler,
runtime, and release path; it does not mean an Intel GPU backend. Physical
NVIDIA hardware supplies reference evidence only; it does not turn SASS,
private RM/UVM, or direct NVIDIA execution into a MetaFlux backend.

## Release Boundary (decision-0040)

decision-0040 supersedes decision-0027: the Intel and physical NVIDIA gates
that decision-0027 assigned to milestone-1.0.0.0 / `v1.0.0` now belong to
milestone-2.0.0.0 / `v2.0.0`. decision-0012 still assigns native NixOS
VM/package qualification to the support-expansion line, reserved as `v0.3.0`.
milestone-0.1.0.0 still closes against its AMD x86_64 reference evidence and
provisional budgets.

No existing benchmark observation becomes binding through this planning
change.

## Scope

Included:

- An explicit stable public compatibility surface, prior-release upgrade path,
  reproducible packages, provenance, and regression closure for the cumulative
  product line.
- The stable release installs and qualifies on the existing AMD x86_64
  reference host without Nix.

Excluded:

- Intel x86_64 support qualification and physical NVIDIA binding promotion,
  which are milestone-2.0.0.0 scope (decision-0040).
- Native NixOS VM/package qualification, which remains `v0.3.0` scope.
- Intel GPU execution, a new MetaFlux CUDA Runtime replacement, unrestricted SASS/cubin execution, private NVIDIA
  RM/UVM compatibility, or a new execution backend.

This exclusion preserves stock PyTorch's own libcudart and its existing Driver
calls; it does not exclude the accepted PyTorch CUDA route.

## Workstreams

| Workstream | Status | Deliverable |
| --- | --- | --- |
| [work-item-1.0.0.3](work/work-item-1.0.0.3-stable-release.md) | Queued | Stable compatibility contract and reproducible v1.0.0 release |
| milestone-2.0.0.0 work items | Queued | Intel host and physical NVIDIA/dual-driver qualification (decision-0040) |

## Milestone Acceptance

- The exact stable public compatibility surface and upgrade/deprecation policy
  are frozen before release. Provider-private implementation details and
  third-party version namespaces are not accidentally promoted to public API.
- The released manifest names supported PyTorch client versions, application
  or corpus scope, inference/training modes, dtype/shape/layout and compiler/
  library dependencies. Its installed activation entry and CPU/GPU selection
  reproduce those claims; the full corpus alone grants no unnamed model claim.
- One clean Git revision reproduces the accepted generic artifacts, passes the
  declared release matrices and provenance checks, and upgrades from every
  declared supported prior release without replacing vendor-owned files.
- The declared performance budgets remain `provisional` at `v1.0.0`;
  milestone-2.0.0.0 owns their promotion to `binding`.

## Decisions to Close

1. Exact v1.0 stable public compatibility surface, upgrade window, and
   deprecation policy.
2. Released device identity, VID/DID registration, and optional custom
   presentation identity policy.
3. Module-signing and Secure Boot packaging workflow.
4. Exact supported kernel and distribution matrix for the stable release.
5. Namespace launcher ownership and released alias allowlist.

## Definition of Done

milestone-1.0.0.0 / `v1.0.0` is complete only when work-item-1.0.0.3 passes
its exit gate from a named Git revision, the stable compatibility surface is
explicit, and the reproducible release is installable without Nix on the
reference host. Intel and physical NVIDIA evidence is owned by
milestone-2.0.0.0 and is not part of this DoD. Native NixOS qualification
remains outside this DoD and does not silently move from `v0.3.0`.
