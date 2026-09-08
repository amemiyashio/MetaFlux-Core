---
status: Verified
decision: decision-0034
updated: 2026-09-08
---

# Agent Tool Detection

## Decision

MetaFlux derives agent commit identity from the harness name already emitted in
the current conversation. `detect-agent-tool` reports only the normalized
subject, CLI interface, and `declared` source. The result is ephemeral and is
never stored in repository goal state.

This decision replaces the fixed agent product identity retained by
decision-0033 and the later executable-probe form of the same decision. The
execution topology is refined by decision-0052; the current Epoch is epoch-0015.

## Rationale

A fixed product name correctly rejected model-derived labels but confused a
repository policy default with the tool actually executing a commit. Executable
PATH scans, process walks, and `--version` probes then added a second identity
machine: they raced AppImage cold starts, treated multiple visible CLIs as
ambiguity, and still did not answer "which harness is speaking in this
conversation." The conversation already names the harness (`zcode`, `codex`,
`claude`). That name is the identity.

## Detection Order

1. Use the `--agent-tool` argument when the current command already has the
   conversation-emitted harness name.
2. Otherwise use the `METAFLUX_AGENT_TOOL` launcher declaration.

No other source is valid. PATH order never selects a candidate. Process
ancestry, `/proc`, executable existence, `--version`, `--help`, and file
digests are not identity inputs.

## Information Boundary

The subject comes only from the declared harness name after tool-shaped
normalization. Model, provider, template, backend, build, prompt, session,
thread, repository prose, Git configuration, and user labels that are not that
harness name are neither identity sources nor output fields. A model-shaped
declared name is rejected instead of being normalized into a tool identity.

The project does not pin or install the caller harness through Nix. Nix owns
the Python detector runtime and project tools; the harness name is observed
conversation evidence outside product toolchain ownership.

## Commit Boundary

The commit helper accepts the declared name, derives
`SUBJECT <SUBJECT@localhost>`, and passes that same name to the child commit.
The pre-commit checker repeats the detector against the candidate tree and
requires Author, Committer, and `METAFLUX_AGENT_TOOL` to match that subject. It
does not execute the harness. The single active Epoch remains in
`agent/goal.json`; it is not duplicated into Git identity, an environment
declaration, or Git config. Neither helper nor checker changes Git config or
persists detector output.

## Verification

- deterministic declared-name, missing-declaration, and contamination tests for
  the detector;
- dynamic helper Author/Committer tests using a declared fixture name;
- candidate-tree commit-gate tests that re-check the declared name without
  probing an executable;
- residual rejection of the superseded fixed-harness declaration, executable
  declaration, and helper names; and
- skill metadata, bilingual routing, state, architecture, and full CTest gates.
