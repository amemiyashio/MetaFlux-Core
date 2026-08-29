# Session Summary

## Objective and outcome

Added a dedicated Ubuntu 20.04 target SDK construction and consumption manual
to `manage-toolchain`. The guide makes the current yellow release state
explicit: the complete SDK and matching generic LLVM closure already exist,
but the checked-in product release configuration still does not select them.

The documented target tuple was exercised in disposable builds. It produced
glibc-2.31-compatible providers and `metafluxd`, but this probe does not replace
the missing CMake-owned entry point, clean-revision packaging, or release
matrix evidence.

## Durable changes

- `agent/skills/manage-toolchain/SKILL.md`: requires the SDK guide for provider
  sysroot, complete SDK, generic LLVM, product-consumption, diagnosis, and
  qualification work.
- `agent/skills/manage-toolchain/references/ubuntu-20.04-target-sdk.md`: defines
  construction rules, ownership, the explicit CMake target tuple, packaging and
  qualification gates, provenance-verifier usage, and failure diagnosis.
- `agent/progress/current.md`: records the existing materializations, verified
  target-tuple probe, and still-open checked-in release wiring.

## Verification

| Command/gate | Result |
| --- | --- |
| Bundled skill `quick_validate.py` | Passed for `manage-toolchain` |
| `python3 -m json.tool toolchains/ubuntu-20.04-target-sdk-provenance.json` | Passed |
| Target SDK and generic LLVM `nix build --no-link --print-out-paths` | Both materializations resolved successfully |
| Disposable explicit-target provider build | CUDA provider at most `GLIBC_2.17`; NVML provider at most `GLIBC_2.14` |
| Disposable explicit-target `metafluxd` build | System loader, at most `GLIBC_2.29`, allowed system DSOs, no RPATH/RUNPATH, no `/nix/store` string |
| `python3 tools/check-agent-records.py .` | Passed before record finalization |
| `git diff --check` and staged equivalent | Passed |
| CTest | Not run; no product or test implementation changed |

## Cleanup

- Removed: all session-owned `/tmp/metaflux-sdk-guide*` configure and build
  trees, including failed diagnostic routes.
- Retained: no logs, evidence archives, duplicate sources, or GC roots. Existing
  content-addressed SDK and generic LLVM Nix outputs remain ordinary tool
  materializations.

## Decisions and experience

- Retained [D0009](../../../../memory/decisions-index.md) as the Ubuntu
  20.04/glibc 2.31 compatibility floor and D0022 as the tool-ownership boundary.
- Added no decision ID and closed no open-decision row; the guide applies those
  existing decisions.
- No experience record was promoted; the durable project-specific rules live
  in the skill reference.

## Distillation

- Distilled: SDK existence, product consumption, package validation, and
  distribution qualification are separate claims with separate owners.
- Distilled: `nix develop .#release` exposes tools but does not activate the
  Ubuntu target tuple.
- Distilled: a target-side `GLIBC_2.32` or newer result is diagnosed at the
  CMake consumer boundary before changing the SDK or weakening package gates.

## Unresolved items

- M0001-W01: add a checked-in CMake-owned target toolchain/preset or build
  driver that selects the complete documented tuple from a fresh build tree.
- M0001-W01: add a checked-in driver for the complete signed-provenance verifier
  argument set and wire it into the appropriate qualification owner.
- M0001-W01/W06: package and run the full digest-pinned release matrix twice
  from one clean Git revision before claiming single-revision reproducibility.

## Handoff

Read the [target SDK guide](../../../../skills/manage-toolchain/references/ubuntu-20.04-target-sdk.md),
then inspect `CMakePresets.json` and `nix/toolchains/generic-llvm-toolchain.nix`.
The next implementation starts by encoding the documented tuple in CMake
ownership; it does not add a Nix product derivation.
