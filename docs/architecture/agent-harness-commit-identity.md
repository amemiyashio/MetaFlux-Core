# Agent Harness Commit Identity

| Field | Value |
| --- | --- |
| Status | Verified |
| Decision | D0028 |
| Applies to | Git commits created by repository agents |

## Decision

An agent-created Git commit derives both Author and Committer from the active
agent harness observed at runtime. Repository code does not own a product list
mapping Codex, Claude Code, ZCode, or any later harness to hand-maintained Git
identities. Adding a harness therefore does not require a repository patch.

The repository helper reads the harness subject automatically. A validated
generic `METAFLUX_AGENT_HARNESS` declaration is the direct harness protocol and
does not require `/proc`. Otherwise, the helper collects non-empty environment
namespaces ending in `_SESSION_ID`, `_THREAD_ID`, or `_PROJECT_DIR` and matches
them against the Linux ancestor-process command lines. The nearest unambiguous
match is the active subject. Missing, malformed, or ambiguous evidence stops
the commit; human Git configuration is never a fallback.

## Identity Derivation

The detected subject is normalized to a lowercase ASCII slug containing only
letters, digits, and single hyphen separators. The command-local identity is
then generated without a vendor table:

```text
name  = Agent Harness (<subject>)
email = <subject>@localhost
```

Session and thread values are corroborating presence signals only. They never
enter the identity or logs. On the automatic-detection path, the helper reads
process metadata but does not execute, probe, or modify the harness process.

## Boundary And Consequences

- The helper sets all four `GIT_AUTHOR_*` and `GIT_COMMITTER_*` variables only
  for its `git commit` child process.
- Local and global Git configuration remain human-owned and unchanged.
- The command interface has no per-product harness selector. A harness that
  needs an explicit protocol supplies `METAFLUX_AGENT_HARNESS` in its runtime
  environment.
- `METAFLUX_AGENT_HARNESS` is a provenance declaration supplied by the harness
  runtime. It is not authenticated evidence, and an agent must not synthesize
  or override it to choose a preferred identity.
- Git Author and Committer identify workflow provenance; they are not
  authentication, signing, or proof of a particular human operator.
- This is the required repository workflow for agent-created commits, not a Git
  hook identity check. Direct human commits remain valid and continue to use
  the human-owned Git configuration.
- Commit `ded1dad` using `Codex <codex@localhost>` and commit `f862852` using
  `ZCode <zcode@localhost>` remain factual evidence of the superseded static
  implementation. D0028 changes future derivation, not those Git objects.

## Verification State

The isolated helper suite passes seven cases covering an unseen harness,
nearest-ancestor selection, direct-protocol isolation, missing or ambiguous
automatic evidence, malformed subjects, stale Git identity override,
cross-harness handoff, configuration isolation, removed fixed selection, and
protected commit options. The repository runtime resolves the current subject
without a selector as `codex`; SC0004 binds the synchronized migration and real
automatic content commit.
