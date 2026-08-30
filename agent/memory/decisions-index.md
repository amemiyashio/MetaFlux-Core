---
status: Current
updated: 2026-08-30
---

# Decision Index

This index provides stable lookup IDs. The linked source remains authoritative;
an index row does not promote `Proposed` material to `Verified`.

| ID | Topic | Canonical source | Source status |
| --- | --- | --- | --- |
| D0001 | Wine-style transparent compatibility and low-overhead warm paths | [M0100](../plan/M0100-core-foundation/plan.md) | Active plan |
| D0002 | C17/C++20/C ABI split and compiler epoch 1 | [W0101 language tasks](../plan/M0100-core-foundation/work/W0101-build-toolchain.md), [compiler epoch 1](../../toolchains/README.md#compiler-epoch-1-d0018) | Active workstream and verified tool input |
| D0003 | Compatibility and backend plugins are independent axes | [Plugin ownership](../../plugins/README.md) | Repository boundary |
| D0004 | Contracts are separated by plugin, protocol, shared, and UAPI zones | [Contracts index](../../contracts/README.md) | Repository boundary |
| D0005 | Registry authority and leased data-plane worker ownership | [Control and data plane](../../docs/architecture/control-and-data-plane.md) | Proposed |
| D0006 | cdev/vfio-user transport precedes dynamic vPCI lifecycle | [M0110](../plan/M0110-kernel-guest-transport/plan.md), [M0120](../plan/M0120-vpci-lifecycle/plan.md) | Queued plans |
| D0007 | Vulkan is an execution backend with explicit capability tiers | [M0130](../plan/M0130-vulkan-backend/plan.md) | Queued plan |
| D0008 | Synthetic vendor identity in vroot is a presentation disguise, never a vendor ABI claim or vendor-driver match | [M0120](../plan/M0120-vpci-lifecycle/plan.md) | Queued plan |
| D0009 | Userspace glibc floor is 2.31 (Ubuntu 20.04) with a restricted provider DT_NEEDED universe | [M0100](../plan/M0100-core-foundation/plan.md) | Active plan |
| D0010 | Every transport owns one directory split into C17 client and C++20 worker halves that never share headers | [Transports ownership](../../transports/README.md) | Repository boundary |
| D0011 | Component dependency edges are machine-checked against a role whitelist and the C/CXX language wall | [Component graph check](../../tools/README.md) | Repository boundary |
| D0012 | `v0.1.0` qualifies four generic distribution rows; native NixOS VM/package qualification moves to `v0.2.0` | [W0101](../plan/M0100-core-foundation/work/W0101-build-toolchain.md#release-qualification-matrix-d0012) | Verified M0100 matrix policy; later NixOS qualification boundary |
| D0013 | Validated absolute-path discovery for same-build vendor CUDA/NVML libraries | [W0106](../plan/M0100-core-foundation/work/W0106-modes-release.md#vendor-library-discovery-d0013) | Active milestone policy |
| D0014 | Per-UID compiler cache isolation, quotas, atomic publication, and deterministic eviction | [W0103](../plan/M0100-core-foundation/work/W0103-compiler-cpu.md#cache-isolation-and-eviction-d0014) | Active milestone policy |
| D0015 | Effective-core CPU worker placement, NUMA-local pools, and CTA-granularity stealing | [W0103](../plan/M0100-core-foundation/work/W0103-compiler-cpu.md#cpu-and-numa-placement-d0015) | Active milestone policy |
| D0016 | Immutable R535/R550/R570/R580/R610 CUDA/NVML header acquisition manifests | [Toolchain ABI inputs](../../toolchains/README.md#cudanvml-abi-inputs-d0016) | Verified input policy and build gate |
| D0017 | Compiler-epoch-1 PTX 9.0/sm_70 capability and semantic-oracle corpus | [W0103](../plan/M0100-core-foundation/work/W0103-compiler-cpu.md#ptx-oracle-and-corpus-d0017) | Verified manifest and semantic oracle |
| D0018 | LLVM 22.1.8 source identity and exact correctness backport | [Compiler epoch 1](../../toolchains/README.md#compiler-epoch-1-d0018) | Verified tool input and correctness gates |
| D0019 | Static MLIR/LLVM component closure for the generic daemon | [M0100 compiler link closure](../plan/M0100-core-foundation/plan.md#compiler-link-closure-d0019) | Measured active milestone policy |
| D0020 | Timezone-relative artifact mirror routing with canonical identity and verification | [Artifact download routing](../../toolchains/README.md#artifact-download-routing-d0020) | Repository tool-input policy; Asia/Shanghai route historically qualified |
| D0021 | Nix-owned declaration of every newly required project workflow tool | [Superseded by D0022](../../toolchains/README.md#tool-provider-boundary-d0022) | Superseded |
| D0022 | Nix fixes and provides tool versions without owning project workflows or lifecycle policy | [Tool provider boundary](../../toolchains/README.md#tool-provider-boundary-d0022) | Repository boundary |
| D0023 | Intel x86_64 host qualification deferred from M0100 / `v0.1.0` to the `v0.2.0` support expansion | [W0101](../plan/M0100-core-foundation/work/W0101-build-toolchain.md) | Active release-boundary decision |
| D0024 | Standard product SemVer plus four-part delivery coordinates, derived M/W/S identifiers, and the `v0.1.0` / `v0.2.0` qualification boundary | [Release and delivery identity](../../docs/release-versioning.md) | Repository identity and release-boundary policy |
| D0025 | Decision-authorized semantic changes synchronize every affected current and historical record while preserving factual evidence | [Semantic change governance](../../docs/architecture/semantic-change-governance.md) | Verified repository governance contract |

New decisions receive the next `DNNNN` identifier and point to a plan or
architecture record containing rationale, consequences, and verification state.
