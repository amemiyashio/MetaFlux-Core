---
id: P20260828-007
status: Recorded
captured: 2026-08-28
milestone: M0100
workstream: W0101
branch: main
git_revision: c4ccd6117573316054822f48e0c8b6d47c9cda8b
workspace: Claude bridge committed; this checkpoint and its session record are committed afterward
---

# Claude Code Bridge

Milestone: [M0100](../../../plan/M0100-core-foundation/plan.md). Workstream:
[W0101](../../../plan/M0100-core-foundation/work/W0101-build-toolchain.md).
This checkpoint records the state at revision
`c4ccd6117573316054822f48e0c8b6d47c9cda8b`.

## Snapshot

Claude Code now sees the repository rules through its own entry file and hits
edit-time guards, entirely within the repository:

| Piece | Design |
| --- | --- |
| [`CLAUDE.md`](../../../../CLAUDE.md) | One-line `@AGENTS.md` import; single rulebook, no drift (validator-enforced) |
| `.claude/hooks/pre_edit.py` | PreToolUse guard: checkpoint rewrites blocked (immutable history); non-`agent/` edits blocked without an in-progress session; denials carry remediation commands; fails open on schema drift |
| `.claude/hooks/session_start.py` | SessionStart banner routing into AGENTS.md |
| Scope | Documented in [`.claude/README.md`](../../../../.claude/README.md): nothing global, inert for non-Claude tools, per-user hook approval, pre-commit + Nix checks remain authoritative |

## Verification evidence

| Gate | Result |
| --- | --- |
| Guard: checkpoint edit / non-agent edit without session | Denied, exit 2 |
| Guard: agent/ edit / malformed stdin | Allowed (fail-open) |
| Guard: non-agent edit after scaffolding | Allowed |
| SessionStart banner | Printed |
| Validator self-test | 23/23 cases |
| Repository records / dev preset | ok (8 sessions) / 16/16 |

## Resume notes

1. Non-Claude contributors are unaffected; the bridge is opt-in and
   repository-local.
2. The next material boundary remains W0101: sysroot, CUDA/NVML header
   acquisition, the LLVM 22 patchset, reference-host baselines, and the three
   product spikes from the improvement review.
3. Re-run the full preset matrix and `nix flake check path:.` before the next
   checkpoint.

Related work record:
[S0100-20260828-007-claude-bridge](../../../sessions/liquidated-v1.json).
