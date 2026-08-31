---
id: P20260829-005
status: Recorded
captured: 2026-08-29
milestone: M0100
workstream: W0101
branch: main
git_revision: 267edc4cd1344906ec2a5d9d1e44b4c4eb7aeb35
workspace: content breakthroughs are committed; compact session and progress records are pending their separate record commit
---

# Stage Breakthrough Commit Policy

Active milestone: [M0100](../../../plan/M0100-core-foundation/plan.md). Active
workstream: [W0101](../../../plan/M0100-core-foundation/work/W0101-build-toolchain.md).

## Engineering state

`record-session` now distinguishes checkpoint mode from close mode. A content
commit is triggered when an independently nameable durable outcome has passing
focused gates, a coherent independently reviewable diff, and a following phase
with different scope or risk. The session can remain active after the commit;
its compact revision/evidence update is committed separately.

The pre-commit hook does not infer this boundary or create commits. It validates
record integrity and active-session coverage only after an agent or human has
chosen to invoke `git commit`.

## Applied checkpoints

| Revision | Durable boundary |
| --- | --- |
| `b05901a` | Ubuntu 20.04/glibc 2.31 release boundary, deterministic packages, ELF gates, and focused runtime regressions |
| `267edc4` | Stage-breakthrough detection and checkpoint/close workflow in repository Skills |

## Verification evidence

| Gate | Result |
| --- | --- |
| Skill Creator quick validation | `record-session` and `start-work` passed |
| Agent record validator | Passed before final record refresh |
| Skill routing static corpus | 11 domain skills and 68 cases passed |
| Skill routing self-test | 23/23 cases passed |
| Git pre-commit gate | Passed for both content revisions |

## Resume notes

1. Load `start-work` at task entry and `record-session` for durable changes.
2. Check the four breakthrough conditions after each focused verification gate.
3. Commit coherent content before expanding scope, then append and separately
   commit the compact session/checkpoint record while keeping a continuing
   session active.

Related work record:
[S0100-20260829-006-stage-breakthrough-commits](../../../sessions/liquidated-v1.json).
