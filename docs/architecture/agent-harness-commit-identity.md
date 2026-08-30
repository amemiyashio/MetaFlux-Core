# Agent Harness Commit Identity

| Field | Value |
| --- | --- |
| Status | Proposed |
| Decision | D0028 |
| Applies to | Git commits created by repository agents |

## Decision

An agent-created Git commit derives both Author and Committer from the active
agent harness observed at runtime. Repository code does not own a product list
mapping Codex, Claude Code, ZCode, or any later harness to hand-maintained Git
identities. Adding a harness therefore does not require a repository patch.

The repository helper reads the harness subject automatically. A validated
generic `METAFLUX_AGENT_HARNESS` declaration is the direct harness protocol.
Otherwise, the helper collects non-empty environment namespaces ending in
`_SESSION_ID`, `_THREAD_ID`, or `_PROJECT_DIR` and matches them against the
Linux ancestor-process command lines. The nearest unambiguous match is the
active subject. Missing, malformed, or ambiguous evidence stops the commit;
human Git configuration is never a fallback.

## Identity Derivation

The detected subject is normalized to a lowercase ASCII slug containing only
letters, digits, and single hyphen separators. The command-local identity is
then generated without a vendor table:

```text
name  = Agent Harness (<subject>)
email = <subject>@localhost
```

Session and thread values are corroborating presence signals only. They never
enter the identity or logs. The helper reads process metadata but does not
execute, probe, or modify the harness process.

## Boundary And Consequences

- The helper sets all four `GIT_AUTHOR_*` and `GIT_COMMITTER_*` variables only
  for its `git commit` child process.
- Local and global Git configuration remain human-owned and unchanged.
- The command interface has no per-product harness selector. A harness that
  needs an explicit protocol supplies `METAFLUX_AGENT_HARNESS` in its runtime
  environment.
- Git Author and Committer identify workflow provenance; they are not
  authentication, signing, or proof of a particular human operator.
- Commit `ded1dad` using `Codex <codex@localhost>` and commit `f862852` using
  `ZCode <zcode@localhost>` remain factual evidence of the superseded static
  implementation. D0028 changes future derivation, not those Git objects.

## Verification State

D0028 is approved for migration through SC0004. It becomes Verified only after
generic unseen-harness, ancestry selection, ambiguity rejection, Git identity,
configuration-isolation, protected-option, residual-search, and real automatic
commit checks pass.
