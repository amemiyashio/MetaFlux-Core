---
id: milestone-2.0.0.0
delivery: 2.0.0.0
release: v2.0.0
status: Queued
budgets: provisional
depends_on: [milestone-0.1.0.0, milestone-0.1.1.0, milestone-0.1.2.0, milestone-0.1.3.0, milestone-1.0.0.0]
areas: [backend.cpu, compat.cuda, performance, vulkan, release]
updated: 2026-09-05
---

# milestone-2.0.0.0: Physical Hardware Qualification

## Outcome

Own and close every MetaFlux qualification gate that requires physical
hardware the development fleet does not guarantee: physical NVIDIA cards,
Intel x86_64 host rows, and physical dual-driver (AMD + NVIDIA) Vulkan
evidence. decision-0040 moves these gates out of milestone-1.0.0.0 and the
`v0.1.x` milestones into this milestone so product completion is never blocked
on hardware the executing host cannot provide.

Physical NVIDIA hardware supplies native and passthrough reference evidence;
it does not turn SASS, private RM/UVM, or direct NVIDIA execution into a
MetaFlux backend. Intel support means the x86_64 host CPU, topology,
placement, compiler, runtime, and release path; it does not mean an Intel GPU
backend.

## Release Boundary (decision-0040)

decision-0040 supersedes decision-0027 by moving Intel x86_64 support
qualification and physical NVIDIA binding-performance promotion from
milestone-1.0.0.0 / `v1.0.0` into milestone-2.0.0.0 / `v2.0.0`, and by
collecting the physical dual-driver Vulkan rows deferred across
milestone-0.1.3.0 work items here. milestone-1.0.0.0 remains the stable
compatibility-contract and reproducible `v1.0.0` release with provisional
budgets; milestone-0.1.0.0 historical observations remain provisional for
their own revisions.

No existing benchmark observation becomes binding through this planning
change. Every gate below must be produced from the revisions and invocations
it names, on the physical hardware it names.

## Scope

Included:

- Intel x86_64 build, runtime, topology, NUMA, correctness, compatibility, and
  generic-release qualification on the approved support matrix.
- Physical NVIDIA H2D and D2H same-path native baselines, passthrough loss,
  stock-tool interference, and complete device/driver/PCIe/NUMA identity.
- Physical dual-driver (AMD + NVIDIA) Vulkan capability, memory, execution,
  validation-layer soak, driver-change, non-completing-submission, and
  external-memory freeze evidence.
- Promotion of the declared performance budgets from provisional to binding
  only after the strict harness passes on the required reference hardware.

Excluded:

- Native NixOS VM/package qualification, which remains `v0.3.0` scope.
- Intel GPU execution, CUDA Runtime, SASS/cubin execution, private NVIDIA
  RM/UVM compatibility, or a new execution backend.
- Treating a missing physical field or skipped row as a passing result.

## Workstreams

| Workstream | Status | Deliverable |
| --- | --- | --- |
| [work-item-2.0.0.1](work/work-item-2.0.0.1-intel-host-support.md) | Queued | Intel x86_64 host support and qualification |
| [work-item-2.0.0.2](work/work-item-2.0.0.2-nvidia-binding-performance.md) | Queued | Physical NVIDIA binding evidence and budget promotion |
| [work-item-2.0.0.3](work/work-item-2.0.0.3-dual-driver-physical-qualification.md) | Queued | Physical dual-driver Vulkan qualification and external-memory freeze |
| [PyTorch CUDA ops table RE](../milestone-0.2.0.0-pytorch-cuda-compatibility/work/work-item-0.2.0.1-torch-client-bringup.md) | Blocked on physical NVIDIA | Reverse-engineer the a094798c ops table entries (work-item-0.2.0.1, `6bd5fb6c`/`a094798c` boundary) against the real NVIDIA driver behavior |

## Milestone Acceptance

- The approved Intel x86_64 matrix builds and runs the cumulative product
  suite without vendor-specific CPU assumptions or silent feature loss.
- The strict binding harness completes rather than skips every required field
  on the approved physical NVIDIA matrix.
- The physical dual-driver Vulkan matrix produces capability, memory-tier,
  execution, validation-soak, driver-change, and external-memory evidence from
  both driver families with complete device/driver identities.
- One clean Git revision reproduces every accepted artifact; results pin CPU
  affinity, NUMA, GPU identity, driver/library hashes, binaries, commands,
  raw samples, stopping rules, and source/tool revisions.

## Decisions to Close

1. Exact Intel x86_64 CPU generation, topology, firmware, and distribution support matrix.
2. Exact physical NVIDIA GPU, driver, PCIe, and host-role binding reference matrix.
3. Exact physical AMD + NVIDIA dual-driver Vulkan reference matrix and freeze order.

## Definition of Done

milestone-2.0.0.0 / `v2.0.0` is complete only when work-item-2.0.0.1 through
work-item-2.0.0.3 exit gates pass from named Git revisions, the Intel,
physical NVIDIA, and dual-driver evidence is archived by its owning harnesses,
and the declared performance budgets that depend on physical hardware are
promoted to binding with the exact evidence revisions.
