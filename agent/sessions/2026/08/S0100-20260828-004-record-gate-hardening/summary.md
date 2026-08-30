# Summary

Closed the two worthwhile small items from the agent/ re-assessment, hardening
the record gate itself. No product code changed.

Dual-source consistency is now checked in two grades: `progress/current.md`
must reference the newest recorded checkpoint (unknown or stale references
fail validation — both are forgotten bookkeeping with unambiguous fixes), and
the latest complete session's milestone/work-item statuses are compared
against plan frontmatter with a warning on drift, since either side may be
legitimately ahead and older sessions legitimately reflect their own time.

The validator gained its own gate: `tools/test-check-agent-records.py` builds
a synthetic golden `agent/` tree in a temporary directory and pins fifteen
mutation cases — required session fields, event sequencing, distillation with
pre-cutoff grandfathering, index completeness in both directions, ledger
counts, staleness and status-drift warnings, markdown links, checkpoint
id/path agreement, and current-progress freshness. It runs as CTest
`metaflux.architecture.agent-records-selftest` (architecture label) in every
test preset.

The three deferred small items (experience revalidation triggers, parallel
writer protocol, distillation cutoff redesign) remain deliberately untouched
until scale demands them.

## Changed paths

- `tools/check-agent-records.py`: current-progress freshness rule, status
  consistency warnings, plan-status helper.
- `tools/test-check-agent-records.py`: new fifteen-case self-test suite.
- `tests/CMakeLists.txt`: selftest wired as an architecture-labeled test.
- `tools/README.md`: selftest documentation.

## Verification

| Command/gate | Result |
| --- | --- |
| `python3 tools/test-check-agent-records.py` | 15/15 cases passed |
| `python3 tools/check-agent-records.py .` | ok (4 sessions, 44 events, 60 Markdown) |
| dev preset CTest | 16/16 passed, including both architecture tests |
| Scaffolded index row placement | table end, not file end |

## Decisions and experience

- No new DNNNN decisions (agent-workflow rules, recorded in tools/README.md
  and enforced by the validator).

## Distillation

- Promoted: two-grade consistency design -> tools/README.md (session
  verification above).
- Session-only: golden-tree pattern - generic practice already embodied by the
  suite, so it was not duplicated as experience.

## Unresolved items

- None blocking. Deferred small items are tracked in the re-assessment
  conversation record, not as open decisions (they are workflow preferences,
  not blocking decisions).

## Handoff

First command: `python3 tools/test-check-agent-records.py && python3
tools/check-agent-records.py .`. Minimum reading:
[open decisions](../../../../memory/open-decisions.md), then
[current progress](../../../../progress/current.md).
