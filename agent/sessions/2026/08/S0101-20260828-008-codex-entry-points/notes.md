# Notes

## Codex behavior

The [official OpenAI documentation](https://learn.chatgpt.com/docs/agent-configuration/agents-md)
states that Codex builds its instruction chain once per run, reads project
guidance from the project root down to the working directory, and selects
`AGENTS.override.md`, then `AGENTS.md`, then configured fallbacks at each
level. MetaFlux-Core's root `AGENTS.md` is therefore a native Codex entry point.

`.claude/` is not a Codex hook mechanism. It remains a repository-local,
optional Claude Code bridge; Codex receives the same invariants through
`AGENTS.md`, while `.githooks/pre-commit` and Nix checks enforce them without
depending on a specific agent frontend.

## Resumed failure and correction

The interrupted session had already expanded `source.agentRecords` and added
the check, but stopped after changing an invalid nested Git setup. On resume,
the isolated check passed the record validator and then failed at
`test -x repo/.claude/hooks/pre_edit.py`.

That assertion contradicted `.claude/settings.json`, which invokes both Python
hooks as `python3 <path>`; their Git mode is intentionally `100644`. The final
fixture initializes the copied `repo/` as the real Git root, checks executable
permission only for `.githooks/pre-commit`, and checks both Python hooks as
ordinary files.
