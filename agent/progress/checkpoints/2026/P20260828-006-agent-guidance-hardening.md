---
id: P20260828-006
status: Recorded
captured: 2026-08-28
milestone: M0100
workstream: W0101
branch: main
git_revision: e2aebe96364eb653c1af2b896771c08910835fe7
workspace: guidance hardening committed; this checkpoint and its session record are committed afterward
---

# Agent Guidance Hardening

Milestone: [M0100](../../../plan/M0100-core-foundation/plan.md). Workstream:
[W0101](../../../plan/M0100-core-foundation/work/W0101-build-toolchain.md).
This checkpoint records the state at revision
`e2aebe96364eb653c1af2b896771c08910835fe7`.

## Snapshot

An agent that skips the repository rules can no longer commit unnoticed. Three
layers, each catching a different failure mode:

| Layer | Catches | Mechanism |
| --- | --- | --- |
| [AGENTS.md](../../../../AGENTS.md) | The uninformed but conforming agent | Root entry point read automatically by agentic tools; five hard rules |
| [start-work skill](../../../skills/start-work/SKILL.md) | The agent that reads rules but improvises the order | Cold-start procedure with a verification command |
| `.githooks/pre-commit` | The agent that reads nothing | Records validation, silent self-test, and session coverage: non-`agent/` staged changes require an in-progress session, error includes the scaffold command |

The hook installs itself via `core.hooksPath` when entering the Nix devshell
(python3 added to the shells). `--no-verify` remains an escape hatch; the nix
checks are the backstop.

## Verification evidence

| Gate | Result |
| --- | --- |
| Hook, staged non-agent changes, no session | Rejected exit 1 with scaffold instruction |
| Hook after scaffolding | Passed |
| Devshell rebuild (`nix develop path:. -c true`) | OK with python3 and hook wiring |
| Implementation commit | Hook ran live and passed |
| `tools/check-agent-records.py .` | ok (7 sessions, 57 events, 74 Markdown) |

## Resume notes

1. New agents read [AGENTS.md](../../../../AGENTS.md) first; the on-ramp is
   the [start-work skill](../../../skills/start-work/SKILL.md).
2. The next material boundary remains W0101: sysroot, CUDA/NVML header
   acquisition, the LLVM 22 patchset, and reference-host baselines — plus the
   three product spikes recommended in the 2026-08-28 improvement review.
3. Re-run the full preset matrix and `nix flake check path:.` before the next
   checkpoint.

Related work record:
[S0100-20260828-006-agent-guidance-hardening](../../../sessions/liquidated-v1.json).
