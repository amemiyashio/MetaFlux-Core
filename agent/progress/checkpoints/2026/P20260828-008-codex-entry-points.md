---
id: P20260828-008
status: Recorded
captured: 2026-08-28
milestone: M0001
workstream: M0001-W01
branch: main
git_revision: 3fe0ace283c857f926b4d9a9555438a5df7e835e
workspace: Agent entry-point check committed; this checkpoint and its session record are committed afterward
---

# Codex Entry-Point Verification

Milestone: [M0001](../../../plan/M0001-core-foundation/plan.md). Workstream:
[M0001-W01](../../../plan/M0001-core-foundation/work/W01-build-toolchain.md).
This checkpoint records the state at revision
`3fe0ace283c857f926b4d9a9555438a5df7e835e`.

## Snapshot

Codex receives the repository rules through its native root
[`AGENTS.md`](../../../../AGENTS.md) discovery. The optional
[`CLAUDE.md`](../../../../CLAUDE.md) and `.claude/` hooks remain a Claude Code
bridge, not a Codex hook layer. Tool-independent enforcement remains the
pre-commit gate and Nix checks.

`checks.x86_64-linux.entry-points` now copies the independent Agent-record
fileset into a synthetic Git repository and proves:

- `AGENTS.md` is present and non-empty;
- `CLAUDE.md` retains its `@AGENTS.md` import;
- `.githooks/pre-commit` is present and executable;
- `.claude/settings.json` and both Python hooks survive the fileset;
- the copied source passes `tools/check-agent-records.py`, including all
  Markdown-link dependencies.

The project remains an engineering scaffold; this workflow check does not
promote any CUDA, NVML, runtime, compiler, or backend fixture to functional
implementation.

## Verification evidence

| Gate | Result |
| --- | --- |
| `nix build path:.#checks.x86_64-linux.entry-points -L` | Passed |
| Validator self-test | 23/23 cases |
| Dev preset | 16/16 tests |
| `nix flake check path:. -L` | All checks passed |
| Agent records | Passed before record commit |

## Resume notes

1. Begin with `python3 tools/check-agent-records.py .`; this checkpoint is
   immutable.
2. Codex follows root `AGENTS.md`; do not add a Codex-specific copied rulebook.
3. The next M0001-W01 product boundaries remain the release provider sysroot,
   CUDA/NVML header acquisition, LLVM 22 patchset, and reference-host
   qualification.

Related work record:
[S20260828-008](../../../sessions/2026/08/S20260828-008-codex-entry-points/summary.md).
