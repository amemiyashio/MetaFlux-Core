# Summary

Hardened the agent guidance so an agent cannot change this repository without
onboarding through its rules. Three layers, each aimed at a different failure
mode of a rule-skipping agent.

**Entry point**: `AGENTS.md` at the repository root — the file agentic tools
read automatically — states the five hard rules (read order,
session-before-change, no silent constraint relaxation, verify before commit,
record outcomes) and points into `agent/`. The root README and a new "Before
changing anything" block atop `agent/README.md` both route agents there.

**On-ramp skill**: `start-work` encodes the cold-start procedure — read order,
open-decisions check for the task area, skill matching, scaffold-before-change,
verification, finish through `record-session`. It is the first entry of the
skills index.

**Machine gate**: `.githooks/pre-commit`, installed automatically by the Nix
devshell (`core.hooksPath`, python3 added to the shells), validates records,
runs the validator self-test silently, and rejects staged changes outside
`agent/` while no session is in progress — with the exact scaffold command in
the error message.

All three layers were verified in the failure direction before committing:
staged non-agent changes with no in-progress session were rejected with the
scaffold instruction; scaffolding made the hook pass; the implementation
commit then ran the hook for real.

## Changed paths

- `AGENTS.md`: new root entry point (five hard rules).
- `agent/README.md`: "Before changing anything" block.
- `README.md`: agent routing line.
- `agent/skills/start-work/SKILL.md`, `agent/skills/README.md`: on-ramp skill.
- `.githooks/pre-commit`: session-coverage and records gate.
- `nix/shells/default.nix`: python3 + automatic `core.hooksPath` wiring.

## Verification

| Gate | Result |
| --- | --- |
| Hook with staged non-agent changes, no session | Rejected, exit 1, scaffold instruction shown |
| Hook after scaffolding | Passed |
| `nix develop path:. -c true` | Devshell rebuilt with python3 and hook wiring |
| Implementation commit | Hook ran live and passed |
| `tools/check-agent-records.py .` | ok (7 sessions, 57 events, 74 Markdown) |

## Decisions and experience

- No new DNNNN decisions: workflow enforcement recorded in AGENTS.md and the
  hook itself.

## Distillation

- Promoted: three-layer guidance model -> AGENTS.md and the start-work skill
  (session verification above).
- Session-only: none; no separate experience claim was created.

## Unresolved items

- None blocking. Hooks are bypassable with `--no-verify`; the nix checks
  remain the backstop for anything that slips through.

## Handoff

First command: `python3 tools/check-agent-records.py .`. New agents read
[AGENTS.md](../../../../../AGENTS.md) first; the on-ramp procedure is the
[start-work skill](../../../../skills/start-work/SKILL.md).
