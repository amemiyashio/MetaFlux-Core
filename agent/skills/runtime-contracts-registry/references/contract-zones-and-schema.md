# Contract Zones and Schema Ownership

Select the zone by what crosses the boundary:

| Zone | Canonical family | Representation rule |
| --- | --- | --- |
| In-process plugin | `contracts/plugin/` | Versioned C function tables; pointers only with explicit lifetime and allocator ownership |
| Encoded protocol | `contracts/protocol/` | Fixed-width little-endian bytes; no native structs or pointers |
| Shared memory | `contracts/shared/` | Fixed offsets and handles; explicit alignment, atomics, cache lines, and publication |
| Linux UAPI | `contracts/uapi/linux/` | Linux-compatible fixed layouts, compat handling, reserved fields, and extension rules |

For each freeze, record one schema owner plus the complete generated-artifact
manifest. work-item-0.1.1.1 authors the candidate base UAPI, device protocol,
BAR/extension directory, and capability extension rules from one canonical
data-plane schema without changing milestone-0.1.0.0 descriptors; work-item-0.1.1.4 qualifies and
freezes it. The Linux, vfio-user, and PCI skills own their layer mechanics; this
skill owns the shared source and cross-consumer drift check.

The milestone-0.1.1.0 base manifest freezes an explicit base-definition allowlist, not every
future file beneath its source directories. Each allowlisted definition appears
once. A later versioned extension owns a separate manifest that imports the base
by content hash and references only its extension definitions; it never adds an
import back into the frozen base.

Within one manifest, a direct import list may name a `(path, version, content
hash)` tuple only once. Transitive closure is a DAG and de-duplicates an identical
tuple reached through multiple paths; the validator rejects cycles, path/version
hash conflicts, and two different contents claiming the same import identity.

Generated files remain with the component that owns their source manifest.
Consumers may wrap generated types behind local APIs, but they may not copy field
lists, offsets, opcodes, masks, or version numbers into private definitions.

Primary repository sources: [contracts](../../../../contracts/README.md),
[component ownership](../../../memory/component-map.md), and
[work-item-0.1.1.1](../../../plan/milestone-0.1.1.0-kernel-guest-transport/work/work-item-0.1.1.1-abi-benchmark-contract.md),
and [work-item-0.1.1.4](../../../plan/milestone-0.1.1.0-kernel-guest-transport/work/work-item-0.1.1.4-fault-abi-freeze.md).
