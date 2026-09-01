---
name: detect-agent-tool
description: Resolve the active agent harness or CLI executable and report bounded tool identity without reading model metadata.
---

# Detect Agent Tool

Use automatically from `start-work` Stage Zero, or when the user asks which
agent harness or CLI tool is executing the work. This skill owns executable
detection only. It does not select, identify, or report a model.

## Detection Boundary

Run the detector through the Git-aware Nix environment before using any other
project executable:

```sh
nix develop . --command python3 -B \
  agent/skills/detect-agent-tool/scripts/detect_agent_tool.py --json
```

When the launcher or operator knows the exact executable, remove ambiguity by
passing it directly:

```sh
nix develop . --command python3 -B \
  agent/skills/detect-agent-tool/scripts/detect_agent_tool.py \
  --executable AGENT_TOOL_EXECUTABLE --json
```

The detector resolves candidates in this order:

1. the exact `--executable` argument;
2. the exact `METAFLUX_AGENT_TOOL_EXECUTABLE` launcher declaration;
3. a recognized executable in the current process ancestry;
4. exactly one recognized CLI executable visible inside the Nix environment.

Multiple visible candidates are ambiguous. Supply the exact executable; never
choose by PATH order. The recognized-name roster is only a bounded discovery
adapter. It is not an identity allowlist: an exact executable may use another
tool-shaped name.

## Allowed Output

The JSON result contains only executable tool evidence:

- schema version, normalized executable subject, and CLI interface;
- resolved executable path and discovery source;
- the numeric tool version extracted from `--version`;
- whether `--help` succeeds; and
- the executable SHA-256 digest.

Treat the result as ephemeral startup and commit evidence. Do not persist it in
`agent/goal.json`, plans, or an execution ledger, and do not pin the local agent
harness in repository Nix declarations. Nix provides the detector runtime and
all project tools; the harness/CLI is an observed caller outside product
toolchain ownership.

## Prohibited Inputs

Do not inspect or parse model names, providers, templates, backends, build
labels, prompts, conversations, sessions, threads, repository prose, Git
configuration, or user-supplied identity labels. Never derive the subject from
version output: derive it only from the resolved executable basename. Discard
raw `--version` and `--help` output after extracting the bounded tool facts.

## Task Stops

Use the `start-work` task-stop contract. Missing, invalid, or ambiguous
executable evidence emits `agent-tool.*` with
`user-or-application / stop-and-report`: request the exact absolute harness or
CLI executable and resume only when its bounded probes pass. Invalid CLI output
selection is `current-agent / fix-and-retry`. Never replace these diagnostics
with model, provider, template, PATH-order, or repository-prose guesses.

## Verification

```sh
nix develop . --command python3 -B \
  agent/skills/detect-agent-tool/scripts/test_detect_agent_tool.py
```
