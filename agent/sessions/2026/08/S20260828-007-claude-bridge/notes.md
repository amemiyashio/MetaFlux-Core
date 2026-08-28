# Notes

## Why an @-import instead of a copied rulebook

Claude Code expands `@path` imports in CLAUDE.md natively. A copied rulebook
would drift the first time either file changed, and two entry files that
disagree is worse than one entry file that is ignored — an agent that follows
stale rules with confidence is harder to catch than one that reads nothing
and trips the gates. The validator's inert-when-absent design keeps the rule
honest: repos without the bridge pay nothing.

## Why the guard fails open

The guard reads Claude Code's hook JSON schema. That schema belongs to an
external tool that can change; a guard that crashes closed would brick edits
after an upstream format change, which is exactly the kind of self-inflicted
outage that erodes trust in governance. Failing open is safe here because the
guard is the third line — the pre-commit gate and the Nix checks see the same
repository state independently of any CLI's schema.

## Edit-time versus commit-time

The pre-commit gate is authoritative but late: a rule-skipping agent could
make dozens of edits before the first commit rejects them. The PreToolUse
guard moves the same two invariants (checkpoint immutability, session
coverage) to the moment of each edit, so Claude gets corrected within one
tool call instead of after a session's worth of work. Keeping the invariant
logic duplicated-but-tiny in two places (hook script, pre-commit script) was
chosen over a shared module: each is ~40 lines, the shared version would need
a stable import path across three invocation contexts, and divergence between
them is caught by the failure-path tests each time either changes.

## Non-Claude contributors

Nothing in the build, tests, or nix checks reads `.claude/`. The directory is
versioned so Claude users share the bridge, and that is its entire blast
radius.
