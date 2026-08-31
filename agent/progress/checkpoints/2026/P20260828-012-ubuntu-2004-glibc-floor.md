---
id: P20260828-012
status: Recorded
captured: 2026-08-28
milestone: M0100
workstream: W0101
branch: main
git_revision: 235801e7c8710cea2086a26d3a3a7b64c11d7cf1
workspace: W0101 content committed; concurrent S0100-20260828-011-skill-design-convergence work and pre-existing staged changes remain outside this checkpoint
---

# Ubuntu 20.04 Userspace Floor

Milestone: [M0100](../../../plan/M0100-core-foundation/plan.md). Active
workstream:
[W0101](../../../plan/M0100-core-foundation/work/W0101-build-toolchain.md).

## Snapshot

The owner reaffirmed D0009 and the W0101 source now agrees: Ubuntu 20.04 LTS is
the minimum supported userspace distribution and glibc 2.31 is the fixed ABI
floor. Generic providers are required to use the corresponding release sysroot
and may not reference symbols newer than `GLIBC_2.31`.

The open release distribution matrix may add qualification targets but may not
raise this floor. No sysroot, package, runtime code, or implementation maturity
changed in this checkpoint.

## Verification evidence

| Gate | Result |
| --- | --- |
| `python3 -B tools/check-agent-records.py .` | Passed before record finalization: 13 sessions, 102 events, 147 Markdown files |
| Scoped `git diff --check` | Passed |
| Active-source consistency search | All active references agree on Ubuntu 20.04/glibc 2.31; no candidate wording remains |
| CTest | Not run; content change is confined to `agent/` planning records |

## Decisions and durable outcomes

- [D0009](../../../memory/decisions-index.md) remains the decision identity;
  this checkpoint synchronizes W0101 rather than creating a duplicate decision.

## Open work and risks

- W0101 must implement the Ubuntu 20.04/glibc 2.31 provider sysroot and
  enforce the `GLIBC_2.31` ceiling on generic artifacts.
- The broader release distribution qualification matrix remains open.

## Resume notes

1. Read D0009 and W0101 before editing Nix or release packaging.
2. Keep Nix host glibc out of generic release artifacts.
3. Validate the symbol ceiling with `readelf --version-info` and the complete
   release closure before closing the W0101 sysroot gate.

Related work record:
[S0101-20260828-012-ubuntu-2004-glibc-floor](../../../sessions/liquidated-v1.json).
