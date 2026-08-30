---
status: Complete
updated: 2026-08-30
milestone: M0100
workstream: W0106
checkpoint: P20260830-013
---

# Current Progress

Completed milestone: [M0100](../plan/M0100-core-foundation/plan.md), delivery
`0.1.0.0`, product release `v0.1.0`. All workstreams W0101-W0106 are Complete.
Product and delivery identities follow [D0024](../memory/decisions-index.md).
Both foundation and completion sessions are terminal.

Repository-wide replacements of established meaning follow
[D0025](../memory/decisions-index.md). SC0001 remains Applied for that governance
migration; SC0002 replaces only its pre-D0026 promotion-model consequence.
SC0005 is Applied for the M0100 closure-record correction. None is an active
history-edit permit.

[D0026](../memory/decisions-index.md) defines the Verified replacement: only
materially promoted durable claims receive one light, medium, or dark semantic
transformation depth, while `session-only` remains an independent disposition
and `$roast` is explicit-only. Applied SC0002 binds the 68-surface migration to
content revision `991e5327c8a3b1f5d05112f895011f8d83f0bff0`: 58 surfaces were
migrated, three obsolete skill-package paths were removed, and seven evidence
files were retained byte-for-byte. Guidance G002/G004 were adopted by their
target-session owners and both inboxes are empty.

[D0027](../memory/decisions-index.md) and Applied SC0003 now bind Intel x86_64
support qualification plus physical NVIDIA binding-performance promotion to
M1000 / `v1.0.0`. Native NixOS VM/package qualification remains in the
unallocated `v0.2.0` line. G003/G005 were adopted once by their target owners
and their transient packets were removed.

Agent-created Git commits now use the active harness as both Author and
Committer through the `start-work` helper. Revision `ded1dad` proves the Codex
path end to end while leaving the repository-local human identity unchanged;
`record-session` routes content, checkpoint, and closing-record commits through
the same command-local mechanism. Five later M0100 closure commits intended the
`zcode` harness subject but their immutable Git objects record
`amamiya <amamiya@localhost>` for both roles. Applied SC0005 and P013 preserve
the intended and actual identities separately instead of rewriting history.

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
| Integration CTest | 64/64 passed, including component boundaries, registry recovery, PTX interpreter/compiled differentials, provider ABI, daemon integration, release assertions, and million-noop stress |
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
| Git source identity | Build manifests record 40-hex commit, tree, and clean status |
| Reproducibility (independent rebuilds) | Two independent builds from same revision produce byte-for-byte identical metafluxd, libcuda.so, libnvidia-ml.so, and manifests |
| W0102 stress coverage | All 15 sub-items covered; focused ordinary + sanitizer gates pass |
| Complete D0012 matrix (G006 revision) | 8/8 pass with CUDA Add/Copy acceptance on new revision |
| D0026 migration verification | Architecture CTest 6/6; Agent records 163/163 plus repository 27 sessions/210 events/205 Markdown; semantic edits 21/21; guidance 20/20; routing 82 cases and 34/34 self-tests; roast package and independent A-E forward review passed |
| D0027 migration verification | Architecture CTest 6/6; Agent validator 163/163 plus repository 28 sessions/215 events/213 Markdown at record closure; semantic edits 21/21; guidance 20/20; protected evidence and residual scans passed |
| Agent harness commit identity | Isolated forward test 7/7; both workflow skills valid; real content commit `ded1dad` records Codex as Author and Committer while local Git configuration remains `amamiya` |
| M0100 closure consistency | Record correction `1812617`; candidate-index gates `4d1ff2b` / `9586b45`; Agent records 31 sessions / 242 events / 226 Markdown; self-test 169/169; architecture 6/6; semantic edits 21/21 |

The generic release entry point is checked in at
`tools/build-generic-release.sh` with the CMake-owned Ubuntu 20.04 target tuple.
The package builder enforces the glibc 2.31 ceiling, system dependency closure,
and absence of `/nix/store`, RPATH, and RUNPATH references. The offline D0012
matrix owns fresh install, real upgrade, removal, coexistence, and packaged
Add/Copy evidence for Ubuntu 20.04.6, Ubuntu 22.04.5, Ubuntu 24.04.4, and Rocky
Linux 9.8.

## Versioned Next Work

1. M0100, its foundation session, and its completion session are terminal; the
   Applied SC0005 record correction does not reopen product scope or lifecycle.
2. Start new product implementation only under its allocated delivery and
   milestone. M0100 has no remaining closure action.
3. New work uses an explicit four-part delivery coordinate and the derived
   M/W/S identity; no pre-D0024 alias is accepted.
4. Schedule Intel x86_64 support and physical NVIDIA binding performance under
   M1000 / `v1.0.0`; keep native NixOS VM/package qualification in the
   unallocated `v0.2.0` expansion. Do not reopen M0100 for any of them.

## Tool Boundary

Nix fixes and exposes declared tool versions only. Git owns source identity;
agent-run commits use command-local harness identity through `start-work`, while
human Git configuration remains untouched. CMake/Ninja own builds; CTest and
repository harnesses own validation; `packaging/` owns artifacts; sessions own
compact work records and exact cleanup; host operators own Nix-store retention
and garbage collection (D0022).
