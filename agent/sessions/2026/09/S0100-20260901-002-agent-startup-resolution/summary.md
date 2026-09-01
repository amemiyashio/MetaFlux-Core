# Session Summary

## Objective and outcome

Applied the bounded destructive SC0008 migration at content revision
`a7e370d68c660a1d42210631641bef0d3406cf91`. D0031 now owns stable
harness-product resolution, exact Codex identity, mandatory preflight, and
Nix-first executable entry and acquisition. Nix is restricted to clear,
reproducible, stable version identity, materialization, and exposure; later
governed toolchain revisions may evolve those versions. Product focus returns
to a newly scaffolded W0112 successor.

## Durable changes

- `docs/architecture/agent-startup-resolution.md`: Verified D0031 canonical
  startup order and no-compatibility boundary.
- `agent/skills/start-work/SKILL.md`: Stage Zero now precedes ordinary context
  loading and commands.
- `agent/skills/start-work/scripts/commit_as_harness.py`: reduced subject bound
  and non-harness identity-class rejection.
- `toolchains/README.md`: D0022 now carries the D0031 Nix-first amendment
  without transferring workflow ownership to Nix.
- `agent/semantic-changes/SC0008-agent-startup-resolution.md`: Applied migration
  record with synchronized current surfaces and preserved history.

## Verification

| Command/gate | Result |
| --- | --- |
| Harness helper and policy | Passed: 9/9 |
| Codex skill validator | Passed |
| Skill routing | Passed: 89 cases; self-test 34/34 |
| Agent records | Passed; self-test 197 cases |
| Semantic-change edit gate | Passed: 23/23 |
| Wrong subject / Codex preflight | Passed: model/template label rejected; `codex` resolved exactly |
| Nix-first probes | Passed: rg, Python, CMake, Ninja, and CTest |
| Content pre-commit | Passed at `7db7bcf`, `73b471f`, and `a7e370d` |

## Cleanup

- Removed: none.
- Retained: D0031, current skill/tooling, P100 product evidence, and historical
  Git objects as facts only.

## Decisions and experience

- D0031 amends D0022/D0028 and owns the new startup order. D0029/SC0008 own the
  destructive migration and exact handoff.
- Fixed tool versions are stable per revision, not permanently immutable;
  deliberate version changes remain owned by `manage-toolchain`.

## roast

### light roasts

- none.

### medium roasts

- none.

### dark roasts

- Destructive stable-harness and Nix-first startup resolution ->
  `docs/architecture/agent-startup-resolution.md` (`a7e370d`; helper/skill
  9/9, routing 89 plus 34/34, record self-test 197, semantic edits 23/23;
  authority: D0031, SC0008)

## session-only

- none.

## Unresolved items

- W0112: live `/dev/metafluxN` Add/Copy, registered-memory/DMA import,
  replacement-generation isolation, and Linux 6.12/6.18 fault qualification
  remain open.

## Handoff

Continue as
`S0112-20260901-003-m0110-w0112-post-startup-governance`. Read current focus,
current progress, D0031/start-work, and the W0112 Exit Gate. Resolve
`Agent harness subject: codex` from runtime instruction context and enter the
Git-aware Nix environment before repository executables.
