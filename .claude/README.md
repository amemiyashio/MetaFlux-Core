# Claude Code Bridge (repository-local)

This directory makes the repository's agent rules visible to Claude Code
without touching any global configuration. Scope guarantees:

- Everything here lives inside the repository. Nothing writes to `~/.claude`
  or modifies the Claude CLI installation.
- Contributors who do not use Claude Code are unaffected: `.claude/` is inert
  for other tools, and the repository's hard gates (AGENTS.md, the
  `.githooks/pre-commit` session-coverage gate, repository validators, and
  owner-specific tests) are tool-agnostic.
- Claude Code asks each user to approve project-level hooks before they run,
  so the bridge is opt-in even for Claude users.

Contents:

- `settings.json`: registers the hooks below. Project-scoped only.
- `hooks/session_start.py`: prints the onboarding banner into every new
  Claude session (read AGENTS.md, scaffold before changing, protected history
  requires D0025/SC authorization).
- `hooks/pre_edit.py`: PreToolUse guard for Edit/Write/MultiEdit/NotebookEdit.
  Allows new checkpoints, blocks unlisted edits to existing checkpoints and
  terminal sessions, and blocks edits outside `agent/` while no session is in
  progress. Protected-history authorization is read only from a committed
  Active SC. The bridge fails open on schema drift; the pre-commit hook,
  repository validators, and owner-specific tests remain authoritative.

The single source of rules is [`AGENTS.md`](../AGENTS.md); the root
`CLAUDE.md` is a one-line `@AGENTS.md` import plus a pointer here, enforced by
`tools/check-agent-records.py`.
