---
id: P20260828-009
status: Recorded
captured: 2026-08-28
milestone: M0100
workstream: W0101
branch: main
git_revision: 12efb20e52e215fdfc70a3d4a59c2fc46bfcf4d9
workspace: Codex skill package migration committed; this checkpoint and its session record are committed afterward
---

# Codex Skill Packages and Readiness Guidance

Milestone: [M0100](../../../plan/M0100-core-foundation/plan.md). Workstream:
[W0101](../../../plan/M0100-core-foundation/work/W0101-build-toolchain.md).
This checkpoint records the state at revision
`12efb20e52e215fdfc70a3d4a59c2fc46bfcf4d9`.

## Snapshot

The repository expert-skill layer now follows the official Codex package
contract. [`agent/skills`](../../../skills/README.md) remains the physical
catalog so historical links stay valid; the repository-root
[`.agents/skills`](../../../../.agents/skills) symlink is the native Codex
discovery path.

Each package requires uppercase `SKILL.md` with `name` and `description`.
Codex-standard optional frontmatter and `agents/openai.yaml`, `scripts/`,
`references/`, and `assets/` are accepted. The repository's Draft/Active/Retired
lifecycle remains in the catalog and is no longer a nonstandard frontmatter
field. Existing minimal `SKILL.md`-only packages remain valid.

The new
[implementation-readiness](../../../skills/implementation-readiness/SKILL.md)
package provides a sourced method for deciding whether implementation may
start and what boundary comes next. It independently grades architecture,
workstream activation, implementation maturity, and release/operations; it
classifies open decisions by latest safe closure point and requires executable
fitness evidence for important quality attributes.

This checkpoint does not promote any CUDA, NVML, runtime, compiler, transport,
or backend fixture to functional implementation. W0101 remains active.

## Verification evidence

| Gate | Result |
| --- | --- |
| Bundled Codex `quick_validate.py` | 5/5 skill packages passed |
| Validator self-test | 29/29 cases passed |
| Agent records | Passed before content commit |
| Isolated Nix Agent-record and entry-point checks | Passed |
| Dev preset | 16/16 tests passed |
| `nix flake check path:. -L` | All checks passed |

## Resume notes

1. Begin with `python3 tools/check-agent-records.py .`; this checkpoint is
   protected. Append corrections by default; use an exact committed D0025/SC
   row only for authorized semantic synchronization.
2. Invoke `$implementation-readiness` for future architecture/workstream
   readiness reviews; do not infer implementation maturity from scaffold
   completeness.
3. The next W0101 product boundaries remain the release provider sysroot,
   CUDA/NVML header acquisition, LLVM 22 patchset, and reference-host
   qualification.

Related work record:
[S0101-20260828-009-implementation-readiness-skill](../../../sessions/liquidated-v1.json).
