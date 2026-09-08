---
status: Current
updated: 2026-09-08
---

# Open Decisions

Aggregates every unresolved decision from the milestone plans so that one file
answers "what is still undecided". A row appears when a plan lists a decision
under its "Decisions to Close" and leaves when the decision closes into the
[decision index](decisions-index.md). The milestone plans remain canonical for
rationale and closure conditions; this ledger is the navigable aggregate.
`tools/check-agent-state.py` verifies that each numbered plan decision matches
exactly one row here by normalized content; equal counts with different decisions
fail.

| Milestone | Decision | Blocks | Closure condition |
| --- | --- | --- | --- |
| milestone-0.2.0.0 | Exact baseline-required CUDA Driver and profile-specific internal-table surface, with behavior provenance and stable unsupported-slot failures | Stock PyTorch baseline | Before work-item-0.2.0.1 integration |
| milestone-0.2.0.0 | Minimal versioned ecosystem-neutral request schema and lifetime required for stock PyTorch eager add to become canonical Kernel IR | Stock PyTorch baseline | Before work-item-0.2.0.1 integration |
| milestone-0.2.0.0 | Daemon CPU execution-mode surface and cache identity required for the stock PyTorch baseline | Stock PyTorch baseline | Before work-item-0.2.0.1 integration |
| milestone-0.2.0.0 | Library-backed operator boundary, including whether matmul is supported without vendored cuBLAS execution | Operator corpus | Before work-item-0.2.0.2 corpus freeze |
| milestone-0.2.0.0 | Vulkan daemon routing shape and its qualification matrix | Framework Vulkan routing | Before work-item-0.2.0.3 route implementation |
| milestone-1.0.0.0 | Exact v1.0 stable public compatibility surface, upgrade window, and deprecation policy | Stable release contract | Before work-item-1.0.0.3 release qualification |
| milestone-1.0.0.0 | Released device identity, VID/DID registration, and optional custom presentation identity policy | Release identity | Before work-item-1.0.0.3 release qualification |
| milestone-1.0.0.0 | Module-signing and Secure Boot packaging workflow | Kernel packaging | Before work-item-1.0.0.3 package qualification |
| milestone-1.0.0.0 | Exact supported kernel and distribution matrix for the stable release | Release matrix | Before work-item-1.0.0.3 release qualification |
| milestone-1.0.0.0 | Namespace launcher ownership and released alias allowlist | Released aliases | Before work-item-1.0.0.3 package qualification |
| milestone-2.0.0.0 | Exact Intel x86_64 CPU generation, topology, firmware, and distribution support matrix | Intel support qualification | Before work-item-2.0.0.1 host rows are pinned |
| milestone-2.0.0.0 | Exact physical NVIDIA GPU, driver, PCIe, and host-role binding reference matrix | Binding performance qualification | Before work-item-2.0.0.2 reference runs |
| milestone-2.0.0.0 | Exact physical AMD + NVIDIA dual-driver Vulkan reference matrix and freeze order | Dual-driver qualification | Before work-item-2.0.0.3 reference runs |
