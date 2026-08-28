---
id: P20260828-013
status: Recorded
captured: 2026-08-28
milestone: M0001
workstream: M0001-W01
branch: main
git_revision: 497a58fb8d54da5ea569bb894736de98c58d8fb8
workspace: expert-skill and routing content committed; this checkpoint records cross-milestone architecture guidance without advancing queued implementation status
---

# Expert Skill Design Convergence

Active milestone: [M0001](../../../plan/M0001-core-foundation/plan.md). Active
workstream:
[M0001-W01](../../../plan/M0001-core-foundation/work/W01-build-toolchain.md).
The skill changes also refine queued M0001-W02 and M0002-M0004 planning.

## Snapshot

The repository now has 16 validated Codex skill packages. Eleven domain experts
have explicit, non-overlapping ownership across neutral runtime contracts,
CUDA/NVML compatibility, PTX semantics, MLIR mechanics, CPU execution, Linux
UAPI, vfio-user, PCI presentation, lifecycle, and Vulkan. The added
`runtime-contracts-registry` package owns registry views, client negotiation,
shared layouts, backend C ABI, and canonical cross-layer schema generation.

Package and routing expectations are machine-enforced. The structured bilingual
corpus contains 68 cases for the 11 domain skills, including 13 compositions;
the observation scorer keeps real model/host routing evidence separate from
static corpus validity. M0001-W02 and M0003 planning now specify publication,
admission, range reserve/retire, telemetry, exact recovery, close reserve, and
live-writer quarantine invariants consistently.

This checkpoint records architecture and activation guidance. It does not claim
that queued runtime, transport, lifecycle, compatibility, or Vulkan behavior is
implemented or release-qualified.

## Verification evidence

| Gate | Result |
| --- | --- |
| Bundled Codex `quick_validate.py` | Passed for all 16 packages |
| `python3 -B tools/check-agent-records.py .` | Passed before record finalization: 13 sessions, 105 events, 148 Markdown files |
| `python3 -B tools/test-check-agent-records.py` | 52 cases passed |
| `python3 -B tools/check-skill-routing.py .` | 11 domain skills and 68 cases passed |
| `python3 -B tools/test-check-skill-routing.py` | 23/23 cases passed |
| `nix develop path:. -c ctest --preset dev --output-on-failure` | 18/18 tests passed |
| `nix flake check path:. -L` | Passed |
| `git diff --check` and staged equivalent | Passed |

## Decisions and durable outcomes

- [D0008](../../../memory/decisions-index.md) remains the decision identity;
  its Linux scan-versus-bind wording is clarified without permitting vendor
  driver matching or pre-bind `ONLINE` publication.
- Neutral runtime/contracts ownership, single-owner composition routing, and
  lifecycle publication invariants are durable in the skill catalog and active
  milestone plans.
- No open decision was closed and no product maturity status advanced.

## Open work and risks

- Run and archive the 68-case observation corpus three times for every Codex
  model/host combination that is to be behaviorally qualified.
- Implement and measure the queued workstreams before using these planning
  contracts as release evidence.
- M0001-W01 still owns the release sysroot, header acquisition, LLVM patchset,
  and reference-host performance gates before M0001-W02 activation.

## Resume notes

1. Start with the smallest owner set in `agent/skills/README.md`.
2. For shared registry or publication changes, read M0001-W02 and
   `runtime-contracts-registry` before changing schema or state.
3. Keep static routing validation and captured model behavior as separate
   evidence classes.

Related work record:
[S20260828-011](../../../sessions/2026/08/S20260828-011-skill-design-convergence/summary.md).
