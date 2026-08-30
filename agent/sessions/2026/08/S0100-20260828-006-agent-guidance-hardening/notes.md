# Notes

## Why three layers instead of one

Each layer catches a different kind of rule-skipping agent:

- AGENTS.md catches the conforming-but-uninformed agent: tools in the Claude
  Code / ZCode / Codex family automatically read a root AGENTS.md, so the
  rules ride the mechanism the agent already trusts.
- The start-work skill catches the agent that reads the rules but improvises
  the order: it encodes cold start as a sequence with a verification command.
- The pre-commit hook catches the agent that reads nothing: its commit simply
  fails with an error that teaches the correct next command. The error is the
  documentation.

## Session-coverage rule design

The rule is "staged changes outside agent/ require an in-progress session",
not "a session dated today": the two-commit pattern (content first, records
after) means the content commit happens while the session is legitimately
in_progress, and the records commit stages only agent/ paths, so the rule
never fights the workflow it enforces. `--no-verify` bypasses everything, as
any git hook can; the nix checks in CI remain the backstop.

## Hook output discipline

The first version redirected the self-test's stdout but not stderr, so every
commit scrolled the full fixture diagnostics. Hooks should be quiet when
green and verbose only when rejecting — both streams silenced for the
self-test, and the coverage error carries the exact remediation command.

## What was deliberately not done

- No new validator rules: the hook orchestrates existing validators rather
  than growing new ones; staged-change analysis does not belong in the
  records validator.
- No pre-push full-matrix run: the ctest suite is currently sub-second, but
  wiring it into pre-push adds environment fragility for near-zero catch
  value over pre-commit; revisit when the suite grows.
- No CI workflow: the repository has no remote yet.
