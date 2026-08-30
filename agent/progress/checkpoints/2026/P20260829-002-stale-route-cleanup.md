---
id: P20260829-002
status: Recorded
captured: 2026-08-29
milestone: M0100
workstream: W0101
branch: main
git_revision: 2eba873670473cfb002d66e1ff442674929afc06
workspace: cleanup content committed; session/checkpoint records follow separately; no task-owned generated tree or project GC root; S0100-20260828-013-m0100-foundation remains active
---

# Stale Route Cleanup

Active milestone: [M0100](../../../plan/M0100-core-foundation/plan.md). Active
workstream:
[W0101](../../../plan/M0100-core-foundation/work/W0101-build-toolchain.md).

## Engineering state

[D0022](../../../memory/decisions-index.md) now holds across all scanned active
surfaces. Git owns source identity and history; Nix exposes fixed tools only;
CMake/Ninja, CTest/tests, packaging, owning evidence harnesses, sessions, and
host operators retain their respective workflows and lifecycle policy.

The residue correction removed stale NixOS package defaults, Nix-owned test and
benchmark wording, compiler-epoch ownership of project PGO profiles, root-local
lifecycle evidence, and checkpoint/source-history ambiguity. M0100 product
implementation remains active and is not release-qualified by this cleanup.

## Verification evidence

| Gate | Result |
| --- | --- |
| Nix parse/format and flake inspection | Passed; only tools, four shells, and formatter exposed |
| Tool version probes | Clang 22.1.8, CMake 4.1.6, Ninja 1.13.2 |
| NixOS module positive/negative evaluation | Explicit package accepted; missing package rejected |
| Agent records and validator self-test | Passed; 54/54 self-test cases |
| Skill routing corpus and self-test | Passed; 68 cases and 23/23 self-tests |
| Active stale-semantic scan and independent diff review | Passed |

## Cleanup evidence

- Removed at least 2.56 GiB of exact project temporary paths and repository
  residue.
- Deleted a prevalidated 1,294-path dead Nix referrer closure; Nix reported
  26.1 GiB freed. The set had no live, current-tool, or external-referrer
  intersection, and no broad GC ran.
- Final state has zero matching old product seeds, MetaFlux source copies,
  project GC roots, project temporary paths, repository build/result routes, or
  Python caches. The 104 legitimate MetaFlux tool paths remain.
- Root filesystem is 37% used with 602 GiB available at handoff.

## Decisions and durable outcomes

- D0022 remains the single current tool/workflow boundary; no new decision was
  needed.
- Active owner documents and future Agent templates carried the correction at
  this checkpoint. D0024 later performs a breaking identity and version-boundary
  migration across historical sessions and checkpoints without changing their
  measured evidence.

## Open work and risks

- At capture time, M0100 release, stock-tool, AMD reference,
  optimized-lowering, fault, quota, and soak gates remained open. D0023/D0024
  later assigned Intel host qualification and physical NVIDIA
  binding-performance promotion to `v0.2.0`; D0027 later supersedes only those
  future destinations with M1000 / `v1.0.0`.
- S0100-20260828-013-m0100-foundation continues the product vertical slice; its prior 58/58 dev result
  is not release certification.
- Unrelated host Nix-store retention remains an operator concern.

## Resume notes

1. Read current progress, M0100, and the smallest matching domain skill.
2. Use `nix develop . --command <owner command>` only to obtain fixed tools.
3. Keep generated build/evidence paths outside the repository and remove exact
   session-owned paths after recording compact results.

Related work records:
[S0100-20260829-002-cleanup-stale-routes](../../../sessions/2026/08/S0100-20260829-002-cleanup-stale-routes/summary.md)
and
[S0100-20260828-013-m0100-foundation](../../../sessions/2026/08/S0100-20260828-013-m0100-foundation/summary.md).
