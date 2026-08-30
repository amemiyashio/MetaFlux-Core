# Session Summary

## Objective and outcome

Advanced M0100 to a verified CPU-backed CUDA/NVML Add/Copy vertical slice and
closed the generic Linux glibc enforcement gap. Ubuntu 20.04/glibc 2.31 is the
single compatibility floor; Ubuntu 22.04, Ubuntu 24.04, and Rocky 9 are frozen
qualification rows rather than separate ABI targets. The complete and provider
package paths now construct deterministic artifacts and reject ABI/provenance
drift before the offline matrix runs.

## Durable changes

- `compiler/core/src/artifact_cache.cpp` and its test: administrator AOT
  publication no longer depends on mutable-cache global state.
- `services/metafluxd/execution.cpp` and `services/metafluxd/tests/`: AOT
  prewarm stays on the administrator tier and the million-noop daemon stress
  test is part of CTest.
- `plugins/backend/cpu/compiler/`: runtime LLD receives an explicit single-
  thread resource limit with a regression check.
- `packaging/build.py`: canonical deterministic DEB/RPM/tar builder with
  target-manifest identity checks and system ELF/glibc gates.
- `packaging/`, `toolchains/`, and `agent/skills/manage-toolchain/SKILL.md`:
  document and enforce the Ubuntu 20.04/glibc 2.31 target boundary, including
  private cache modes and current-timezone mirror routing.
- `tests/release/`: complete/provider package matrices now validate release
  fixtures with the same loader, dependency, path, and glibc ceiling checks.

## Verification

| Command/gate | Result |
| --- | --- |
| `ctest --test-dir .../m0100-integration --output-on-failure` | 65/65 passed |
| Complete offline package matrix | 8/8 rows passed; Ubuntu 20.04 fixture modes and AOT passed |
| Provider offline package matrix | 8/8 rows passed |
| Reproducibility rebuild | DEB, RPM, and tar SHA-256 matched across two staging roots |
| `python3 tests/release/test_release_matrix_assertions.py` | Passed, including GLIBC_2.32 rejection |
| `python3 tools/check-agent-records.py .` | Passed before final record refresh |
| `python3 tools/check-skill-routing.py .` and self-test | Passed; 23/23 routing cases |
| `nix flake show .` and tool probes | Passed; Clang 22.1.8, CMake 4.1.6, Ninja 1.13.2, RPM 4.20.1 |

## Cleanup

- Removed: session-owned `/tmp/metaflux-complete-artifacts.flDSmU`,
  `/tmp/metaflux-provider-artifacts.Z45tmJ`, `/tmp/metaflux-release-inputs.8LPiUv`,
  final/repeated matrix directories, reproducibility staging directories, and
  external `m0100-integration`/`m0100-generic-release` build trees after their
  results were summarized here.
- Retained: none; package and matrix outputs are reproducible disposable work,
  not session archives.

## Decisions and experience

- D0009 remains the canonical Ubuntu 20.04/glibc 2.31 floor; D0022 remains the
  Nix tool-provider boundary. No new decision ID was needed.
- No experience record was needed; the reusable enforcement belongs in the
  packaging and release harnesses.

## roast

### light roasts

- D0009 target-build rules and Ubuntu 20.04/glibc 2.31 boundary ->
  toolchains/README.md (`test_release_matrix_assertions.py` passed including
  GLIBC_2.32 rejection)
- Tool materialization procedure applies private-cache and current-timezone
  mirror routing at the D0009 floor -> agent/skills/manage-toolchain/SKILL.md
  (Nix flake show and tool probes passed)
- Deterministic DEB/RPM/tar construction rejects manifest and system ELF/glibc
  drift -> packaging/build.py (DEB, RPM, and tar SHA-256 matched across two
  staging roots)
- Complete and provider release fixtures apply loader, dependency, path, and
  glibc ceiling checks -> tests/release/run_release_package_matrix.py (complete
  and provider offline matrices each passed 8/8)
- Administrator AOT cache publication no longer depends on mutable-cache global
  state -> compiler/core/src/artifact_cache.cpp (M0100 integration CTest passed
  65/65)
- Daemon AOT prewarm remains on the administrator tier ->
  services/metafluxd/execution.cpp (million-noop daemon stress ran in the M0100
  integration CTest that passed 65/65)
- Runtime LLD receives an explicit single-thread resource limit ->
  plugins/backend/cpu/compiler/src/compiler.cpp (M0100 integration CTest passed
  65/65)

### medium roasts

- none.

### dark roasts

- none.

## session-only

- none.

## Unresolved items

- At this session's capture time, W0101 remained active for Intel/AMD reference-
  host qualification, the native NixOS package row, stock-tool and optimized-
  lowering qualification, and remaining fault, quota, and soak gates. The
  generic package claim was verified for the rows executed here but was not the
  entire M0100 release gate. D0027 later assigned Intel support qualification to
  M1000 / `v1.0.0`; native NixOS qualification remains assigned to the
  unallocated `v0.2.0` expansion. That later destination split does not change
  any test result or create qualification evidence for this session.

## Handoff

Read [`agent/progress/current.md`](../../../../progress/current.md), D0009, and
W0101, then reload the smallest matching expert skill. Start with
`python3 tools/check-agent-records.py .`; use `packaging/build.py` and the
digest-pinned release harnesses for any subsequent generic artifact claim.
