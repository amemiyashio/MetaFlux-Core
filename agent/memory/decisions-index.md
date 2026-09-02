---
status: Current
updated: 2026-09-01
---

# Decision Index

This index provides stable lookup IDs. The linked source remains authoritative;
an index row does not promote `Proposed` material to `Verified`.

| ID | Topic | Canonical source | Source status |
| --- | --- | --- | --- |
| decision-0001 | Wine-style transparent compatibility and low-overhead warm paths | [milestone-0.1.0.0](../plan/milestone-0.1.0.0-core-foundation/plan.md) | Complete milestone-0.1.0.0 plan |
| decision-0002 | C17/C++20/C ABI split and compiler epoch 1 | [work-item-0.1.0.1 language tasks](../plan/milestone-0.1.0.0-core-foundation/work/work-item-0.1.0.1-build-toolchain.md), [compiler epoch 1](../../toolchains/README.md#compiler-epoch-1-decision-0018) | Complete workstream and verified tool input |
| decision-0003 | Compatibility and backend plugins are independent axes | [Plugin ownership](../../plugins/README.md) | Repository boundary |
| decision-0004 | Contracts are separated by plugin, protocol, shared, and UAPI zones | [Contracts index](../../contracts/README.md) | Repository boundary |
| decision-0005 | Registry authority and leased data-plane worker ownership | [Control and data plane](../../docs/architecture/control-and-data-plane.md) | Proposed |
| decision-0006 | cdev/vfio-user transport precedes dynamic vPCI lifecycle | [milestone-0.1.1.0](../plan/milestone-0.1.1.0-kernel-guest-transport/plan.md), [milestone-0.1.2.0](../plan/milestone-0.1.2.0-vpci-lifecycle/plan.md) | Queued plans |
| decision-0007 | Vulkan is an execution backend with explicit capability tiers | [milestone-0.1.3.0](../plan/milestone-0.1.3.0-vulkan-backend/plan.md) | Queued plan |
| decision-0008 | Synthetic vendor identity in vroot is a presentation disguise, never a vendor ABI claim or vendor-driver match | [milestone-0.1.2.0](../plan/milestone-0.1.2.0-vpci-lifecycle/plan.md) | Queued plan |
| decision-0009 | Userspace glibc floor is 2.31 (Ubuntu 20.04) with a restricted provider DT_NEEDED universe | [milestone-0.1.0.0](../plan/milestone-0.1.0.0-core-foundation/plan.md) | Complete milestone-0.1.0.0 plan |
| decision-0010 | Every transport owns one directory split into C17 client and C++20 worker halves that never share headers | [Transports ownership](../../transports/README.md) | Repository boundary |
| decision-0011 | Component dependency edges are machine-checked against a role whitelist and the C/CXX language wall | [Component graph check](../../tools/README.md) | Repository boundary |
| decision-0012 | `v0.1.0` qualifies four generic distribution rows; native NixOS VM/package qualification moves to `v0.2.0` | [work-item-0.1.0.1](../plan/milestone-0.1.0.0-core-foundation/work/work-item-0.1.0.1-build-toolchain.md#release-qualification-matrix-decision-0012) | Verified milestone-0.1.0.0 matrix policy; later NixOS qualification boundary |
| decision-0013 | Validated absolute-path discovery for same-build vendor CUDA/NVML libraries | [work-item-0.1.0.6](../plan/milestone-0.1.0.0-core-foundation/work/work-item-0.1.0.6-modes-release.md#vendor-library-discovery-decision-0013) | Completed milestone-0.1.0.0 policy |
| decision-0014 | Per-UID compiler cache isolation, quotas, atomic publication, and deterministic eviction | [work-item-0.1.0.3](../plan/milestone-0.1.0.0-core-foundation/work/work-item-0.1.0.3-compiler-cpu.md#cache-isolation-and-eviction-decision-0014) | Completed milestone-0.1.0.0 policy |
| decision-0015 | Effective-core CPU worker placement, NUMA-local pools, and CTA-granularity stealing | [work-item-0.1.0.3](../plan/milestone-0.1.0.0-core-foundation/work/work-item-0.1.0.3-compiler-cpu.md#cpu-and-numa-placement-decision-0015) | Completed milestone-0.1.0.0 policy |
| decision-0016 | Immutable R535/R550/R570/R580/R610 CUDA/NVML header acquisition manifests | [Toolchain ABI inputs](../../toolchains/README.md#cudanvml-abi-inputs-decision-0016) | Verified input policy and build gate |
| decision-0017 | Compiler-epoch-1 PTX 9.0/sm_70 capability and semantic-oracle corpus | [work-item-0.1.0.3](../plan/milestone-0.1.0.0-core-foundation/work/work-item-0.1.0.3-compiler-cpu.md#ptx-oracle-and-corpus-decision-0017) | Verified manifest and semantic oracle |
| decision-0018 | LLVM 22.1.8 source identity and exact correctness backport | [Compiler epoch 1](../../toolchains/README.md#compiler-epoch-1-decision-0018) | Verified tool input and correctness gates |
| decision-0019 | Static MLIR/LLVM component closure for the generic daemon | [milestone-0.1.0.0 compiler link closure](../plan/milestone-0.1.0.0-core-foundation/plan.md#compiler-link-closure-decision-0019) | Measured completed milestone-0.1.0.0 policy |
| decision-0020 | Timezone-relative artifact mirror routing with canonical identity and verification | [Artifact download routing](../../toolchains/README.md#artifact-download-routing-decision-0020) | Repository tool-input policy; Asia/Shanghai route historically qualified |
| decision-0021 | Nix-owned declaration of every newly required project workflow tool | [Superseded by decision-0022](../../toolchains/README.md#tool-provider-boundary-decision-0022) | Superseded |
| decision-0022 | Nix fixes and provides tool versions without owning project workflows or lifecycle policy | [Tool provider boundary](../../toolchains/README.md#tool-provider-boundary-decision-0022) | Repository boundary |
| decision-0023 | Intel x86_64 host qualification deferred from milestone-0.1.0.0 / `v0.1.0` to the `v0.2.0` support expansion | [Superseded future destination](../plan/milestone-1.0.0.0-stable-qualification/plan.md#release-boundary-decision-0027) | Superseded by decision-0027; milestone-0.1.0.0 AMD reference boundary retained |
| decision-0024 | Standard product SemVer plus four-part delivery coordinates and full-word milestone/work-item identifiers | [Release and delivery identity](../../docs/release-versioning.md) | Current product identity policy; execution identifiers superseded by decision-0033 |
| decision-0025 | Decision-authorized semantic changes synchronized current and historical execution records | [Goal-first execution](../../docs/architecture/agent-execution.md) | Superseded by decision-0033 |
| decision-0026 | Roast used record-shaped dispositions for project knowledge promotion | [Goal-first execution](../../docs/architecture/agent-execution.md#knowledge-promotion) | Superseded by decision-0033 |
| decision-0027 | Intel x86_64 support and physical NVIDIA binding-performance qualification belong to milestone-1.0.0.0 / `v1.0.0`; native NixOS remains `v0.2.0` scope | [milestone-1.0.0.0 release boundary](../plan/milestone-1.0.0.0-stable-qualification/plan.md#release-boundary-decision-0027) | Queued release-boundary decision |
| decision-0028 | Agent commits used a runtime-declared harness identity | [Agent tool detection](../../docs/architecture/agent-tool-detection.md) | Superseded by decision-0034 executable evidence |
| decision-0029 | One persisted execution focus and exact execution owner authorized durable commits | [Goal-first execution](../../docs/architecture/agent-execution.md) | Superseded by decision-0033 |
| decision-0030 | Local managed providers select cdev first, bind its data queue to the same Unix control session/view/generation, and use only narrowly classified pre-success memfd fallback | [milestone-0.1.1.0 provider cdev boundary](../plan/milestone-0.1.1.0-kernel-guest-transport/plan.md#provider-cdev-selection-and-fallback-diagnostics-decision-0030) | Current milestone-0.1.1.0 decision; implementation and qualification remain open |
| decision-0031 | Agent startup enters a mandatory Git-aware Nix tool environment before project-tool use | [Start work](../skills/start-work/SKILL.md) | Nix-first boundary retained; fixed identity superseded by decision-0034 |
| decision-0032 | Confirmed Nix gaps and MetaFlux driver debugging use bounded, non-secret host-privilege helpers | [Host privilege escalation](../../docs/architecture/host-privilege-escalation.md) | Retained by decision-0033 and machine-checked |
| decision-0033 | Goal-first multi-Agent execution uses destructive Epoch governance, parallel Batch Iterations, independent integration, ephemeral roast promotion, and no execution-history ledger | [Goal-first execution](../../docs/architecture/agent-execution.md) | Verified topology; fixed identity boundary superseded by decision-0034 |
| decision-0034 | Agent identity comes from bounded harness/CLI executable detection and never from model metadata | [Agent tool detection](../../docs/architecture/agent-tool-detection.md) | Verified; activates epoch-0002 |
| decision-0035 | Epoch-0003 governance: remove stale out-of-repository worktrees, advance Epoch, and continue non-1.0 plan completion with deferred lane carried forward | [agent/goal.json](../goal.json) | Active governance; activates epoch-0003 |
| decision-0036 | Tool provisioning is a resolvable task, not a blocker: when a required tool is absent, always attempt Nix then manage-host-privilege/pacman before declaring inability to proceed | [AGENTS.md](../../AGENTS.md) rule 7, [manage-host-privilege](../skills/manage-host-privilege/SKILL.md) | Active; prevents agents from treating missing tools as absolute blockers |

New decisions receive the next `decision-NNNN` identifier and point to a plan or
architecture record containing rationale, consequences, and verification state.
