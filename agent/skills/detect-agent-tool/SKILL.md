---
name: detect-agent-tool
description: Report the harness name already emitted in this conversation without probing executables or model metadata.
---

# Detect Agent Tool

Use automatically from `start-work` Stage Zero, or when the user asks which
agent harness is executing the work. This skill owns the conversation-emitted
harness name only. It does not select, identify, or report a model, and it does
not inspect PATH, processes, `/proc`, or an executable.

## Detection Boundary

Pass the harness name already shown in this conversation (`zcode`, `codex`,
`claude`, or another tool-shaped name) through the Git-aware Nix environment:

```sh
nix develop . --command python3 -B \
  agent/skills/detect-agent-tool/scripts/detect_agent_tool.py \
  --agent-tool HARNESS_NAME --json
```

The detector accepts exactly one declaration:

1. the `--agent-tool` argument; otherwise
2. the `METAFLUX_AGENT_TOOL` launcher declaration.

There is no PATH scan, process-ancestry walk, executable probe, `--version`
probe, or digest. If neither declaration is present, stop and ask for the
harness name already emitted in the conversation.

## Allowed Output

The JSON result contains only:

- schema version, normalized harness subject, CLI interface, and discovery
  source `declared`.

Treat the result as ephemeral startup and commit evidence. Do not persist it in
`agent/goal.json`, plans, or an execution ledger, and do not pin the local agent
harness in repository Nix declarations.

## Prohibited Inputs

Do not inspect or parse model names, providers, templates, backends, build
labels, prompts, sessions, threads, repository prose, Git configuration, PATH,
process tables, or user-supplied identity labels that were not the harness name
already emitted in this conversation. Never derive the subject from an
executable basename or version output.

## Task Stops

Use the `start-work` task-stop contract. Missing, invalid, or contaminated
harness names emit `agent-tool.*` with
`user-or-application / stop-and-report`: request the conversation-emitted
harness name and resume only when it normalizes to a tool-shaped subject.
Invalid CLI output selection is `current-agent / fix-and-retry`. Never replace
these diagnostics with model, PATH-order, process, or repository-prose guesses.

## Verification

```sh
nix develop . --command python3 -B \
  agent/skills/detect-agent-tool/scripts/test_detect_agent_tool.py
```
