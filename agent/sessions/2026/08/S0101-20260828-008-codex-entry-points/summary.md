# Summary

Verified that Codex receives the repository rules through native `AGENTS.md`
discovery and completed the interrupted Nix entry-point check. The independent
Agent-record source now contains every repository-local entry and validator
dependency, while `checks.x86_64-linux.entry-points` proves the copied source
retains the Codex rules, tool-agnostic pre-commit gate, and optional Claude
bridge.

The check uses a real synthetic Git root. It treats `.githooks/pre-commit` as
an executable entry point, but correctly treats the Claude Python hooks as
ordinary files invoked by `python3` through `.claude/settings.json`.

## Changed paths

- `nix/lib/source.nix`: include `AGENTS.md`, the optional Claude bridge,
  `.githooks`, the validator self-test, and the component-graph checker in the
  independent `agentRecords` fileset.
- `nix/checks/default.nix`: add and export the isolated `entry-points` check.
- `agent/sessions/2026/08/S0101-20260828-008-codex-entry-points/`: exact work record.
- `agent/progress/current.md` and `agent/progress/checkpoints/2026/P20260828-008-codex-entry-points.md`:
  refreshed resume point and immutable verification snapshot.

## Verification

| Command/gate | Result |
| --- | --- |
| `nix build path:.#checks.x86_64-linux.entry-points -L` | Passed |
| `python3 tools/test-check-agent-records.py` | 23/23 cases passed |
| `nix develop path:. -c ... ctest --preset dev --output-on-failure` | 16/16 tests passed |
| `nix flake check path:. -L` | All checks passed |
| `python3 tools/check-agent-records.py .` | Passed before record commit |

## Decisions and experience

- No new DNNNN decision or ENNNN experience record. This change enforces
  existing repository workflow boundaries and does not alter product scope.

## Distillation

- Distilled: none. Codex's native `AGENTS.md` behavior is external tool
  documentation; the repository-specific guarantee is embodied directly in
  the Nix check and checkpoint rather than duplicated into durable memory.

## Unresolved items

- No entry-point work remains. W0101 retains its existing sysroot,
  header-acquisition, LLVM patchset, and reference-host qualification work.

## Handoff

Start with `python3 tools/check-agent-records.py .`, then read
[current progress](../../../../progress/current.md) and
[P20260828-008](../../../../progress/checkpoints/2026/P20260828-008-codex-entry-points.md).
