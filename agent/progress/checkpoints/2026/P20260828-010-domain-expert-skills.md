---
id: P20260828-010
status: Recorded
captured: 2026-08-28
milestone: M0100
workstream: W0101
branch: main
git_revision: a40a47553a9654024e06ac80200d61d1543c563b
workspace: Domain expert skill content committed; this checkpoint and its session record are committed afterward
---

# Domain Expert Skill Matrix

Milestone: [M0100](../../../plan/M0100-core-foundation/plan.md). Active
workstream:
[W0101](../../../plan/M0100-core-foundation/work/W0101-build-toolchain.md).
This checkpoint records the skill catalog at revision
`a40a47553a9654024e06ac80200d61d1543c563b` while also routing future work in
queued M0110-M0130.

## Snapshot

The [expert skill catalog](../../../skills/README.md) now contains 15 unique
Active Codex packages. The ten new domain experts cover:

- CUDA Driver ABI and NVML telemetry compatibility;
- PTX/SIMT semantics, MLIR compiler engineering, and x86 CPU execution;
- Linux cdev/UAPI, vfio-user guest transport, PCIe/vPCI presentation, and
  generation/epoch lifecycle resilience;
- Vulkan 1.3 compute and target-constrained SPIR-V execution.

Each new package has English `SKILL.md` instructions in fixed
`Inputs -> Routing -> Workflow -> Output -> Verification` order,
`agents/openai.yaml` with an exact `$skill-id` prompt, and focused references
that link primary sources without copying specifications. There are 38 new
reference files.

[Trigger evaluations](../../../skills/trigger-evals.md) define two positive and
one near-miss negative prompt per domain expert plus five composition cases. The
catalog routes PTX-to-CPU through PTX, MLIR, and CPU owners; vfio-user BAR/MSI-X
reset through Linux, virtualization, PCI, and lifecycle owners; and Vulkan
lowering through MLIR and Vulkan owners.

Von Neumann and Harvard terminology remains inside the CPU memory-architecture
reference, where it informs concrete address-space, split-cache, coherence,
NUMA, ordering, MMIO, and DMA decisions. MLIR remains the cross-backend compiler
expert while CPU and Vulkan own target constraints.

This checkpoint changes guidance only. No runtime API, contract, build graph,
durable constraint, open decision, milestone status, or fixture maturity changed.

## Verification evidence

| Gate | Result |
| --- | --- |
| Bundled Codex `quick_validate.py` | 15/15 packages passed |
| Static package/trigger assertions | 15 unique packages, 38 references, 30 single and 5 composition cases |
| Cached diff whitespace check | Passed for all 60 content files |
| Agent records | Passed before content commit |
| `nix flake check path:. -L` | All checks passed |
| CTest | Not run; content changes are confined to `agent/` |

## Resume notes

1. Begin with `python3 tools/check-agent-records.py .` and `$start-work`; this
   checkpoint is immutable.
2. Select the narrowest expert or composition route from the catalog. Exact
   versions come from the active plan, compiler epoch, pinned headers, and
   qualified support matrix.
3. W0101 remains active. The next product boundaries remain the release
   provider sysroot, CUDA/NVML header acquisition, LLVM 22 patchset, and
   reference-host qualification.
4. Do not interpret expert guidance or trigger coverage as functional CUDA,
   NVML, compiler, kernel, guest, PCI, lifecycle, or Vulkan evidence.

Related work record:
[S0100-20260828-010-domain-expert-skills](../../../sessions/2026/08/S0100-20260828-010-domain-expert-skills/summary.md).
