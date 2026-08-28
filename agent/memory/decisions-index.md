---
status: Current
updated: 2026-08-28
---

# Decision Index

This index provides stable lookup IDs. The linked source remains authoritative;
an index row does not promote `Proposed` material to `Verified`.

| ID | Topic | Canonical source | Source status |
| --- | --- | --- | --- |
| D0001 | Wine-style transparent compatibility and low-overhead warm paths | [M0001](../plan/M0001-core-foundation/plan.md) | Active plan |
| D0002 | C17/C++20/C ABI split and compiler epoch 1 | [M0001-W01](../plan/M0001-core-foundation/work/W01-build-toolchain.md) | Active workstream |
| D0003 | Compatibility and backend plugins are independent axes | [Plugin ownership](../../plugins/README.md) | Repository boundary |
| D0004 | Contracts are separated by plugin, protocol, shared, and UAPI zones | [Contracts index](../../contracts/README.md) | Repository boundary |
| D0005 | Registry authority and leased data-plane worker ownership | [Control and data plane](../../docs/architecture/control-and-data-plane.md) | Proposed |
| D0006 | cdev/vfio-user transport precedes dynamic vPCI lifecycle | [M0002](../plan/M0002-kernel-guest-transport/plan.md), [M0003](../plan/M0003-vpci-lifecycle/plan.md) | Queued plans |
| D0007 | Vulkan is an execution backend with explicit capability tiers | [M0004](../plan/M0004-vulkan-backend/plan.md) | Queued plan |
| D0008 | Synthetic vendor identity in vroot is a presentation disguise, never a vendor ABI claim or vendor-driver match | [M0003](../plan/M0003-vpci-lifecycle/plan.md) | Queued plan |
| D0009 | Userspace glibc floor is 2.31 (Ubuntu 20.04) with a restricted provider DT_NEEDED universe | [M0001](../plan/M0001-core-foundation/plan.md) | Active plan |
| D0010 | Every transport owns one directory split into C17 client and C++20 worker halves that never share headers | [Transports ownership](../../transports/README.md) | Repository boundary |
| D0011 | Component dependency edges are machine-checked against a role whitelist and the C/CXX language wall | [Component graph check](../../tools/README.md) | Repository boundary |

New decisions receive the next `DNNNN` identifier and point to a plan or
architecture record containing rationale, consequences, and verification state.
