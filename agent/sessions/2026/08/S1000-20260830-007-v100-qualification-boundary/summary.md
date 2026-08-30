# Session Summary

## Objective and outcome

Moved Intel x86_64 support and physical NVIDIA binding-performance
qualification from the former `v0.2.0` expansion boundary to M1000 /
`v1.0.0`. Native NixOS VM/package qualification remains in the unallocated
`v0.2.0` line. D0027 is the canonical decision and SC0003 is Applied against
content revision `d9c67e47eff7ae17a4b6404412c946c81544afe2`; neither absent
hardware gate is represented as passed.

## Durable changes

- `agent/plan/M1000-stable-qualification/`: queued M1000 plan with W1001 Intel,
  W1002 physical NVIDIA binding performance, and W1003 stable release ownership.
- `agent/memory/decisions-index.md`: D0027 authority and limited D0023/D0024
  supersession.
- `agent/semantic-changes/SC0003-v100-qualification-boundary.md`: exact migration
  inventory, evidence locks, handoffs, and effective revision.
- Current plans, memory, progress, performance guidance, and 16 historical
  surfaces: split destination with capture-time facts preserved.

## Verification

| Command/gate | Result |
| --- | --- |
| `python3 tools/check-agent-records.py .` | Passed at record closure: 28 sessions, 215 events, 213 Markdown files |
| `python3 tools/test-check-agent-records.py` | Passed 163/163 |
| `python3 tools/check-semantic-change-edits.py --cached .` | Passed |
| `python3 tools/test-semantic-change-edits.py` | Passed 21/21 |
| `python3 agent/skills/session-guidance/scripts/test_guidance.py` | Passed 20/20 |
| Architecture CTest through the Nix development shell | Passed 6/6 |
| Protected-history and retained-evidence audit | Passed; retained event hash `146f370624e436221ad3596d7b356b4b23409a66` |
| Exact residual and `git diff --check` scans | Passed |

## Cleanup

- Removed: transient G003 and G005 guidance packets after each target owner
  recorded exactly one adopted disposition.
- Retained: none; no session-owned download, snapshot, build, or guidance
  artifact remains.

## Decisions and experience

- D0027 owns the v1.0 qualification boundary; SC0003 records its complete
  application without changing historical evidence maturity.

## roast

### light roasts

- none.

### medium roasts

- Exact current/tooling/history migration and handoff lifecycle ->
  `agent/semantic-changes/SC0003-v100-qualification-boundary.md` (effective
  revision `d9c67e47eff7ae17a4b6404412c946c81544afe2`)

### dark roasts

- Stable qualification boundary ->
  `agent/plan/M1000-stable-qualification/plan.md`
  (Intel x86_64 and physical NVIDIA binding qualification are M1000 /
  `v1.0.0`; native NixOS remains `v0.2.0`; authority: D0027, SC0003)

## session-only

- none.

## Unresolved items

- W1001, W1002, and W1003 remain queued under M1000; this migration schedules
  their ownership and does not manufacture Intel or physical NVIDIA evidence.
- Native NixOS VM/package qualification remains an unallocated `v0.2.0` item.

## Handoff

Read `agent/progress/current.md` and
`agent/plan/M1000-stable-qualification/plan.md`. SC0003 is evidence rather than
an active history-edit permit; future breaking replacements require a new
decision-bound Active SC.
