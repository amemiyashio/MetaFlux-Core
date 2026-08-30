# Summary

Clarify the existing D0020 timezone-relative mirror priority directly in the
`manage-toolchain` routing rules.

## Durable changes

- `agent/skills/manage-toolchain/SKILL.md`: makes current-configured-timezone
  mirror preference explicit in Routing, before adjacent-timezone mirrors and
  canonical upstream.

## Verification

| Command/gate | Result |
| --- | --- |
| `python3 -B tools/check-agent-records.py .` | Passed |
| Skill routing corpus | Passed; 68 cases |
| Skill routing self-test | Passed; 23/23 cases |
| `git diff --check` | Passed |

## Cleanup

- Removed: none; no download, build, or temporary artifact was created.
- Retained: none outside Git content.

## Decisions and experience

- D0020 remains unchanged: current configured timezone, adjacent timezone, then
  canonical upstream, with immutable upstream identity authoritative.
- No new experience record was needed.

## roast

### light roasts

- Existing D0020 timezone mirror priority -> `agent/skills/manage-toolchain/SKILL.md` (current configured timezone, adjacent timezone, then canonical upstream; 68 routing cases and 23/23 self-tests)

### medium roasts

- none.

### dark roasts

- none.

## session-only

- none.

## Unresolved items

- None for this guidance clarification; W0101 remains active for its product
  qualification gates.

## Handoff

For a tool download, read `manage-toolchain`, select the mirror from the current
execution environment's configured timezone first, verify canonical identity,
and use the existing adjacent-timezone/upstream fallback order.
