# Expert Skills

Skills are repository-scoped Codex skill packages: focused instructions,
references, assets, and optional scripts for repeatable work. They are
load-on-demand expertise, not daily reading; Codex first sees each package's
name and description, then reads its full instructions only when the task
matches.

Skills differ from their neighbors by intent:

| Neighbor | Owns | A skill instead owns |
| --- | --- | --- |
| [`experience/`](../experience/README.md) | Validated observations about the world (`Candidate` until reproduced) | Prescribed steps that are correct because the repository machinery enforces them |
| [`templates/`](../templates/README.md) | Record shapes (passive skeletons) | Task-specific instructions and supporting resources |
| `memory/` | Stable context | Task-scoped procedure |

## Codex package form

The [OpenAI Codex skill format](https://developers.openai.com/codex/skills/)
is authoritative. Each skill is one directory named by a durable
lowercase-hyphenated slug with this shape:

```text
skill-name/
  SKILL.md             # required instructions and metadata
  agents/openai.yaml   # optional Codex UI, policy, and dependencies
  scripts/             # optional deterministic helpers
  references/          # optional documentation loaded on demand
  assets/              # optional templates and resources
```

`SKILL.md` uses YAML frontmatter. `name` and `description` are required;
`license`, `allowed-tools`, and `metadata` are accepted standard extensions.
The `name` must equal the directory slug. Lifecycle status is repository
catalog metadata, kept in the index below rather than added to standard skill
frontmatter. The old minimal package containing only `SKILL.md` remains valid.

Physical packages remain under `agent/skills/` so existing durable links do
not move. The repository root's `.agents/skills` symlink is the Codex-native
discovery entry point and must resolve to this directory. Package instructions
should name concrete inputs and outputs; procedures that mutate the repository
should also name proportionate verification evidence.

## Index

`tools/check-agent-records.py` enforces that this README indexes every skill
directory, every package satisfies the Codex `SKILL.md` contract, names match
their slugs, and `.agents/skills` resolves to this catalog. `Draft`, `Active`,
and `Retired` remain the repository lifecycle states; retired packages remain
for history.

| Skill | Status | Use when |
| --- | --- | --- |
| [start-work](start-work/SKILL.md) | Active | Beginning any task, before the first change |
| [add-component](add-component/SKILL.md) | Active | Adding any new boundary target to the build |
| [close-decision](close-decision/SKILL.md) | Active | Resolving a row of the open-decisions ledger |
| [record-session](record-session/SKILL.md) | Active | Recording any work session from start to checkpoint |
| [implementation-readiness](implementation-readiness/SKILL.md) | Active | Assessing whether architecture or a workstream is ready for implementation |
