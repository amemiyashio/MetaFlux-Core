# Claude Code Bridge (repository-local)

This directory makes the repository's agent rules visible to Claude Code
without touching any global configuration. Scope guarantees:

- Everything here lives inside the repository. Nothing writes to `~/.claude`
  or modifies the Claude CLI installation.
- Contributors who do not use Claude Code are unaffected: `.claude/` is inert
  for other tools, and the repository's hard gates (AGENTS.md, the
  `.githooks/pre-commit` session-coverage gate, and the Nix checks) are
  tool-agnostic.
- Claude Code asks each user to approve project-level hooks before they run,
  so the bridge is opt-in even for Claude users.

Contents:

- `settings.json`: registers the hooks below. Project-scoped only.
- `hooks/session_start.py`: prints the onboarding banner into every new
  Claude session (read AGENTS.md, scaffold before changing, checkpoints are
  immutable).
- `hooks/pre_edit.py`: PreToolUse guard for Edit/Write/MultiEdit/NotebookEdit.
  Blocks rewrites of `agent/progress/checkpoints/` and any edit outside
  `agent/` while no session is in progress, with the remediation command in
  the denial. Fails open on schema drift — the pre-commit hook and Nix checks
  remain the authoritative gates.

The single source of rules is [`AGENTS.md`](../AGENTS.md); the root
`CLAUDE.md` is a one-line `@AGENTS.md` import plus a pointer here, enforced by
`tools/check-agent-records.py`.
