---
status: Active
updated: 2026-08-30
milestone: M0100
workstream: W0106
checkpoint: P20260830-006
---

# Current Progress

Active milestone: [M0100](../plan/M0100-core-foundation/plan.md), delivery
`0.1.0.0`, product release `v0.1.0`. Active workstream:
[W0106](../plan/M0100-core-foundation/work/W0106-modes-release.md). Product and
delivery identities follow [D0024](../memory/decisions-index.md).

Repository-wide replacements of established meaning follow
[D0025](../memory/decisions-index.md). Governance revision
`1ecdfb01497610c5042e12bda4a16839d2b9c734` adds independent SC records,
evidence-preserving history synchronization, transient active-session handoff,
and staged/HEAD machine gates. Applied SC0001 enumerates 75 exact current,
tooling, and historical surfaces plus G001/G003 handoffs. Content revision
`90c45eda2815c59617fffea581522b7ed6bff1c0` synchronized all 31 authorized
historical files, completed the terminal summary gate, and preserved the four
whole-file evidence surfaces. Its history-edit authority is now closed.

[D0026](../memory/decisions-index.md) now defines the proposed replacement of
the pre-D0026 promotion model: only durable promotions receive one
light, medium, or dark semantic-transformation depth, while `session-only`
remains an independent disposition and `$roast` is explicit-only. Revision
`21f1d47cd0522682e4047542fc53627bff2967fe` establishes the decision only.
S0100-20260830-006-project-knowledge-roast owns the pending SC0002
authorization, implementation, active handoffs, and complete
evidence-preserving migration.

SC0002 is now Active with 68 exact affected surfaces. Guidance G002 and G004
are published to the two other active owners; protected historical edits remain
pending until this authorization is committed to `HEAD`.

## Current Boundary

The M0100 core implementation and its generic release route have recorded
passing evidence. D0024's breaking semantic-identity migration is complete in
content revisions `78fc9d8`, `8b79bb0`, and `209caee`: current and historical
M/W/S records, entry points, validators, build targets, and version output now
use one derived coordinate scheme. Historical results remain evidence only for
the revisions and invocations they name.

The following items are not `v0.1.0` blockers:

- Intel x86_64 host qualification (D0023).
- Physical NVIDIA H2D/D2H and passthrough evidence that promotes provisional
  performance budgets to binding.
- Native NixOS VM/package qualification.

All three belong to the `v0.2.0` support expansion. M0100 keeps the measured
performance targets provisional and uses AMD x86_64 as its reference host.

## Recorded M0100 Evidence

| Gate | Recorded result |
| --- | --- |
| Integration CTest | 65/65 passed, including component boundaries, registry recovery, PTX interpreter/compiled differentials, provider ABI, daemon integration, release assertions, and million-noop stress |
| Recovery stress | 50/50 ordinary and 20/20 ASan passed |
| CUDA Add/Copy | Interpreter, cold JIT, warm JIT, and AOT passed |
| Stock `nvidia-smi` / NVML | Supported views and CUDA/NVML identity parity passed |
| Modes and coexistence | Managed, passthrough, auto/fail-open, managed-only, namespace isolation, and recursion prevention passed |
| Optimization and hardening | PGO USE build (135 commands), O2/O3 comparison (28 commands), and ASan/UBSan hardening (121 commands) passed |
| Generic target build | Ubuntu 20.04 SDK route produced `metafluxd` at `GLIBC_2.29`, providers at `GLIBC_2.17` / `GLIBC_2.14`, with no Nix path or RPATH |
| D0012 provider matrix | One recorded run passed 8/8 rows across four distributions and two formats |
| D0012 complete matrix | Two clean same-revision runs passed 8/8 rows, including packaged CUDA Add/Copy |
| Reproducibility | Independent DEB, RPM, and tar builds matched byte-for-byte |
| Signed target SDK provenance | Passed for snapshot `20260820T000000Z`, two signed releases, three indexes, and ten packages |
| D0024 migration verification | Dev build and CTest 63/63; Agent records 111/111; guidance 17/17; routing 68 cases and 23/23 self-tests; 18 skills valid |
| D0025 migration verification | Architecture CTest 6/6; Agent records 136/136; semantic edits 21/21; guidance 17/17; routing 81 cases and 34/34 self-tests; two workflow skills valid |

The generic release entry point is checked in at
`tools/build-generic-release.sh` with the CMake-owned Ubuntu 20.04 target tuple.
The package builder enforces the glibc 2.31 ceiling, system dependency closure,
and absence of `/nix/store`, RPATH, and RUNPATH references. The offline D0012
matrix owns fresh install, real upgrade, removal, coexistence, and packaged
Add/Copy evidence for Ubuntu 20.04.6, Ubuntu 22.04.5, Ubuntu 24.04.4, and Rocky
Linux 9.8.

## Versioned Next Work

1. The foundation owner processes G001 at its next control boundary. The M0100
   completion owner processes G002 before G003, then reconciles its compact
   session record with D0024/D0025.
2. Close `v0.1.0` only from a verified Git revision after the remaining active
   session records and transient guidance are resolved by their owners.
3. New work uses an explicit four-part delivery coordinate and the derived
   M/W/S identity; no pre-D0024 alias is accepted.
4. Schedule Intel host, physical NVIDIA binding performance, and native NixOS
   VM/package qualification under `v0.2.0`; do not reopen M0100 for them.

## Tool Boundary

Nix fixes and exposes declared tool versions only. Git owns source identity;
CMake/Ninja own builds; CTest and repository harnesses own validation;
`packaging/` owns artifacts; sessions own compact work records and exact cleanup;
host operators own Nix-store retention and garbage collection (D0022).
