# Repository Tools

Repository scripts implement narrow checks owned by their domain. Nix provides
their executable toolchain; the scripts, CMake, CTest, and Git retain command
semantics and evidence ownership.

`agent_diagnostics.py` provides the shared task-stop error shape used by Agent
workflow gates. Governed CLIs default to human stderr blocks and accept
`--diagnostic-format json` for a versioned error envelope. Successful payloads,
exit codes, and raw product-tool output remain owned by the invoking tool; the
diagnostic layer performs no repair and stores no state.

## Agent State

`check-agent-state.py` validates the decision-0033 execution model and the
decision-0034 agent-tool identity boundary:

- full-word milestone, work-item, decision, and experience identities;
- dotted delivery coordinates, plan dependencies, and Exit Gates;
- `agent/goal.json` Epoch/Batch/Iteration hierarchy and acyclic lane graph;
- the current skill catalog, explicit workflow policy, links, and decision refs;
- absence of superseded execution-history paths, markers, and parsers.

```sh
nix develop . --command python3 tools/check-agent-state.py .
nix develop . --command python3 tools/test-check-agent-state.py
```

The optional commit gate re-runs the candidate-tree agent-tool detector,
validates the candidate goal, and requires Git Author/Committer to match the
conversation-emitted harness subject. Epoch is read once from the candidate
`agent/goal.json`; no environment copy participates in commit identity.
Integration agents additionally submit exact committed revisions; the checker
proves that the current Epoch activation precedes the base, the base precedes
the tip, and the base belongs to current main history.

```sh
nix develop . --command python3 tools/check-agent-state.py . --commit-gate
nix develop . --command python3 tools/check-agent-state.py . \
  --integration-base BASE_REVISION --integration-tip TIP_REVISION
```

The pre-commit hook materializes the exact Git index into a temporary tree and
runs its candidate state checker, checker self-test, routing gates, detector and
identity-helper tests, and `git diff --cached --check`. Agent commits are made
through `agent/skills/start-work/scripts/commit_as_agent_tool.py`. Before any
candidate self-test can create a foreign temporary repository, the hook clears
the invoking repository variables reported by `git rev-parse --local-env-vars`.
The individual Git-fixture tests repeat that isolation defensively so a linked
worktree's absolute `GIT_DIR`, index, common directory, refs, and configuration
cannot become fixture output.

## Skill Routing

`check-skill-routing.py` validates the static English/Chinese routing corpus.
Every routed skill has positive and near-miss coverage, cross-domain work has
composition coverage, and explicit workflow cases name their exact skill.

```sh
nix develop . --command python3 -B tools/check-skill-routing.py .
nix develop . --command python3 -B tools/test-check-skill-routing.py
```

## Component Dependency Graph

`check-component-graph.py` validates the component graph emitted by
`cmake/MetaFluxComponentGraph.cmake`. It rejects edges outside the role
whitelist, C-to-C++ language-wall violations, and client links into the daemon.

```sh
nix develop . --command python3 tools/check-component-graph.py \
  tmp/build/dev/metaflux-component-graph.json
```

The same check runs as `metaflux.architecture.component-graph` in test-enabled
CMake presets.

## Lifecycle Model

`check-lifecycle-model.py` verifies manifest imports and hashes, then explores
the bounded generation/epoch model, loss fence, and telemetry race model. Its
compact result belongs under `tmp/outputs/` (decision-0042).

```sh
nix develop . --command python3 tools/check-lifecycle-model.py \
  --base-manifest contracts/protocol/transport/v1/schema/manifest.json \
  --manifest contracts/protocol/transport/v1/schema/extensions/lifecycle/v1/manifest.json \
  --model contracts/protocol/transport/v1/schema/extensions/lifecycle/v1/model.json \
  --bounds tests/lifecycle/model-bounds.json \
  --output tmp/outputs/lifecycle/model-check.json
```

Other scripts remain owned by the plan, component, release, or toolchain surface
that invokes them. Their focused tests are registered in `tests/CMakeLists.txt`.
