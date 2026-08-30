---
id: P20260828-005
status: Recorded
captured: 2026-08-28
milestone: M0100
workstream: W0101
branch: main
git_revision: 36edd9ece327ca1959b52c3cd844eaee4081e61b
workspace: skills layer committed; this checkpoint and its session record are committed afterward
---

# Skills Layer

Milestone: [M0100](../../../plan/M0100-core-foundation/plan.md). Workstream:
[W0101](../../../plan/M0100-core-foundation/work/W0101-build-toolchain.md).
This checkpoint records the state at revision
`36edd9ece327ca1959b52c3cd844eaee4081e61b`.

## Snapshot

`agent/` gains its fourth record type: expert skills as load-on-demand
procedural playbooks, machine-enforced like the rest.

| Aspect | Design |
| --- | --- |
| Form | `skills/<slug>/SKILL.md` with `name`/`description`/`status` frontmatter; slugs are link identity, never renamed |
| Boundary | skills prescribe machinery-enforced steps; experience validates world claims; templates are passive shapes |
| Seeds | [add-component](../../../skills/add-component/SKILL.md), [close-decision](../../../skills/close-decision/SKILL.md), [record-session](../../../skills/record-session/SKILL.md) |
| Enforcement | bidirectional index completeness, slug names, frontmatter presence; validator self-test at twenty cases |
| Found along the way | [templates index](../../../templates/README.md) written — it had never existed since bootstrap |

## Verification evidence

| Gate | Result |
| --- | --- |
| `tools/test-check-agent-records.py` | 20/20 cases passed |
| `tools/check-agent-records.py .` | ok (5 sessions, 50 events, 68 Markdown) |
| dev preset CTest | 16/16 passed |

## Resume notes

1. First command: `python3 tools/check-agent-records.py .`; read
   [skills](../../../skills/README.md) when a task matches one, then
   [open decisions](../../../memory/open-decisions.md) and
   [current progress](../../current.md).
2. Write a new skill only for a procedure already practiced at least once;
   the three seeds distill sessions S0100-20260828-001-spec-consistency through
   S0100-20260828-004-record-gate-hardening.
3. The next material boundary remains W0101: sysroot, CUDA/NVML header
   acquisition, the LLVM 22 patchset, and provisional AMD reference evidence;
   `v0.2.0` owns physical NVIDIA binding promotion.
4. Re-run the full preset matrix and `nix flake check path:.` before the next
   checkpoint.

Related work record:
[S0100-20260828-005-skills-layer](../../../sessions/2026/08/S0100-20260828-005-skills-layer/summary.md).
