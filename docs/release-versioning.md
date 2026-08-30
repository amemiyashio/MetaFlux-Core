---
status: Current
decision: D0024
updated: 2026-08-30
---

# Release And Delivery Versioning

MetaFlux uses product SemVer and delivery coordinates for different jobs. The
product version describes artifacts and compatibility. A delivery coordinate
names the milestone, work item, or session scope that produces evidence for
that product line. Neither namespace is reused for an ABI, protocol, SONAME,
schema, tool, or third-party version.

## Product SemVer

The repository-root [`VERSION`](../VERSION) file is the single source for the
current product artifact version. Product versions have exactly three decimal
components. Documentation and Git tags add `v`; package metadata and program
output use the bare value.

The approved delivery line is:

| Milestone | Product release | Delivery coordinate | Outcome |
| --- | --- | --- | --- |
| [M0100](../agent/plan/M0100-core-foundation/plan.md) | `v0.1.0` | `0.1.0.0` | CPU-backed CUDA/NVML core foundation |
| [M0110](../agent/plan/M0110-kernel-guest-transport/plan.md) | `v0.1.1` | `0.1.1.0` | Local cdev and static guest transport |
| [M0120](../agent/plan/M0120-vpci-lifecycle/plan.md) | `v0.1.2` | `0.1.2.0` | Lifecycle and experimental vPCI presentation |
| [M0130](../agent/plan/M0130-vulkan-backend/plan.md) | `v0.1.3` | `0.1.3.0` | Vulkan execution backend |

`v0.2.0` is the next support and qualification expansion. Intel host
qualification, physical NVIDIA binding-performance qualification, and native
NixOS VM qualification belong there. They are not `v0.1.0` completion gates.
No M record is allocated to `v0.2.0` until its plan is approved; its milestone
scope would compact to `M0200`. `v1.0.0` remains unassigned and is reserved for
an explicit stable public compatibility commitment, not as a synonym for
unscheduled later work.

The `v0.1.x` releases form the initial-development line. A later milestone may
add default-off or experimental capability while preserving the established
compatibility path. A stable public API/ABI break or a required support-matrix
expansion advances the product minor version. An unallocated future version is
not a roadmap promise.

## Delivery Coordinates

A delivery coordinate has four non-negative decimal components:

```text
MAJOR.MINOR.PATCH.WORK
```

The first three components equal the owning product release. The fourth
component is `0` for a milestone and the positive local work ordinal for a work
item. Its compact body is the direct concatenation of the decimal components:

| Coordinate input | Normalized coordinate | Compact body |
| --- | --- | --- |
| `0.1.1.1` | `0.1.1.1` | `0111` |
| `0.2.1.1` | `0.2.1.1` | `0211` |
| `1.2.1` | `1.2.1.0` | `1210` |
| `12.2.1.1` | `12.2.1.1` | `12211` |

The explicit dotted coordinate is authoritative because concatenation is not
reversible when components have multiple digits. The repository rejects a new
record if its compact body collides with another explicit coordinate.

Record identities use that body directly:

- A milestone is `M<compact>`, for example `M0110` for `0.1.1.0`.
- A work item is `W<compact>`, for example `W0111` for `0.1.1.1`.
- A session is `S<compact>-YYYYMMDD-NNN-<slug>`. Its `delivery` names the
  narrowest useful scope; the date and sequence distinguish repeated sessions
  without changing product precedence.

Every M and W record carries `delivery` frontmatter. Every `session.json`
carries `delivery`. Validators derive the expected ID from that field, require
W to resolve to its owning M, require S references to resolve to current M/W
records, and require the plan index release to equal the plan frontmatter.
Changing an assigned product release therefore creates or supersedes a record;
it does not leave an arbitrary serial ID behind.

D0024 is the one breaking migration exception: it renamed every pre-policy M/W/S
record and rewrote those identifier references in historical sessions and
checkpoints. The evidence claims and cited Git revisions in those records did
not change. After D0024, the normal immutability rules apply again and an
assigned coordinate is superseded instead of renamed.

## Independent Version Namespaces

These values do not follow the product delivery coordinate and are not renamed
when a product release advances:

- CUDA/NVML provider DSO versions and SONAMEs such as `1.0.0`.
- Contract, protocol, UAPI, schema, and function-table versions such as `v1`.
- Compiler epochs and cache schema versions.
- Linux, glibc, CUDA, LLVM, Python, PyTorch, and distribution versions.
- Decision, experience, checkpoint, and guidance IDs, which are durable ledger
  identities rather than product delivery scopes.

This policy follows [Semantic Versioning 2.0.0](https://semver.org/spec/v2.0.0.html)
for product versions. The fourth delivery component is repository trace
metadata and never appears in product package versions.
