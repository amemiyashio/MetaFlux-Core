---
id: P20260901-100
status: Recorded
captured: 2026-09-01
milestone: M0110
workstream: W0112
branch: main
git_revision: 95c960d1ca4fded19cd330303076b63c59deae36
workspace: live cdev qualification entry point
---

# M0110 W0112 Live Cdev Qualification Entry Point

## Outcome

Revision `95c960d` adds a CTest-integrated live cdev qualification executable.
It exercises the generated ioctl and mmap ABI, queue metadata, poll state,
eventfd-backed worker leasing, payload query and dual mapping, non-aligned
long-term memory registration with verified unregister, unknown ioctl and
reserved-byte rejection, stale-generation allocation, and owner-close queue
and payload VMA tombstones.

The gate returns the CTest skip code when `/dev/metaflux0` is absent or the
exclusive worker lease is busy. A skip is environment evidence only and never
counts as W0112 live qualification.

The local identity correction replaced revisions `047ea94`, `bdf93ca`, and
`2da2b13` with tree-equivalent current-branch revisions `0404481`, `e4caf75`,
and `95c960d`, respectively. P099 remains immutable capture-time evidence of
the original revision; this checkpoint is the current resume boundary.

## Verification evidence

| Gate | Result |
| --- | --- |
| Content tree preservation | Passed: old and replacement HEAD tree `936d5efcd3a54c5c8276bf28b60de8611a11c3e1` |
| Harness identity | Passed: all three replacement commits use `Agent Harness (codex) <codex@localhost>` for Author and Committer |
| Development build | Passed |
| Full development CTest | Passed: 85 executable tests; live cdev gate explicitly skipped |
| Live gate classification | Passed: `/dev/metaflux0` absence returns code 77 and CTest records `Skipped` |
| Linux 6.18 core Kbuild | Passed; existing compiler-version warning only |
| Agent records | Passed before checkpoint record |

## Cleanup

- Removed: temporary history-rewrite branch and stash after exact tree and
  identity verification.
- Retained: current W0112 source, live test, plan, and current-epoch session;
  old commit objects remain recoverable only through ordinary Git reflog/GC
  policy and are not branch execution authority.

## roast

### light roasts

- none.

### medium roasts

- Executable live cdev UAPI and VMA qualification gate ->
  `agent/plan/M0110-kernel-guest-transport/work/W0112-local-cdev.md`
  (`95c960d`; 85 executable CTest passes plus one explicit missing-device skip)

### dark roasts

- none.

## session-only

- `/dev/metafluxctl` and `/dev/metaflux0` are absent on this host, so the live
  gate was skipped and provides no Add/Copy or kernel lifetime acceptance.

## Handoff

Continue as `S0112-20260901-001-m0110-w0112-current-epoch`. The next product
unit remains activated-device Add/Copy and the Linux 6.12/6.18 fault matrix.
