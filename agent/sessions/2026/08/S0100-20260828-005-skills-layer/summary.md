# Summary

Added the expert-skills layer under `agent/skills/` and enforced its form like
every other record layer. No product code changed.

Skills are procedural playbooks for working in this repository — load-on-demand
expertise, deliberately distinct from `experience/` (validated observations
about the world) and `templates/` (passive record shapes). Form: one directory
per durable lowercase slug containing a `SKILL.md` with
`name`/`description`/`status` frontmatter (`Draft`, `Active`, `Retired`).

Seeded with three skills encoding the procedures proven across this session
series: `add-component`, `close-decision`, and `record-session`. The validator
now enforces the skill form — bidirectional index completeness, slug
directory names, and `SKILL.md` frontmatter — bringing the self-test suite to
twenty cases.

The link checker surfaced two authoring errors on first run: over-deep links
from `SKILL.md` (skills sits one level shallower than sessions) and a link to
`agent/templates/README.md`, which had never existed; the missing template
index was written as part of this change.

## Changed paths

- `agent/skills/README.md`, `agent/skills/{add-component,close-decision,
  record-session}/SKILL.md`: new skills layer with three seeds.
- `agent/templates/README.md`: new template index (previously missing).
- `agent/README.md`: skills row in directory roles, skill slug in stable
  identifiers, `Draft/Active/Retired` in status rules.
- `tools/check-agent-records.py`: `validate_skills` rule.
- `tools/test-check-agent-records.py`: five new cases (twenty total).
- `tools/README.md`: validator scope description refreshed.

## Verification

| Command/gate | Result |
| --- | --- |
| `python3 tools/test-check-agent-records.py` | 20/20 cases passed |
| `python3 tools/check-agent-records.py .` | ok (5 sessions, 50 events, 68 Markdown) |
| dev preset CTest | 16/16 passed |

## Decisions and experience

- No new DNNNN decisions: the skills layer is agent-workflow governance
  recorded in `agent/README.md` and `agent/skills/README.md`.

## Distillation

- Distilled: the skills-vs-experience-vs-templates boundary into
  `agent/skills/README.md`; the three seeded procedures are themselves the
  distillation of S0100-20260828-001-spec-consistency,
  S0100-20260828-002-layout-convergence,
  S0100-20260828-003-agent-record-convergence, and
  S0100-20260828-004-record-gate-hardening.

## Unresolved items

- None blocking.

## Handoff

First command: `python3 tools/check-agent-records.py .`. Minimum reading:
[skills](../../../../skills/README.md) when starting a task that matches one,
then [open decisions](../../../../memory/open-decisions.md) and
[current progress](../../../../progress/current.md).
