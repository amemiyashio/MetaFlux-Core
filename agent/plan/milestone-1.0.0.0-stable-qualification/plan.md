---
id: milestone-1.0.0.0
delivery: 1.0.0.0
release: v1.0.0
status: Queued
budgets: provisional
depends_on: [milestone-0.1.0.0, milestone-0.1.1.0, milestone-0.1.2.0, milestone-0.1.3.0]
areas: [build, backend.cpu, compat.cuda, performance, release]
updated: 2026-08-30
---

# milestone-1.0.0.0: Stable Host and Binding Qualification

## Outcome

Deliver the `v1.0.0` stable-compatibility release after MetaFlux qualifies
Intel x86_64 host support and promotes the physical NVIDIA binding-performance
budgets with complete measured evidence. This milestone consumes the cumulative
`v0.1.x` product line; it does not move either hardware gate back into milestone-0.1.0.0.

Native NixOS VM/package qualification remains an independent `v0.2.0` support
expansion. Intel support here means the x86_64 host CPU, topology, placement,
compiler, runtime, and release path; it does not mean an Intel GPU backend.
Physical NVIDIA hardware supplies the native and passthrough reference evidence;
it does not turn SASS, private RM/UVM, or direct NVIDIA execution into a MetaFlux
backend.

## Release Boundary (decision-0027)

decision-0027 assigns Intel x86_64 support qualification and physical NVIDIA
binding-performance promotion to milestone-1.0.0.0 / `v1.0.0`. It supersedes decision-0023 only for
the future Intel destination and supersedes decision-0024 only for the former assignment
of those two gates to `v0.2.0`. milestone-0.1.0.0 still closes against its AMD x86_64
reference evidence and provisional budgets. decision-0012 still assigns native NixOS
VM/package qualification to `v0.2.0`, whose plan remains unallocated.

No existing benchmark observation becomes binding through this planning change.
The exact Intel and physical NVIDIA evidence required below must be produced from
the revisions and invocations it names.

## Scope

Included:

- Intel x86_64 build, runtime, topology, NUMA, correctness, compatibility, and
  generic-release qualification on the approved support matrix.
- Physical NVIDIA H2D and D2H same-path native baselines, passthrough loss,
  stock-tool interference, and complete device/driver/PCIe/NUMA identity.
- Promotion of the declared performance budgets from provisional to binding only
  after the strict harness passes on the required AMD and Intel reference roles.
- An explicit stable public compatibility surface, prior-release upgrade path,
  reproducible packages, provenance, and regression closure for the cumulative
  product line.

Excluded:

- Native NixOS VM/package qualification, which remains `v0.2.0` scope.
- Intel GPU execution, CUDA Runtime, SASS/cubin execution, private NVIDIA RM/UVM
  compatibility, or a new execution backend.
- Treating a missing physical field, skipped binding row, or provisional milestone-0.1.0.0
  observation as a passing `v1.0.0` result.

## Workstreams

| Workstream | Status | Deliverable |
| --- | --- | --- |
| [work-item-1.0.0.1](work/work-item-1.0.0.1-intel-host-support.md) | Queued | Intel x86_64 host support and qualification |
| [work-item-1.0.0.2](work/work-item-1.0.0.2-nvidia-binding-performance.md) | Queued | Physical NVIDIA binding evidence and budget promotion |
| [work-item-1.0.0.3](work/work-item-1.0.0.3-stable-release.md) | Queued | Stable compatibility contract and reproducible v1.0.0 release |

## Milestone Acceptance

- The approved Intel x86_64 matrix builds and runs the cumulative product suite,
  including interpreter, JIT, AOT, provider, daemon, topology, NUMA, fault, and
  package paths, without vendor-specific CPU assumptions or silent feature loss.
- The strict binding harness completes rather than skips every required H2D,
  D2H, passthrough, stock-tool interference, identity, trace, audit, and native
  baseline field on the approved physical NVIDIA matrix.
- AMD and Intel reference-role results remain separate artifacts. Each result
  pins CPU affinity, NUMA, GPU identity, driver/library hashes, binaries,
  commands, raw samples, stopping rules, and source/tool revisions.
- The canonical performance budgets become `binding` only after work-item-1.0.0.2 passes;
  milestone-0.1.0.0 historical observations remain provisional evidence for their own
  revisions.
- The exact stable public compatibility surface and upgrade/deprecation policy
  are frozen before release. Provider-private implementation details and
  third-party version namespaces are not accidentally promoted to public API.
- One clean Git revision reproduces the accepted generic artifacts, passes the
  declared release matrices and provenance checks, and upgrades from every
  declared supported prior release without replacing vendor-owned files.

## Decisions to Close

1. Exact Intel x86_64 CPU generation, topology, firmware, and distribution support matrix.
2. Exact physical NVIDIA GPU, driver, PCIe, and host-role binding reference matrix.
3. Exact v1.0 stable public compatibility surface, upgrade window, and deprecation policy.

## Definition of Done

milestone-1.0.0.0 / `v1.0.0` is complete only when work-item-1.0.0.1-work-item-1.0.0.3 exit gates pass from named
Git revisions, the Intel and physical NVIDIA evidence is archived by its owning
harness, the performance budget status is binding, the stable compatibility
surface is explicit, and the reproducible release is installable without Nix.
Native NixOS qualification remains outside this DoD and does not silently move
from `v0.2.0`.
