# Summary

Removed the remaining active and physical residue of the superseded Nix
product-workflow route. D0022 now holds across current documentation, packaging,
tests, performance qualification, lifecycle evidence, and Agent handoffs.
Historical records remain intact, and fixed tool materializations remain usable.

## Durable changes

- `packaging/nixos/`: requires callers to inject the packaging-owned daemon
  package instead of reaching into the repository tool flake.
- `tests/`, W0106, and the CPU performance reference: assign build, test,
  PGO evidence, and benchmark ownership to their actual CMake/CTest/test and
  packaging workflows.
- W0121 and its lifecycle skill reference: write model evidence under the
  repository-adjacent `.metaflux-evidence` hierarchy.
- Agent/Claude/Git entry points: remove stale Nix-check installation language,
  expose obsolete root build/result routes, and define checkpoints as compact
  engineering handoffs rather than source history.

## Verification

| Command/gate | Result |
| --- | --- |
| Nix parse/format, Bash/Python syntax, `git diff --check` | Passed |
| Agent records and validator self-test | Passed; 54/54 self-test cases |
| Skill routing corpus and self-test | Passed; 68 cases and 23/23 self-tests |
| `nix flake show . --no-write-lock-file` | Tools, four development shells, and formatter only |
| Development-shell probes | Clang 22.1.8, CMake 4.1.6, Ninja 1.13.2 |
| NixOS module evaluation | Explicit package accepted; missing package rejected |
| Independent residue/diff review | Passed; no active D0022 conflict found |

## Cleanup

- Removed at least 2,749,767,680 bytes (2.56 GiB) of exact project temporary
  paths, Python caches, old `result*` roots, and empty superseded route
  directories.
- Deleted one prevalidated 1,294-path dead Nix referrer closure with digest
  `ab9b8811e981483d61e18f93eabb54b0ca35f826e0943635705f01115f14ab07`;
  Nix reported 26.1 GiB freed. Final validation-only source/derivation paths
  were also removed. No broad garbage collection ran.
- Retained 104 legitimate MetaFlux tool materializations, about 2.1 GiB. Final
  checks found no old product seed, MetaFlux source copy, project GC root,
  project temporary path, repository build/result route, or Python cache.

## Decisions and experience

- [D0022](../../../../memory/decisions-index.md) remains authoritative; this
  cleanup applied it and introduced no new architectural decision.
- Historical Nix observations remain in E0001-E0003 and completed records; they
  are not active workflow entry points.

## Distillation

- Distilled remaining ownership corrections into their active owner documents
  and the future checkpoint template; no failed-route artifact was promoted.

## Unresolved items

- W0101 and W0106 remain active: release matrices, stock-tool runs,
  reference-host evidence, optimized lowering, fault, quota, and performance
  gates are still open.
- Host-wide retention of unrelated Nix paths remains operator policy.

## Handoff

Read [current progress](../../../../progress/current.md), D0022, M0100, and the
smallest matching domain skill. Enter fixed tools with `nix develop .`, run each
workflow through its owner, and remove only exact session-owned generated paths
at handoff.
