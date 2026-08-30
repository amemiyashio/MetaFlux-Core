# Summary

Corrected the repository's toolchain and session boundaries after repeated raw
worktree evaluation copied generated build trees into the Nix store. D0022 now
limits Nix to fixed tool materialization and development shells; Git, CMake,
CTest, packaging, test harnesses, sessions, and the host retain their own
source, workflow, evidence, cleanup, and retention responsibilities.

The new `manage-toolchain` skill governs this boundary. Session governance was
rewritten as a curated work ledger with an explicit cleanup phase, while M0100
and W0101 were reduced to task and acceptance definitions that consume the
canonical `toolchains/` policy.

## Changed paths

- `toolchains/`, `flake.nix`, and `nix/`: established portable tool identity and
  reduced flake outputs to tools, development shells, and the formatter.
- `agent/skills/manage-toolchain/`, `agent/skills/record-session/`, Agent rules,
  templates, and validators: added toolchain governance and mandatory cleanup
  accounting without source snapshots.
- `agent/memory/`, M0100/W0101, and queued release work items: recorded D0022 and
  removed Nix-owned build, test, packaging, qualification, and GC language.
- `CMakePresets.json`, performance runners, packaging/test documentation, and
  architecture documentation: moved generated work outside the repository and
  returned each workflow to its owner.
- `nix/toolchains/default.nix` and `nix/shells/default.nix`: removed a duplicate
  Clang wrapper and Nix self-RPATH while retaining fixed tool-runtime libraries
  in the development shell.

## Verification

| Command/gate | Result |
| --- | --- |
| `nix flake show . --json` | Passed; only tool packages, four dev shells, and formatter are exposed |
| Clang, LLVM coverage, CMake, and Ninja version probes | Passed: 22.1.8, 22.1.8, 4.1.6, and 1.13.2 |
| `cmake --preset dev --fresh` and `cmake --build --preset dev` through the shell | Passed in repository-external build directory |
| `ctest --preset dev --output-on-failure` through the shell | 58/58 passed |
| Agent-record validator self-test | 54/54 cases passed |
| `python3 -B tools/check-agent-records.py .` | Passed before final record commit |
| M0100 optimization and performance runner self-tests | 12/12 and 12/12 passed |
| Bundled Codex `quick_validate.py` for `manage-toolchain` | Passed; independent forward test preserved owner routing |
| Nix format, JSON/Python syntax, link audit, and `git diff --check` | Passed; built ELF files contain no RPATH/RUNPATH |

## Decisions and experience

- [D0022](../../../../memory/decisions-index.md) supersedes D0021 and assigns
  tool identity to `toolchains/`, materialization to Nix, and all workflows and
  lifecycle policy to their existing owners.
- [E0001](../../../../experience/E0001-nix-untracked-flake.md),
  [E0002](../../../../experience/E0002-provider-closure-symbol-gates.md), and
  [E0003](../../../../experience/E0003-llvm-mlir-sdk-outputs-runpath.md) retain
  historical observations while marking broad Nix project checks as superseded.

## Cleanup

- Removed 5.7 GiB of ignored files from the old repository-local `build/`
  directory after confirming no CMake, Ninja, or CTest process was active.
- Deleted exactly 164 Nix-dead MetaFlux `*-source` paths that each contained an
  old `build/` tree and were at least 1 GiB. Nix reported 564.1 GiB reclaimed;
  no broad garbage collection or toolchain deletion was performed.
- Removed the 568 MiB external `.metaflux-build/MetaFlux-Core/dev` tree after
  final CTest and static verification. No session-owned build or evidence tree
  remains.

## roast

### light roasts

- none.

### medium roasts

- Curated session cleanup semantics -> `agent/skills/record-session/SKILL.md` (54/54 validator cases; no source snapshots)
- Nix untracked-worktree lesson -> `agent/experience/E0001-nix-untracked-flake.md` (recorded evidence; failed-route build trees and raw logs excluded)
- Provider closure and symbol-gate lesson -> `agent/experience/E0002-provider-closure-symbol-gates.md` (recorded evidence; failed-route build trees and raw logs excluded)
- LLVM/MLIR SDK output and RUNPATH lesson -> `agent/experience/E0003-llvm-mlir-sdk-outputs-runpath.md` (recorded evidence; failed-route build trees and raw logs excluded)

### dark roasts

- Tool ownership boundary -> `toolchains/README.md` (fixed tool identity and materialization gates verified above; authority: D0022, SC not required)

## session-only

- none.

## Unresolved items

- S0100-20260828-013-m0100-foundation and W0101 remain active. This correction
  does not promote the in-progress implementation to release-qualified status.
- General Nix-store retention remains host-operator policy; no repository GC
  timer, threshold, root, or semantic was introduced.

## Handoff

Read `toolchains/README.md`, invoke `manage-toolchain` only for tool identity or
materialization changes, and continue S0100-20260828-013-m0100-foundation
through the owner-specific CMake/CTest/packaging gates. Use
`nix develop . --command <owner command>` and remove exact session-owned
external work directories at handoff.
