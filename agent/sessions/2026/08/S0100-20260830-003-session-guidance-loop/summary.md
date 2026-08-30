# Session Summary

## Objective and outcome

Implemented and verified a session-local expert-guidance loop. Specialists can
publish bounded direction or a candidate patch without taking source ownership;
the receiving session owner validates, dispositions, and removes the transient
input at a defined control boundary.

## Durable changes

- `agent/skills/session-guidance/` owns packet creation, atomic state changes,
  exact interrupted-create recovery, disposition checks, and cleanup.
- Repository rules and the start/record skills now scan ready guidance only at
  resume, collaborator completion, next-work-unit, checkpoint, and close
  boundaries.
- The record validator accepts compact material dispositions, isolates active
  transient inboxes from durable scans, and requires terminal inboxes to be
  empty.
- The pre-commit hook rejects staged guidance on either side of additions,
  modifications, copies, renames, and type changes while permitting deletion.
- CTest runs the guidance CLI regression suite as an architecture gate.

## Verification

| Command/gate | Result |
| --- | --- |
| Guidance CLI self-test | 16/16 passed |
| Agent-record validator self-test | 81/81 passed |
| Current repository record gate | 24 sessions, 187 events, 186 Markdown files; passed |
| Skill quick validation and routing | Passed; routing corpus 68 cases and self-test 23/23 |
| Full dev CTest | 63/63 passed |
| `nix flake check path:. -L` | Passed |
| Independent forward use and targeted review | Passed; no residual finding |

## Cleanup

- Removed: isolated forward-test repository, temporary validator output, and all
  test-owned packet, patch, and recovery residue.
- Retained: no session-owned disposable artifact. The pre-existing shared
  repository-external dev build tree was left untouched.

## Decisions and experience

- The reusable protocol belongs in a skill; only its transient input belongs in
  an active session. No new advice archive or source snapshot was introduced.
- No architecture decision or experience record was required because the
  workflow preserves existing technical ownership and durable record types.

## roast

### light roasts

- Guidance control-boundary scanning -> `AGENTS.md` (resume, collaborator
  completion, next-work-unit, checkpoint, and close boundaries)

### medium roasts

- Session-guidance lifecycle and ownership ->
  `agent/skills/session-guidance/SKILL.md` (16/16 CLI tests, 81/81 validator
  tests, full dev CTest 63/63, and independent forward review)

### dark roasts

- none.

## session-only

- none.

## Unresolved items

- None.

## Handoff

At a control boundary, run
`python3 agent/skills/session-guidance/scripts/guidance.py list --session SESSION`.
Load the skill only when a ready packet exists or guidance processing was
explicitly requested.
