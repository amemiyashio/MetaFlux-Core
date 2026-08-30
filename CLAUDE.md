# CLAUDE.md

@AGENTS.md

Claude Code edit-time guards live in `.claude/hooks/` and enforce the same
rules as the pre-commit hook: an in-progress session is required before any
edit outside `agent/`; existing checkpoints and terminal-session files require
an exact Historical row in a committed Active SC. New checkpoints remain
writable. When a guard blocks you, follow the instruction in its error message
instead of retrying.
