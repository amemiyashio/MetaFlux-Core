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
| milestone-0.1.1.0 | Exact base data-plane UAPI v1 and extension namespace | Transport freeze | Only after work-item-0.1.1.4 evidence |
| milestone-0.1.1.0 | Exact Linux, QEMU, and libvfio-user support matrix | Kernel and guest qualification | Before work-item-0.1.1.2/work-item-0.1.1.3 kernels are pinned |
| milestone-0.1.1.0 | QEMU shared-memory command line and deployment ownership | Guest fixture | Before work-item-0.1.1.3 guest slice |
| milestone-0.1.1.0 | DMA width, pin quotas, ring-order range, drain deadlines, interrupt moderation defaults | Data-plane limits | Only after work-item-0.1.1.4 evidence |
| milestone-0.1.1.0 | Registered release VID/DID process | Release identity | Before work-item-0.1.1.5 release |
| milestone-0.1.1.0 | Module-signing and Secure Boot packaging workflow | Kernel packaging | Before work-item-0.1.1.5 release hardening |
| milestone-0.1.2.0 | Exact lifecycle deadline and old-work isolation policy | Lifecycle model | Only after work-item-0.1.2.3 qualification |
| milestone-0.1.2.0 | Persistence format for the generation-candidate high-water mark, committed retirement epoch, and daemon_incarnation_id | Registry persistence | Only after work-item-0.1.2.3 qualification |
| milestone-0.1.2.0 | Bare-metal domain/bus/devfn allocation and maximum logical functions | vroot presentation | Only after work-item-0.1.2.4 promotion evidence |
| milestone-0.1.2.0 | Release VID/DID and optional custom identity workflow, including the registration and legal review that a synthetic NVIDIA presentation identity requires before any release promotion | vroot release | Before vroot package promotion |
| milestone-0.1.2.0 | Exact supported kernel/distribution matrix | Lifecycle qualification | Before work-item-0.1.2.3 cycles begin |
| milestone-0.1.2.0 | Namespace launcher ownership and alias allowlist | NVIDIA-named aliases | Before alias exposure |
| milestone-0.1.2.0 | Module-signing and Secure Boot workflow | Kernel packaging | Before work-item-0.1.2.4 package promotion or work-item-0.1.2.5 release |
| milestone-0.1.3.0 | Exact Vulkan features, limits, driver families, and minimum driver versions | Capability tiers | Before work-item-0.1.3.1 capability ABI |
| milestone-0.1.3.0 | Packed argument ABI and target-environment serialization | SPIR-V lowering | Before work-item-0.1.3.3 lowering |
| milestone-0.1.3.0 | External-memory/semaphore tiers per driver family | Memory tiers | Before work-item-0.1.3.2 memory work |
| milestone-0.1.3.0 | FP modes and unsupported-semantic policy | Semantic diagnostics | Before work-item-0.1.3.3 lowering |
| milestone-0.1.3.0 | Pipeline residency, disk quota, and driver-cache qualification | Cache warm path | Before work-item-0.1.3.5 cache work |
| milestone-0.1.3.0 | Queue topology, batching thresholds, and polling/blocking defaults | Execution streams | Before work-item-0.1.3.4 execution |
| milestone-0.1.3.0 | Device-loss worker isolation and resource deadline policy | Fault integration | Before work-item-0.1.3.6 release |
| milestone-0.2.0.0 | Kernel-intake strategy for torch kernels: arch-pinned PTX-bearing client profiles versus a cubin/SASS intake worker | Torch kernel intake | Before work-item-0.2.0.2 implementation is frozen |
| milestone-0.2.0.0 | Exact provider driver-API surface set required by cudart/ATen init | Baseline client contract | Before work-item-0.2.0.1 integration |
| milestone-0.2.0.0 | Daemon execution-mode surface for framework clients and its cache identity | Framework execution | Before work-item-0.2.0.3 integration |
| milestone-0.2.0.0 | Vulkan daemon routing shape and its qualification matrix | Framework Vulkan routing | Before work-item-0.2.0.3 qualification |
| milestone-1.0.0.0 | Exact v1.0 stable public compatibility surface, upgrade window, and deprecation policy | Stable release contract | Before work-item-1.0.0.3 release qualification |
| milestone-2.0.0.0 | Exact Intel x86_64 CPU generation, topology, firmware, and distribution support matrix | Intel support qualification | Before work-item-2.0.0.1 host rows are pinned |
| milestone-2.0.0.0 | Exact physical NVIDIA GPU, driver, PCIe, and host-role binding reference matrix | Binding performance qualification | Before work-item-2.0.0.2 reference runs |
| milestone-2.0.0.0 | Exact physical AMD + NVIDIA dual-driver Vulkan reference matrix and freeze order | Dual-driver qualification | Before work-item-2.0.0.3 reference runs |
