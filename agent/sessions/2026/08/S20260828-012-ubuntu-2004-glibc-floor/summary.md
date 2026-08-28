# Summary

Reconciled M0001-W01 with the owner's minimum-support ruling and existing
D0009. Ubuntu 20.04 LTS is now explicit as the minimum supported userspace
distribution, glibc 2.31 is the fixed ABI floor, and generic providers must be
built against that release sysroot without symbols newer than `GLIBC_2.31`.

The broader release distribution matrix remains open only to add qualified
targets; it may not raise the fixed floor. This records a plan correction, not
evidence that the release sysroot or generic packages have been implemented.

## Changed paths

- `agent/plan/M0001-core-foundation/work/W01-build-toolchain.md`: removed the
  stale candidate wording and fixed the sysroot/symbol-ceiling requirement.
- `agent/progress/current.md`, `agent/progress/README.md`, and
  `agent/progress/checkpoints/2026/P20260828-012-ubuntu-2004-glibc-floor.md`:
  refreshed the durable handoff.
- `agent/sessions/README.md` and this session directory: recorded the work.

## Verification

| Command/gate | Result |
| --- | --- |
| `python3 -B tools/check-agent-records.py .` | Passed before record finalization: 13 sessions, 102 events, 147 Markdown files |
| Scoped `git diff --check` | Passed |
| Active-source consistency search | D0009, constraints, M0001, W01, current progress, and tests agree on Ubuntu 20.04/glibc 2.31 |
| CTest | Not run; content change is confined to `agent/` planning records |

## Decisions and experience

- Retained [D0009](../../../../memory/decisions-index.md) as the canonical
  decision. No new decision ID or ledger-row closure was needed.
- No experience record was created; this is a project-specific baseline.

## Distillation

- Distilled: removed the sole stale candidate statement from M0001-W01 and
  made its release-sysroot consequence explicit. No new durable constraint was
  added because D0009 and `memory/constraints.md` already carried the ruling.

## Unresolved items

- M0001-W01 still needs the Ubuntu 20.04/glibc 2.31 release sysroot and an
  enforced `GLIBC_2.31` symbol-ceiling check.
- M0001's additional release distribution qualification matrix remains open.

## Handoff

Read D0009 and M0001-W01, then implement the release sysroot independently of
the Nix host glibc and validate generic artifacts with `readelf
--version-info`.
