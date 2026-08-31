# Claude Code Bridge (repository-local)

This directory makes the repository's agent rules visible to Claude Code
without touching any global configuration. Scope guarantees:

- Everything here lives inside the repository. Nothing writes to `~/.claude`
  or modifies the Claude CLI installation.
- Contributors who do not use Claude Code are unaffected: `.claude/` is inert
  for other tools, and the repository's hard gates (AGENTS.md, the
  `.githooks/pre-commit` execution-focus gate, repository validators, and
  owner-specific tests) are tool-agnostic.
- Claude Code asks each user to approve project-level hooks before they run,
  so the bridge is opt-in even for Claude users.

Contents:

- `settings.json`: registers the hooks below. Project-scoped only.
- `hooks/session_start.py`: prints the onboarding banner into every new
  Claude session (read AGENTS.md, require schema version 2 and D0029 on focus
  plus owner, reject legacy-session resumption, distinguish scaffolding from
  focus ownership, and use D0025/SC authorization for protected history).
- `hooks/pre_edit.py`: PreToolUse guard for Edit/Write/MultiEdit/NotebookEdit.
  Allows new checkpoints, blocks unlisted edits to existing checkpoints and
  terminal sessions, and blocks edits outside `agent/` unless
  `agent/progress/focus.json` names one resolvable schema version 2, D0029,
  in-progress owner. Schema version 1, missing-epoch, and mismatched-epoch focus
  or owner records fail closed. When `METAFLUX_SESSION_ID` is present, the guard
  also rejects an owner mismatch.
  Protected-history authorization is read only from a committed Active SC.
  The pre-commit hook still validates the exact candidate tree and owns commit
  authorization.

The single source of rules is [`AGENTS.md`](../AGENTS.md); the root
`CLAUDE.md` is a one-line `@AGENTS.md` import plus a pointer here, enforced by
`tools/check-agent-records.py`.
