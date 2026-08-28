---
id: P20260828-001
status: Recorded
captured: 2026-08-28
milestone: M0001
workstream: M0001-W01
branch: main
git_revision: 5360d51a09234f9753f260f218dc5a87e52c7eef
workspace: spec revision committed; this checkpoint and its session record are committed afterward
---

# Specification Consistency Revisions

Milestone: [M0001](../../../plan/M0001-core-foundation/plan.md). Workstream:
[M0001-W01](../../../plan/M0001-core-foundation/work/W01-build-toolchain.md).
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
| v0.1 memfd wake budget: cold syscalls free, at most one wake syscall per active dispatch, zero-syscall obligation begins with M0002 doorbell transports | [Architecture record](../../../../docs/architecture/control-and-data-plane.md) |
| Numeric performance budgets carry provisional/binding status; binding requires the named harness and an archived baseline | [Agent README](../../README.md), [milestone template](../../../templates/milestone.md) |
| Authoritative logical-device view scoped per managed domain; cross-vendor coexistence uses loader namespaces | [Project memory](../../../memory/project.md) |

M0001 remains Active with M0001-W01 Active; M0001 performance budgets are
provisional pending the W01 reference-host harness. M0003 decision 4 now
includes the synthetic-identity legal review.

## Verification evidence

| Gate | Result |
| --- | --- |
| Agent records (`tools/check-agent-records.py`) | Passed, including this checkpoint and session S20260828-001 |
| Build-affecting changes | None; the only build-file touch is a comment in `tests/CMakeLists.txt` |

The prior P20260827-001 build matrix (dev 14/14, ASan 14/14, provider 9/9,
CUDA-only 5/5, NVML-only 5/5, `nix flake check`) remains the standing evidence
because no compile or link input changed.

## Resume notes

1. Inspect `git status` and `git log` before changing files; history starts at
   the 2026-08-27 baseline commit.
2. Read [current progress](../../current.md), then the relevant M0001
   workstream.
3. The next material boundary is closing the remaining M0001 decisions and the
   M0001-W01 exit conditions (sysroot, header acquisition, LLVM 22 patchset,
   reference-host baselines that promote budgets to binding).
4. Re-run the affected narrow gate and full `nix flake check path:.` before the
   next checkpoint.

Related work record:
[S20260828-001](../../../sessions/2026/08/S20260828-001-spec-consistency/summary.md).
