---
id: P20260830-003
status: Recorded
captured: 2026-08-30
milestone: M0100
branch: main
git_revision: 209caee4fd1eb7af98a9d9b5abefd18cf28fce97
workspace: content committed; record migration pending this checkpoint commit; G002 remains untracked in its target active inbox
---

# Semantic delivery identity migration

## Engineering state

Revision `78fc9d8` establishes product version `0.1.0` from `VERSION` and derives
M/W/S identities from explicit four-part delivery coordinates. Final content
revision `8b79bb0` makes latest-session selection chronological across delivery
scopes. Final content revision `209caee` rejects legacy and truncated session
references in durable Agent records. This separate record checkpoint migrates
all 25 historical sessions without retaining pre-policy aliases. M0100-M0130
now represent releases `v0.1.0`-`v0.1.3`; D0012/D0023 assign Intel host,
physical NVIDIA binding performance, and native NixOS qualification to
`v0.2.0`.

## Verification evidence

| Gate | Result | Artifact/log |
| --- | --- | --- |
| Dev configure/build | Passed | External `.metaflux-build` tree; not retained as evidence |
| Dev CTest | 63/63 passed | Console result summarized in S0100-20260830-004-semantic-version-line |
| Agent validator self-test | 111/111 passed | `tools/test-check-agent-records.py` |
| Guidance self-test | 17/17 passed | `session-guidance/scripts/test_guidance.py` |
| Skill routing | 68-case corpus and 23/23 self-test passed | `agent/skills/trigger-evals.json` |
| Skill packages | 18/18 passed quick validation | `agent/skills/` |
| Historical inventory | 25 directories and 25 index rows; all coordinates agree | `agent/sessions/README.md` |
| Reverse scan | No pre-D0024 M/W/S identifier remains | Repository scan |

## Decisions and durable outcomes

- [D0024](../../../memory/decisions-index.md) is the one breaking identity
  migration; product SemVer stays three-part and delivery trace metadata stays
  four-part.
- Historical checkpoint/session evidence claims and cited revisions remain
  unchanged even though their M/W/S references were rewritten.
- Compact-body collisions are rejected because a multi-digit concatenation is
  not reversible; the dotted coordinate is authoritative.

## Open work and risks

- G002 is ready in the active M0100 completion session and must be processed by
  that owner before its next checkpoint or close boundary.
- No M0200 plan exists yet; `v0.2.0` is a scope boundary, not an allocated plan.

## Resume notes

1. Read `agent/progress/current.md` and D0024.
2. Use `python3 tools/new-session.py <MAJOR.MINOR.PATCH.WORK> <slug>`.
3. Run `python3 tools/check-agent-records.py .` before committing records.
