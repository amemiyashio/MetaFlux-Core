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
| M0002 | Exact base data-plane UAPI v1 and extension namespace | Transport freeze | Only after M0002-W04 evidence |
| M0002 | Exact Linux, QEMU, and libvfio-user support matrix | Kernel and guest qualification | Before W02/W03 kernels are pinned |
| M0002 | QEMU shared-memory command line and deployment ownership | Guest fixture | Before W03 guest slice |
| M0002 | DMA width, pin quotas, ring-order range, drain deadlines, interrupt moderation defaults | Data-plane limits | Only after M0002-W04 evidence |
| M0002 | Provider cdev selection and M0001 fallback diagnostics | W02 cdev path | Before W02 provider wiring |
| M0002 | Registered release VID/DID process | Release identity | Before W05 release |
| M0002 | Module-signing and Secure Boot packaging workflow | Kernel packaging | Before W5 release hardening |
| M0003 | Exact lifecycle deadline and old-work isolation policy | Lifecycle model | Only after M0003-W03 qualification |
| M0003 | Persistence format for the generation-candidate high-water mark, committed retirement epoch, and daemon_incarnation_id | Registry persistence | Only after M0003-W03 qualification |
| M0003 | Bare-metal domain/bus/devfn allocation and maximum logical functions | vroot presentation | Only after M0003-W04 promotion evidence |
| M0003 | Release VID/DID and optional custom identity workflow, including the registration and legal review that a synthetic NVIDIA presentation identity requires before any release promotion | vroot release | Before vroot package promotion |
| M0003 | Exact supported kernel/distribution matrix | Lifecycle qualification | Before W03 cycles begin |
| M0003 | Namespace launcher ownership and alias allowlist | NVIDIA-named aliases | Before alias exposure |
| M0003 | Module-signing and Secure Boot workflow | Kernel packaging | Before W04 package promotion or W05 release |
| M0004 | Exact Vulkan features, limits, driver families, and minimum driver versions | Capability tiers | Before W01 capability ABI |
| M0004 | Packed argument ABI and target-environment serialization | SPIR-V lowering | Before W03 lowering |
| M0004 | External-memory/semaphore tiers per driver family | Memory tiers | Before W02 memory work |
| M0004 | FP modes and unsupported-semantic policy | Semantic diagnostics | Before W03 lowering |
| M0004 | Pipeline residency, disk quota, and driver-cache qualification | Cache warm path | Before W05 cache work |
| M0004 | Queue topology, batching thresholds, and polling/blocking defaults | Execution streams | Before W04 execution |
| M0004 | Device-loss worker isolation and resource deadline policy | Fault integration | Before W06 release |
