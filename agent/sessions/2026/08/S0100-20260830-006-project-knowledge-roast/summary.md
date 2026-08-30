# Session Summary

## Objective and outcome

Replaced the pre-D0026 project-knowledge promotion model with the explicit-only
`$roast` skill, three semantic-transformation depths, and an independent
`session-only` disposition. D0026 is Verified and SC0002 is Applied against
content revision `991e5327c8a3b1f5d05112f895011f8d83f0bff0`.

## Durable changes

- `docs/architecture/project-knowledge-roast.md`: D0026 canonical replacement design.
- `agent/memory/decisions-index.md`: resolvable D0026 authority.
- `agent/skills/session-guidance/`: durable SC handoff ID reservation with 20/20 tests.
- `agent/skills/roast/`: explicit-only knowledge-promotion workflow and routing matrix.
- `tools/check-agent-records.py`: strict roast/session-only, single-owner, and invocation-policy gates.
- `agent/semantic-changes/SC0002-project-knowledge-roast.md`: applied 68-surface migration record.

## Verification

| Command/gate | Result |
| --- | --- |
| `python3 tools/check-agent-records.py .` | Passed: 27 sessions, 210 events, 205 Markdown files |
| `python3 tools/test-check-agent-records.py` | Passed: 163/163 |
| `python3 tools/test-semantic-change-edits.py` | Passed: 21/21 |
| `python3 agent/skills/session-guidance/scripts/test_guidance.py` | Passed: 20/20 |
| `python3 -B tools/check-skill-routing.py .` | Passed: 11 domain skills, 2 workflow skills, 82 cases |
| `python3 -B tools/test-check-skill-routing.py` | Passed: 34/34 |
| `$roast` independent A-E forward review | Passed: exact fixture inputs and observations are in `notes.md` |
| `nix develop . --command ctest --test-dir ../.metaflux-build/MetaFlux-Core/dev -L architecture --output-on-failure` | Passed: 6/6 |
| Locked-evidence audit and `git diff --check` | Passed |

## Cleanup

- Removed: target-session G002 and G004 guidance packets after owner adoption.
- Retained: none.

## Decisions and experience

- D0026 defines roast depth, the independent session-only disposition, and explicit invocation.
- SC0002 applies the complete migration; SC0001 remains Applied for D0025 governance while its old promotion-model consequence is replaced.

## roast

### light roasts

- Strict roast/session-only schema enforcement -> `tools/check-agent-records.py`
  (Agent self-test 163/163)
- Explicit-only roast routing -> `agent/skills/trigger-evals.json` (routing 82
  cases and 34/34 self-tests)

### medium roasts

- SC handoff references now reserve cleaned guidance IDs ->
  `agent/skills/session-guidance/references/protocol.md` (a2df763; 20/20
  guidance tests)
- Complete D0026 migration across current, tooling, active-owner, and
  protected-history surfaces ->
  `agent/semantic-changes/SC0002-project-knowledge-roast.md` (29a4e38 and
  991e532; repository and architecture gates passed)

### dark roasts

- D0026 project-knowledge roast contract ->
  `docs/architecture/project-knowledge-roast.md` (Verified in 29a4e38 and
  effective in 991e532; authority: D0026, SC0002)

## session-only

- none.

## Unresolved items

- none.

## Handoff

Read `agent/progress/current.md`. SC0002 is evidence rather than an active
permit; any later breaking replacement needs a new decision-bound SC already in
`HEAD` before protected history is edited.
