# CLAUDE.md

@AGENTS.md

Claude Code edit-time guards live in `.claude/hooks/` and enforce the same
direction as the pre-commit hook: edits outside `agent/` require one resolvable
in-progress execution-focus owner, and an explicit `METAFLUX_SESSION_ID` must
match it. Existing checkpoints and terminal-session files require an exact
Historical row in a committed Active SC. New checkpoints remain writable. The
candidate-index pre-commit gate remains the exact commit-authority check. When a
guard blocks you, follow the instruction in its error message instead of
retrying.
