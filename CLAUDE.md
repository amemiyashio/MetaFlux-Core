# CLAUDE.md

@AGENTS.md

Claude Code edit-time guards live in `.claude/hooks/` and enforce the same
direction as the pre-commit hook: edits outside `agent/` require one resolvable
schema version 2, D0029, in-progress execution-focus owner, and an explicit
`METAFLUX_SESSION_ID` must match it. Schema version 1 and pre-epoch sessions are
never resume or fallback sources. Existing checkpoints and terminal-session
files require an exact Historical row in a committed Active SC. New checkpoints
remain writable. The candidate-index pre-commit gate remains the exact
commit-authority check. When a guard blocks you, follow the instruction in its
error message instead of retrying.
