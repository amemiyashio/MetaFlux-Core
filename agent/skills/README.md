# Expert Skills

Skills are procedural playbooks for working inside this repository: how to
close a decision, add a component, record a session, run a qualification
matrix. They are load-on-demand expertise, not daily reading; an agent reads
the skill named for the task at hand and follows it verbatim.

Skills differ from their neighbors by intent:

| Neighbor | Owns | A skill instead owns |
| --- | --- | --- |
| [`experience/`](../experience/README.md) | Validated observations about the world (`Candidate` until reproduced) | Prescribed steps that are correct because the repository machinery enforces them |
| [`templates/`](../templates/README.md) | Record shapes (passive skeletons) | Sequenced procedures with verification commands |
| `memory/` | Stable context | Task-scoped procedure |

## Form

Each skill is one directory named by a durable lowercase-hyphenated slug
containing one `SKILL.md` with `name`, `description`, and `status` frontmatter
(`Draft`, `Active`, `Retired`; retired skills remain for history). The body
states when to use the skill, the steps in order, and the verification
command whose output proves the steps were followed.

## Index

`tools/check-agent-records.py` enforces that this README indexes every skill
directory, that every skill directory contains a `SKILL.md` with non-empty
`name` and `description` frontmatter, and that directory names are slugs.

| Skill | Status | Use when |
| --- | --- | --- |
| [add-component](add-component/SKILL.md) | Active | Adding any new boundary target to the build |
| [close-decision](close-decision/SKILL.md) | Active | Resolving a row of the open-decisions ledger |
| [record-session](record-session/SKILL.md) | Active | Recording any work session from start to checkpoint |
