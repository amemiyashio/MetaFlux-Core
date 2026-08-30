---
id: P20260829-001
status: Recorded
captured: 2026-08-29
milestone: M0100
workstream: W0101
branch: main
git_revision: 7b86b35552c0a52661071cf6a74b76aef1faf289
workspace: durable content committed; correction records follow separately; repository and session-owned build trees removed; S0100-20260828-013-m0100-foundation remains active
---

# Toolchain Boundary Correction

Active milestone: [M0100](../../../plan/M0100-core-foundation/plan.md). Active
workstream:
[W0101](../../../plan/M0100-core-foundation/work/W0101-build-toolchain.md).

## Snapshot

[D0022](../../../memory/decisions-index.md) supersedes D0021. Canonical tool
identity now lives in [`toolchains/`](../../../../toolchains/README.md), while
Nix only materializes and exposes fixed tools. Git owns source history;
CMake/Ninja own builds; CTest and test harnesses own qualification;
`packaging/` owns artifacts; sessions own concise outcomes and exact cleanup;
the host owns Nix-store retention.

The `manage-toolchain` skill governs this boundary, and `record-session` now
requires cleanup accounting instead of worktree snapshots. M0100 and W0101 retain
task, dependency, and acceptance semantics without becoming a repository-wide
toolchain policy.

The current M0100 foundation is committed at the revision above and remains
active. This checkpoint records a governance and implementation handoff, not a
release qualification.

## Verification evidence

| Gate | Result |
| --- | --- |
| `nix flake show . --json` | Passed; tool packages, four dev shells, and formatter only |
| Tool version probes | Clang/LLVM 22.1.8, CMake 4.1.6, Ninja 1.13.2 |
| External CMake dev configure/build | Passed |
| Dev CTest preset | 58/58 passed |
| Agent-record validator self-test | 54/54 passed |
| Performance runner self-tests | 12/12 optimization and 12/12 measurement |
| `manage-toolchain` package validation and independent routing test | Passed |
| Nix format, JSON/Python syntax, ELF RPATH/RUNPATH audit, diff checks | Passed |

## Cleanup evidence

- Removed 5.7 GiB from the old ignored repository `build/` tree.
- Deleted 164 Nix-dead MetaFlux source copies containing that tree; Nix
  reported 564.1 GiB reclaimed without a broad GC.
- Removed the 568 MiB external dev tree after verification. No session-owned
  build or evidence directory remains.
- Root filesystem use changed from 100% to 40%; `/nix/store` measured 38 GiB.

## Decisions and durable outcomes

- D0022 and `manage-toolchain` are the single owner route for tool declarations
  and Nix scope.
- Session cleanup is enforced by the template, scaffolder, validator, and
  `record-session` skill for sessions dated 2026-08-29 onward.
- Historical broad Nix-check results remain historical observations only; they
  are not current workflow entry points.

## Open work and risks

- At capture time, M0100 release, generic-package, stock-tool, AMD/NUMA, fault,
  quota, and optimized-lowering gates remained open. D0023 later assigns Intel
  host qualification to `v0.2.0`.
- S0100-20260828-013-m0100-foundation continues the product vertical slice. Its current 58/58 dev
  result is not release certification.
- Host-wide Nix GC policy remains outside repository semantics.

## Resume notes

1. Read `toolchains/README.md`, current progress, M0100, and the smallest
   matching domain skill.
2. Use `nix develop . --command <owner command>` only to obtain fixed tools.
3. Put generated work under the external `.metaflux-build` or
   `.metaflux-evidence` hierarchy and remove exact session-owned paths at
   handoff.

Related work records:
[S0101-20260829-001-toolchain-boundary-correction](../../../sessions/2026/08/S0101-20260829-001-toolchain-boundary-correction/summary.md)
and
[S0100-20260828-013-m0100-foundation](../../../sessions/2026/08/S0100-20260828-013-m0100-foundation/summary.md).
