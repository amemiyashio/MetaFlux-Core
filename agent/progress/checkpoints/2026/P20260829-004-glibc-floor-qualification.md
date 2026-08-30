---
id: P20260829-004
status: Recorded
captured: 2026-08-29
milestone: M0100
workstream: W0101
branch: main
git_revision: f604fbea7f3bc2eaffb4c4448ef89607cd7ed697
workspace: glibc/package enforcement changes remain in the worktree; disposable build and matrix outputs were removed after recording
---

# Generic Linux glibc Floor Qualification

Active milestone: [M0100](../../../plan/M0100-core-foundation/plan.md). Active
workstream: [W0101](../../../plan/M0100-core-foundation/work/W0101-build-toolchain.md).

## Engineering state

D0009 is now enforced at the package boundary rather than being documentation
only. The canonical `packaging/build.py` stages an already-built CMake tree,
requires the Ubuntu 20.04 target SDK and matching generic-toolchain manifest
identities, and rejects non-system interpreters, RPATH/RUNPATH, non-system
`DT_NEEDED`, Nix store paths, and glibc symbols newer than `GLIBC_2.31`.
Release-side activation and CUDA acceptance fixtures receive the same gate.

The package and qualification rows remain one generic x86_64 artifact claim:
Ubuntu 20.04 is the compatibility floor, while Ubuntu 22.04, Ubuntu 24.04, and
Rocky 9 are additional runtime qualification environments. They do not create
additional SDKs or glibc targets.

## Verification evidence

| Gate | Result |
| --- | --- |
| Complete package matrix | 8/8 digest-pinned DEB/RPM/tar rows passed, including Ubuntu 20.04 interpreter, cold-JIT, warm-JIT, and AOT modes |
| Provider package matrix | 8/8 digest-pinned DEB/RPM/tar rows passed |
| Fixture ABI gate | Launcher max `GLIBC_2.14`; CUDA acceptance max `GLIBC_2.2`; system loader and dependency closure passed |
| Reproducibility | Independent builds produced identical DEB, RPM, and tar SHA-256 values |
| CTest | 65/65 tests passed, including daemon million-noop stress and release assertion regressions |
| Tool/record gates | Nix flake/tool probes, skill routing, and Agent record checks passed |

## Decisions and durable outcomes

- [D0009](../../../memory/decisions-index.md) remains the canonical fixed
  Ubuntu 20.04/glibc 2.31 floor.
- [D0022](../../../memory/decisions-index.md) remains unchanged: Nix
  materializes tools, while CMake, packaging, tests, and sessions own their
  respective workflows and semantics.

## Open work and risks

- At capture time, stock-tool, optimized-lowering, fault, quota, and soak gates
  still needed closure. D0023/D0024 later assign Intel host and native NixOS
  VM/package qualification to `v0.2.0`; neither is an M0100 exit gate. This
  checkpoint does not certify the entire milestone.

## Resume notes

1. Read D0009, W0101, and
   [`manage-toolchain`](../../../skills/manage-toolchain/SKILL.md) before
   changing target inputs or shell materialization.
2. Build generic artifacts with the Ubuntu 20.04 target tuple, then run
   `python3 packaging/build.py` and the digest-pinned release harnesses.
3. Keep matrix JSON, package files, and external build trees disposable; retain
   only their compact result in the session and progress records.

Related work record:
[S0100-20260829-005-m0100-vertical-slice](../../../sessions/2026/08/S0100-20260829-005-m0100-vertical-slice/summary.md).
