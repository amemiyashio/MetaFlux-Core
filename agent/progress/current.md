---
status: Active
updated: 2026-08-28
milestone: M0001
workstream: M0001-W01
checkpoint: P20260828-012
---

# Current Progress

Active milestone: [M0001](../plan/M0001-core-foundation/plan.md). Active
workstream:
[M0001-W01](../plan/M0001-core-foundation/work/W01-build-toolchain.md). Latest checkpoint:
[P20260828-012](checkpoints/2026/P20260828-012-ubuntu-2004-glibc-floor.md).

## Current state

The repository has a reproducible, buildable engineering scaffold for the
M0001 boundaries. Targets remain fixtures: CUDA/NVML compatibility, CPU SIMT
execution, compiler service behavior, device registry, and shared transports are
not yet functionally implemented.

Git history on `main` is a linear bootstrap and governance chain through
content revision `a40a475`. The 2026-08-28 self-review removed five
specification contradictions
([S20260828-001](../sessions/2026/08/S20260828-001-spec-consistency/summary.md));
the same-day layering review converged the repository structure on Mesa/Wine
patterns ([S20260828-002](../sessions/2026/08/S20260828-002-layout-convergence/summary.md)):

- D0008: vroot synthetic NVIDIA identity is a presentation disguise with
  registration/legal review required before release promotion.
- D0009: Ubuntu 20.04 LTS is the minimum supported userspace distribution and
  glibc 2.31 is the fixed ABI floor, with a restricted
  provider `DT_NEEDED` universe.
- D0010: every transport owns one directory split into C17 client and C++20
  worker halves that never share headers.
- D0011: component dependency edges are machine-checked against a role
  whitelist and the C/CXX language wall (CTest
  `metaflux.architecture.component-graph`).
- Numeric performance budgets are provisional until the named harness archives
  a baseline; the authoritative logical-device view is scoped per managed
  domain; unit tests live beside their owners; planned-component homes are
  recorded in the [roadmap](../../docs/roadmap.md) instead of placeholder
  directories; the taxonomy is documented in
  [repo layout](../../docs/architecture/repo-layout.md).

The record loop itself is machine-enforced as of
[S20260828-003](../sessions/2026/08/S20260828-003-agent-record-convergence/summary.md):
index completeness, the
[open-decisions ledger](../memory/open-decisions.md) (29 unresolved decisions
across M0001-M0004), mandatory session distillation, staleness warnings, and
`tools/new-session.py` scaffolding.
[S20260828-004](../sessions/2026/08/S20260828-004-record-gate-hardening/summary.md)
added current-progress freshness, status-drift warnings, and the fifteen-case
validator self-test, so the record gate has its own gate.
[S20260828-005](../sessions/2026/08/S20260828-005-skills-layer/summary.md)
added the [expert-skills layer](../skills/README.md) with three seeds
(add-component, close-decision, record-session) and validator-enforced form.
[S20260828-006](../sessions/2026/08/S20260828-006-agent-guidance-hardening/summary.md)
hardened guidance against rule-skipping agents: the root `AGENTS.md` entry
point, the start-work on-ramp skill, and a devshell-installed pre-commit gate
requiring an in-progress session for any change outside `agent/`.
[S20260828-007](../sessions/2026/08/S20260828-007-claude-bridge/summary.md)
added the repository-local Claude Code bridge: `CLAUDE.md` as an `@AGENTS.md`
import plus `.claude/` edit-time guards with the same two invariants,
strictly repo-scoped and inert for other tools.
[S20260828-008](../sessions/2026/08/S20260828-008-codex-entry-points/summary.md)
verified that Codex reads the root `AGENTS.md` natively and added an isolated
Nix entry-point check. The check proves the independent Agent-record fileset
retains `AGENTS.md`, the tool-agnostic pre-commit gate, validator dependencies,
and the optional Claude bridge without treating Claude hooks as Codex hooks.
[S20260828-009](../sessions/2026/08/S20260828-009-implementation-readiness-skill/summary.md)
aligned the expert-skills layer with the official Codex package contract.
Physical packages retain their durable `agent/skills` links while the checked
`.agents/skills` symlink provides native discovery. All five packages pass the
bundled Codex validator; the repository validator now covers the standard
frontmatter and optional resource layout with 29 synthetic cases. The new
[implementation-readiness skill](../skills/implementation-readiness/SKILL.md)
separates architecture, workstream activation, implementation maturity, and
release readiness instead of collapsing them into one score.
[S20260828-010](../sessions/2026/08/S20260828-010-domain-expert-skills/summary.md)
expanded that Codex-native catalog from five to fifteen Active packages. Ten
single-responsibility domain experts now cover CUDA/NVML ABI and telemetry,
PTX/SIMT semantics, MLIR compiler engineering, CPU execution, Linux UAPI,
vfio-user, PCIe/vPCI, lifecycle, and Vulkan/SPIR-V. Their
[composition routes](../skills/README.md) preserve ownership between semantics,
compiler mechanism, target execution, transport, presentation, and lifecycle;
the [trigger corpus](../skills/trigger-evals.md) records 30 single-skill and five
multi-skill routing cases. These are implementation guides, not evidence that
the corresponding queued workstreams are functionally complete.

