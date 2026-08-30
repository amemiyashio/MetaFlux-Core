---
status: Current
updated: 2026-08-29
---

# Open Decisions

Aggregates every unresolved decision from the milestone plans so that one file
answers "what is still undecided". A row appears when a plan lists a decision
under its "Decisions to Close" and leaves when the decision closes into the
[decision index](decisions-index.md). The milestone plans remain canonical for
rationale and closure conditions; this ledger is the navigable aggregate.
`tools/check-agent-records.py` verifies that each numbered plan decision matches
exactly one row here by normalized content; equal counts with different decisions
fail.

| Milestone | Decision | Blocks | Closure condition |
| --- | --- | --- | --- |
| M0110 | Exact base data-plane UAPI v1 and extension namespace | Transport freeze | Only after W0114 evidence |
| M0110 | Exact Linux, QEMU, and libvfio-user support matrix | Kernel and guest qualification | Before W0112/W0113 kernels are pinned |
| M0110 | QEMU shared-memory command line and deployment ownership | Guest fixture | Before W0113 guest slice |
| M0110 | DMA width, pin quotas, ring-order range, drain deadlines, interrupt moderation defaults | Data-plane limits | Only after W0114 evidence |
| M0110 | Provider cdev selection and M0100 fallback diagnostics | W0112 cdev path | Before W0112 provider wiring |
| M0110 | Registered release VID/DID process | Release identity | Before W0115 release |
| M0110 | Module-signing and Secure Boot packaging workflow | Kernel packaging | Before W0115 release hardening |
| M0120 | Exact lifecycle deadline and old-work isolation policy | Lifecycle model | Only after W0123 qualification |
| M0120 | Persistence format for the generation-candidate high-water mark, committed retirement epoch, and daemon_incarnation_id | Registry persistence | Only after W0123 qualification |
| M0120 | Bare-metal domain/bus/devfn allocation and maximum logical functions | vroot presentation | Only after W0124 promotion evidence |
| M0120 | Release VID/DID and optional custom identity workflow, including the registration and legal review that a synthetic NVIDIA presentation identity requires before any release promotion | vroot release | Before vroot package promotion |
| M0120 | Exact supported kernel/distribution matrix | Lifecycle qualification | Before W0123 cycles begin |
| M0120 | Namespace launcher ownership and alias allowlist | NVIDIA-named aliases | Before alias exposure |
| M0120 | Module-signing and Secure Boot workflow | Kernel packaging | Before W0124 package promotion or W0125 release |
| M0130 | Exact Vulkan features, limits, driver families, and minimum driver versions | Capability tiers | Before W0131 capability ABI |
| M0130 | Packed argument ABI and target-environment serialization | SPIR-V lowering | Before W0133 lowering |
| M0130 | External-memory/semaphore tiers per driver family | Memory tiers | Before W0132 memory work |
| M0130 | FP modes and unsupported-semantic policy | Semantic diagnostics | Before W0133 lowering |
| M0130 | Pipeline residency, disk quota, and driver-cache qualification | Cache warm path | Before W0135 cache work |
| M0130 | Queue topology, batching thresholds, and polling/blocking defaults | Execution streams | Before W0134 execution |
| M0130 | Device-loss worker isolation and resource deadline policy | Fault integration | Before W0136 release |
| M1000 | Exact Intel x86_64 CPU generation, topology, firmware, and distribution support matrix | Intel support qualification | Before W1001 host rows are pinned |
| M1000 | Exact physical NVIDIA GPU, driver, PCIe, and host-role binding reference matrix | Binding performance qualification | Before W1002 reference runs |
| M1000 | Exact v1.0 stable public compatibility surface, upgrade window, and deprecation policy | Stable release contract | Before W1003 release qualification |
