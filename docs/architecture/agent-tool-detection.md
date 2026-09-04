---
status: Verified
decision: decision-0034
updated: 2026-09-01
---

# Agent Tool Detection

## Decision

MetaFlux derives agent commit identity from bounded evidence about the executing
harness or CLI executable. `detect-agent-tool` runs inside the Git-aware Nix
environment and reports only the normalized executable subject, resolved path,
numeric tool version, discovery source, help-probe availability, and executable
digest. The result is ephemeral and is never stored in repository goal state.

This decision replaces the fixed agent product identity retained by
decision-0033. The execution topology remains unchanged; decision-0035 completed
the epoch-0003 governance transition, later Epochs superseded it, and the
current Epoch is epoch-0006.

## Rationale

A fixed product name correctly rejected model-derived labels but confused a
repository policy default with the tool actually executing a commit. It also
made another supported harness appear invalid even when its executable identity
was unambiguous. Executable evidence preserves the needed boundary without
coupling the repository to one agent product.

## Detection Order

1. Use an exact executable supplied by the current command.
2. Otherwise use the exact launcher declaration
   `METAFLUX_AGENT_TOOL_EXECUTABLE`.
3. Otherwise accept exactly one recognized executable in process ancestry.
4. Otherwise accept exactly one recognized CLI executable visible from inside
   the Nix environment.

Ambiguous discovery fails and requires an exact executable. PATH order never
selects between candidates. The recognized-name roster is a bounded discovery
adapter rather than an identity allowlist; exact tool-shaped executables remain
valid even when they are absent from that roster.

## Information Boundary

The subject comes only from the resolved executable basename. The version comes
only from a bounded `--version` probe, and raw probe output is discarded after
extracting the numeric tool version. Model, provider, template, backend, build,
prompt, conversation, session, thread, repository prose, Git configuration, and
user labels are neither identity sources nor output fields. A model-shaped
executable name or model-bearing version response is rejected instead of being
normalized into a tool identity.

The project does not pin or install the caller harness through Nix. Nix owns the
Python detector runtime and project tools; the detected executable is observed
caller evidence outside product toolchain ownership.

## Commit Boundary

The commit helper detects the tool, derives
`SUBJECT <SUBJECT@localhost>`, and passes only the exact resolved executable to
the child commit. The pre-commit checker repeats the detector against the
candidate tree and requires Author and Committer to match that subject. The
single active Epoch remains in `agent/goal.json`; it is not duplicated into Git
identity, an environment declaration, or Git config. Neither helper nor checker
changes Git config or persists detector output.

## Verification

- deterministic exact, launcher, ancestor/path, ambiguity, contamination, and
  command-output tests for the detector;
- dynamic helper Author/Committer tests using a fixture executable;
- candidate-tree commit-gate tests that re-probe the declared executable;
- residual rejection of the superseded fixed-harness declaration and helper
  names; and
- skill metadata, bilingual routing, state, architecture, and full CTest gates.
