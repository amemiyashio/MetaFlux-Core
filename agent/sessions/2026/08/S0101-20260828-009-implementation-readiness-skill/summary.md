# Summary

Migrated the repository expert-skill layer to the official Codex package
contract without breaking existing `agent/skills` links. The packages now use
standard `SKILL.md` frontmatter, may carry Codex's optional metadata and resource
directories, and are natively discovered through `.agents/skills`.

Added the `implementation-readiness` expert skill with an evidence framework
grounded in architecture tradeoff analysis, reliable launch practice,
technology maturity, and evolutionary architecture. It keeps architecture,
workstream activation, implementation maturity, and release readiness separate
so a strong scaffold is not mistaken for a finished product.

## Changed paths

- `.agents/skills`: Codex-native repository discovery symlink to the durable
  `agent/skills` catalog.
- `agent/skills/README.md`, the four existing `SKILL.md` files, and
  `agent/README.md`: standard package contract and catalog-owned lifecycle.
- `agent/skills/implementation-readiness/`: standard package, optional Codex UI
  metadata, and sourced readiness evidence framework.
- `tools/check-agent-records.py` and `tools/test-check-agent-records.py`:
  standard-compatible frontmatter, package/name checks, discovery drift checks,
  and 29 synthetic cases.
- `nix/lib/source.nix` and `nix/checks/default.nix`: retain and prove the native
  skill entry point in isolated Agent-record sources.
- This session, `progress/current.md`, and `P20260828-009`: exact work record and
  protected handoff checkpoint.

## Verification

| Command/gate | Result |
| --- | --- |
| Bundled `skill-creator/scripts/quick_validate.py` over all packages | 5/5 skills passed |
| `python3 tools/test-check-agent-records.py` | 29/29 cases passed |
| `python3 tools/check-agent-records.py .` | Passed before content commit |
| Isolated Nix `agent-records` and `entry-points` builds | Passed |
| Dev configure/build plus `ctest --preset dev --output-on-failure` | 16/16 tests passed |
| `nix flake check path:. -L` | All checks passed |

## Decisions and experience

- No DNNNN decision or ENNNN experience record changed. This migration aligns
  repository tooling with Codex's package contract and adds task guidance; it
  does not alter product architecture or acceptance scope.

## Distillation

- Promoted: Codex package shape and native discovery boundary ->
  `agent/skills/README.md`, validator, and Nix entry-point check (session
  verification above).
- Promoted: implementation-readiness method -> new skill and its reference
  (research evidence recorded by the session).
- Session-only: none; the method was not duplicated into product memory.

## Unresolved items

- No skill-format work remains. W0101 retains its existing release sysroot,
  CUDA/NVML header acquisition, LLVM patchset, and reference-host qualification
  boundaries.

## Handoff

Start with `python3 tools/check-agent-records.py .`. Invoke
`$implementation-readiness` for the next architecture/workstream readiness
review, then read [current progress](../../../../progress/current.md) and
[P20260828-009](../../../../progress/checkpoints/2026/P20260828-009-codex-skill-packages.md).
