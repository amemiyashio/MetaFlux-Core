---
status: Current
updated: 2026-08-29
---

# Component Map

This is a navigation index. The linked component documentation owns the detailed
boundary.

| Component | Owns | Canonical source |
| --- | --- | --- |
| Contracts | Plugin ABIs, encoded protocols, shared layouts, Linux UAPI | [contracts](../../contracts/README.md) |
| Runtime | Neutral registry/execution mechanisms and application fast path | [runtime](../../runtime/README.md) |
| Compiler | Kernel/Graph IR, common passes, cache keys, neutral orchestration | [compiler](../../compiler/README.md) |
| Compatibility plugins | Ecosystem ABI, management API, compiler input, semantics | [plugins](../../plugins/README.md) |
| Execution backends | Target compilation and execution behind the backend C ABI | [plugins](../../plugins/README.md) |
| Services | Registry/policy authority, compiler isolation, leased workers | [services](../../services/README.md) |
| Transports | Negotiation and generation-bound data movement | [transports](../../transports/README.md) |
| Kernel | cdev, mappings, waits, PCI binding, lifecycle reference safety | [kernel](../../kernel/README.md) |
| Toolchains | Tool versions, immutable inputs, patches, provisioning boundary, and acquisition routing | [toolchains](../../toolchains/README.md) |
| Packaging | NixOS and generic release integration | [packaging](../../packaging/README.md) |
| Cross-component tests | ABI, integration, compatibility, fault, qualification | [tests](../../tests/README.md) |
| Repository layout | Directory taxonomy, dependency direction, navigation | [repo layout](../../docs/architecture/repo-layout.md) |

Dependency direction is contracts outward. Compatibility plugins and execution
backends meet through runtime contracts, never through direct ecosystem-to-target
dependencies. Control/data-plane authority is tracked in the
[proposed architecture record](../../docs/architecture/control-and-data-plane.md).
