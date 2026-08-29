---
id: P20260829-002
status: Recorded
captured: 2026-08-29
milestone: M0001
workstream: M0001-W01
branch: main
git_revision: 2eba873670473cfb002d66e1ff442674929afc06
workspace: cleanup content committed; session/checkpoint records follow separately; no task-owned generated tree or project GC root; S20260828-013 remains active
---

# Stale Route Cleanup

Active milestone: [M0001](../../../plan/M0001-core-foundation/plan.md). Active
workstream:
[M0001-W01](../../../plan/M0001-core-foundation/work/W01-build-toolchain.md).

## Engineering state

[D0022](../../../memory/decisions-index.md) now holds across all scanned active
surfaces. Git owns source identity and history; Nix exposes fixed tools only;
CMake/Ninja, CTest/tests, packaging, owning evidence harnesses, sessions, and
host operators retain their respective workflows and lifecycle policy.

The residue correction removed stale NixOS package defaults, Nix-owned test and
benchmark wording, compiler-epoch ownership of project PGO profiles, root-local
lifecycle evidence, and checkpoint/source-history ambiguity. M0001 product
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
- Active owner documents and future Agent templates carry the correction;
  historical sessions and checkpoints remain unchanged.

## Open work and risks

- M0001 release, stock-tool, Intel/AMD reference, optimized-lowering, fault,
  quota, soak, and binding-performance gates remain open.
- S20260828-013 continues the product vertical slice; its prior 58/58 dev result
  is not release certification.
- Unrelated host Nix-store retention remains an operator concern.

## Resume notes

1. Read current progress, M0001, and the smallest matching domain skill.
2. Use `nix develop . --command <owner command>` only to obtain fixed tools.
3. Keep generated build/evidence paths outside the repository and remove exact
   session-owned paths after recording compact results.

Related work records:
[S20260829-002](../../../sessions/2026/08/S20260829-002-cleanup-stale-routes/summary.md)
and
[S20260828-013](../../../sessions/2026/08/S20260828-013-m0001-foundation/summary.md).