## Established bootstrap boundaries

- CMake/Ninja component selection and pinned Clang/LLVM/MLIR/LLD epoch 1.
- Nix packages and checks with fileset-scoped monorepo sources.
- Narrow client-protocol and backend-plugin contract targets.
- Ecosystem provider, compiler frontend, and execution backend build roles.
- CUDA/NVML hidden symbol surfaces, independent ABI gates, and co-load test.
- Runtime and compiler-toolchain install-consumer qualification.
- Component graph registration and role-whitelist/language-wall enforcement.
- Stable M0001-M0004 plans, current memory/progress, validated experience,
  templates, Codex-native expert skill packages, and a date-partitioned
  MetaFlux project work record.
- A stdlib-only Agent record/link/scope checker with credential-leak detection
  and an independent Nix fileset that does not enter runtime, provider, daemon,
  or toolchain packages.
- Canonical ownership documentation linked from
  [component map](../memory/component-map.md).

## Verified baseline

- Full dev, release, and ASan: 15/15 tests passed each (including the
  component-graph architecture gate).
- Provider preset: 10/10 tests passed.
- CUDA-only and NVML-only: 6/6 tests passed each.
- Project records, references, output hashes, and project-scope event types
  passed validation; re-validated after both 2026-08-28 revision rounds.
- `nix flake check path:. -L`, including `agent-records` and `entry-points`,
  passed at content revision `12efb20`; the dev preset passed 16/16 tests and
  the validator self-test passed 29/29 cases.
- At content revision `a40a475`, all 15 skill packages passed the bundled Codex
  validator; static routing checks found 15 unique slugs, 38 new references, 30
  single-skill cases, and five composition cases. `git diff --cached --check`,
  the Agent-record validator, and `nix flake check path:. -L` all passed. CTest
  was not rerun because the content change is confined to `agent/`.
- At content revision `235801e`, M0001-W01 was reconciled with D0009: Ubuntu
  20.04 LTS/glibc 2.31 is fixed, while the additional release distribution
  matrix may add qualified targets but may not raise the floor. This changes
  planning truth only; the release sysroot remains an open W01 deliverable.

## Next boundary

1. Close the unresolved decisions aggregated in
   [open decisions](../memory/open-decisions.md); the glibc baseline is closed
   by D0009, and each closure must move its row out of the ledger.
2. Close the remaining M0001-W01 exit conditions: the release provider sysroot,
   CUDA/NVML header acquisition, the LLVM 22 patchset, and reference-host
   performance qualification that promotes the provisional budgets to binding.
3. Then activate M0001-W02 according to its dependencies; do not treat present
   bootstrap functions as frozen production ABI.
4. Refresh this file and create a new checkpoint after any material
   acceptance-boundary change.
5. For domain work, invoke the smallest matching expert set from the
   [composition table](../skills/README.md); keep exact versions subordinate to
   the active plan, compiler epoch, and pinned headers.
