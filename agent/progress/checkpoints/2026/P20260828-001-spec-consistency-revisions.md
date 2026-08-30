---
id: P20260828-001
status: Recorded
captured: 2026-08-28
milestone: M0100
workstream: W0101
branch: main
git_revision: 5360d51a09234f9753f260f218dc5a87e52c7eef
workspace: spec revision committed; this checkpoint and its session record are committed afterward
---

# Specification Consistency Revisions

Milestone: [M0100](../../../plan/M0100-core-foundation/plan.md). Workstream:
[W0101](../../../plan/M0100-core-foundation/work/W0101-build-toolchain.md).
This checkpoint records the state at spec revision
`5360d51a09234f9753f260f218dc5a87e52c7eef`, which sits on baseline
`9703559ef0056b6dd8ef5432b645a1362e72d734`.

## Snapshot

The first two Git commits exist. The scaffold is unchanged functionally: all
targets remain boundary fixtures. A self-review against the repository's own
aspirations produced five record-level amendments across 11 files (+69/-20):

| Amendment | Records |
| --- | --- |
| Synthetic NVIDIA identity in vroot is a presentation disguise, never a vendor ABI claim or vendor-driver match; registration/legal review gates release promotion | D0008 |
| Userspace glibc floor closed at 2.31 (Ubuntu 20.04); provider `DT_NEEDED` universe restricted to libc plus libpthread/libdl only where pre-2.34 targets require them | D0009 |
| `v0.1.0` memfd wake budget: cold syscalls free, at most one wake syscall per active dispatch, zero-syscall obligation begins with M0110 doorbell transports | [Architecture record](../../../../docs/architecture/control-and-data-plane.md) |
| Numeric performance budgets remain provisional for `v0.1.0`; the capture-time binding destination was the `v0.2.0` physical NVIDIA harness and archived baseline, which D0027 later supersedes with M1000 / `v1.0.0` | [M0100](../../../plan/M0100-core-foundation/plan.md) |
| Authoritative logical-device view scoped per managed domain; cross-vendor coexistence uses loader namespaces | [Project memory](../../../memory/project.md) |

M0100 remains Active with W0101 Active; M0100 performance budgets remain
provisional through `v0.1.0`. At capture time, binding promotion was assigned
to the `v0.2.0` physical NVIDIA harness; D0027 later supersedes only that
destination with M1000 / `v1.0.0`. M0120 decision 4 now
includes the synthetic-identity legal review.

## Verification evidence

| Gate | Result |
| --- | --- |
| Agent records (`tools/check-agent-records.py`) | Passed, including this checkpoint and session S0100-20260828-001-spec-consistency |
| Build-affecting changes | None; the only build-file touch is a comment in `tests/CMakeLists.txt` |

The prior P20260827-001 build matrix (dev 14/14, ASan 14/14, provider 9/9,
CUDA-only 5/5, NVML-only 5/5, `nix flake check`) remains the standing evidence
because no compile or link input changed.

## Resume notes

1. Inspect `git status` and `git log` before changing files; history starts at
   the 2026-08-27 baseline commit.
2. Read [current progress](../../current.md), then the relevant M0100
   workstream.
3. The next material boundary is closing the remaining M0100 decisions and the
   W0101 exit conditions (sysroot, header acquisition, LLVM 22 patchset, and
   provisional AMD reference evidence). At capture time, `v0.2.0` owned binding
   promotion; D0027 later supersedes only that destination with M1000 /
   `v1.0.0`.
4. Re-run the affected narrow gate and full `nix flake check path:.` before the
   next checkpoint.

Related work record:
[S0100-20260828-001-spec-consistency](../../../sessions/2026/08/S0100-20260828-001-spec-consistency/summary.md).
