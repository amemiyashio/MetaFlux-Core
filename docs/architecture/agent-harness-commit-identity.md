# Agent Harness Commit Identity

| Field | Value |
| --- | --- |
| Status | Verified |
| Decision | D0028 |
| Applies to | Git commits created by repository agents |

## Decision

An agent-created Git commit derives both Author and Committer from the active
agent's self-declared harness subject. Before its first commit, the agent reads
that subject from its own harness runtime context and emits `Agent harness
subject: <subject>` in the interaction. Repository code does not own a product
list mapping Codex, Claude Code, ZCode, or any later harness to hand-maintained
Git identities. Adding a harness therefore does not require a repository patch.

The agent supplies the same subject command-locally through the generic
`METAFLUX_AGENT_HARNESS` declaration. The repository helper validates that
declaration and derives the Git identity. It does not inspect `/proc`, process
names, product-specific environment namespaces, repository contents, or human
Git configuration to guess the harness. A missing or malformed declaration
stops before Git runs; human Git configuration is never a fallback.

## Identity Derivation

The self-declared subject must already be a normalized lowercase ASCII slug
containing only letters, digits, and single hyphen separators. The helper
validates that form, then generates the command-local identity without a vendor
table:

```text
name  = Agent Harness (<subject>)
email = <subject>@localhost
```

Harness session and thread values never enter the declaration, identity, or
logs. Only the normalized subject is passed to the helper.

## Boundary And Consequences

- The helper sets all four `GIT_AUTHOR_*` and `GIT_COMMITTER_*` variables only
  for its `git commit` child process.
- Local and global Git configuration remain human-owned and unchanged.
- The command interface has no per-product harness selector. The agent supplies
  its self-reported subject through command-local `METAFLUX_AGENT_HARNESS`.
- The self-report is a provenance declaration, not authenticated evidence. The
  agent must read the active harness context, surface the subject before the
  commit, and must not synthesize a preferred or inherited prior-agent value.
- Every agent or harness handoff requires a fresh self-declaration. The
  command-local value prevents one agent's identity from becoming sticky.
- Git Author and Committer identify workflow provenance; they are not
  authentication, signing, or proof of a particular human operator.
- This is the required repository workflow for agent-created commits, not a Git
  hook identity check. Direct human commits remain valid and continue to use
  the human-owned Git configuration.
- Commit `ded1dad` using `Codex <codex@localhost>` and commit `f862852` using
  `ZCode <zcode@localhost>` remain factual evidence of the superseded static
  implementation. D0028 changes future derivation, not those Git objects.

## Verification State

The isolated helper suite passes seven cases covering an unseen declared
harness, complete absence of process/environment inference, missing and
malformed declarations, stale Git identity override, cross-harness handoff,
configuration isolation, removed fixed selection, and protected commit
options. SC0004 binds the synchronized migration and a real commit created only
after the active agent self-declares its harness subject.
