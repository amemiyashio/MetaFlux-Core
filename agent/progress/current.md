---
status: Active
updated: 2026-08-30
milestone: M0100
workstream: W0106
checkpoint: P20260830-007
---

# Current Progress

Active milestone: [M0100](../plan/M0100-core-foundation/plan.md), delivery
`0.1.0.0`, product release `v0.1.0`. Active workstream:
[W0106](../plan/M0100-core-foundation/work/W0106-modes-release.md). Product and
delivery identities follow [D0024](../memory/decisions-index.md).

Repository-wide replacements of established meaning follow
[D0025](../memory/decisions-index.md). SC0001 remains Applied for that governance
migration; SC0002 replaces only its pre-D0026 promotion-model consequence.
Neither record is an active history-edit permit.

[D0026](../memory/decisions-index.md) defines the Verified replacement: only
materially promoted durable claims receive one light, medium, or dark semantic
transformation depth, while `session-only` remains an independent disposition
and `$roast` is explicit-only. Applied SC0002 binds the 68-surface migration to
content revision `991e5327c8a3b1f5d05112f895011f8d83f0bff0`: 58 surfaces were
migrated, three obsolete skill-package paths were removed, and seven evidence
files were retained byte-for-byte. Guidance G002/G004 were adopted by their
target-session owners and both inboxes are empty.

## Current Boundary

The M0100 core implementation and its generic release route have recorded
passing evidence. D0024's breaking semantic-identity migration is complete in
content revisions `78fc9d8`, `8b79bb0`, and `209caee`: current and historical
M/W/S records, entry points, validators, build targets, and version output now
use one derived coordinate scheme. Historical results remain evidence only for
the revisions and invocations they name.

The following items are not `v0.1.0` blockers:

- Intel x86_64 support qualification (D0027, superseding D0023 only for its
  future destination).
- Physical NVIDIA H2D/D2H and passthrough evidence that promotes provisional
  performance budgets to binding.
- Native NixOS VM/package qualification.

Intel x86_64 support qualification and physical NVIDIA binding-performance
promotion belong to M1000 / `v1.0.0`. Native NixOS VM/package qualification
remains the unallocated `v0.2.0` support expansion. M0100 keeps the measured
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
| D0026 migration verification | Architecture CTest 6/6; Agent records 163/163 plus repository 27 sessions/210 events/205 Markdown; semantic edits 21/21; guidance 20/20; routing 82 cases and 34/34 self-tests; roast package and independent A-E forward review passed |

The generic release entry point is checked in at
`tools/build-generic-release.sh` with the CMake-owned Ubuntu 20.04 target tuple.
The package builder enforces the glibc 2.31 ceiling, system dependency closure,
and absence of `/nix/store`, RPATH, and RUNPATH references. The offline D0012
matrix owns fresh install, real upgrade, removal, coexistence, and packaged
Add/Copy evidence for Ubuntu 20.04.6, Ubuntu 22.04.5, Ubuntu 24.04.4, and Rocky
Linux 9.8.

## Versioned Next Work

1. The foundation and completion sessions now use the D0026 terminal-summary
   contract and have empty guidance inboxes; their owners can complete their own
   final cleanup and lifecycle closure.
2. Close `v0.1.0` only from a verified Git revision after the remaining active
   session records are closed by their owners.
3. New work uses an explicit four-part delivery coordinate and the derived
   M/W/S identity; no pre-D0024 alias is accepted.
4. Schedule Intel x86_64 support and physical NVIDIA binding performance under
   M1000 / `v1.0.0`; keep native NixOS VM/package qualification in the
   unallocated `v0.2.0` expansion. Do not reopen M0100 for any of them.

## Tool Boundary

Nix fixes and exposes declared tool versions only. Git owns source identity;
CMake/Ninja own builds; CTest and repository harnesses own validation;
`packaging/` owns artifacts; sessions own compact work records and exact cleanup;
host operators own Nix-store retention and garbage collection (D0022).
