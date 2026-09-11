# Repository Tools

Repository scripts implement narrow checks owned by their domain. Nix provides
their executable toolchain; the scripts, CMake, CTest, and Git retain command
semantics and evidence ownership.

This directory owns repository commands and their support modules. Their
self-tests live under `tests/architecture/`, `tests/kernel/`, `tests/lifecycle/`,
`tests/performance/`, `tests/release/`, and `tests/transport/`. DKMS staging
entrypoints belong to [`packaging/dkms/`](../packaging/dkms/README.md).

## Build Directories

CMake presets and build entrypoints use `tmp/build/<name>` for workspace
builds. `build-generic-release.sh` consumes the `generic-release` configure
preset; `build-debug-kernel.sh`, debug qualification, KUnit, and optimization
entrypoints validate their build/cache directories before creating or resetting them.
`build_directory.py` is a Python adapter to the single path policy in
[`MetaFluxBuildDirectory.cmake`](../cmake/MetaFluxBuildDirectory.cmake).
External build directories remain supported. Retention and cleanup belong to
the invoking workflow; see [`tmp/README.md`](../tmp/README.md).

The `metaflux.architecture.build-directory` and
`metaflux.architecture.build-entrypoints` CTest gates exercise path handling,
early rejection, and explicit build selection without compiling product code.

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
- active reference prerequisites resolve to entries under `references/catalog/`;
- the current skill catalog, explicit workflow policy, links, and decision refs;
- absence of superseded execution-history paths, markers, and parsers.

The state gate also invokes `check-pytorch-cuda-readiness.py` for the registered
CPU-profile work item. It derives corpus counts, validates compiled source/subset
consistency, checks the work item's single summary and rejects completion while
the corpus is unfrozen or partially compiled. It verifies declarations only;
actual executor and device evidence remain the real-client gates' responsibility.

```sh
nix develop . --ignore-environment --keep HOME --keep USER --command python3 tools/check-agent-state.py .
nix develop . --ignore-environment --keep HOME --keep USER --command python3 tests/architecture/test-check-agent-state.py
```

The optional commit gate re-runs the candidate-tree agent-tool detector,
validates the candidate goal, and requires Git Author/Committer to match the
conversation-emitted harness subject. Epoch is read once from the candidate
`agent/goal.json`; no environment copy participates in commit identity.
`references/tools/reference.py` separately verifies exact gitlinks and
materializes only the source entry explicitly needed for research.
The automatic integration stage additionally submits exact committed revisions; the checker
proves that the current Epoch activation precedes the base, the base precedes
the tip, and the base belongs to the current Git `main` history.

```sh
nix develop . --ignore-environment --keep HOME --keep USER --command python3 tools/check-agent-state.py . --commit-gate
nix develop . --ignore-environment --keep HOME --keep USER --command python3 tools/check-agent-state.py . \
  --integration-base BASE_REVISION --integration-tip TIP_REVISION
```

The pre-commit hook materializes the exact Git index into a temporary tree and
runs its candidate state checker, routing gate and `git diff --cached --check`,
between exact commit-input guards. Behavioral self-tests remain registered in
CTest and run in the reviewed verification plan, rather than again inside the
hook. Agent commits are made through
`agent/skills/main/scripts/commit_as_agent_tool.py`. Before candidate checks,
the hook clears
the invoking repository variables reported by `git rev-parse --local-env-vars`.
The individual Git-fixture tests repeat that isolation defensively so a linked
worktree's absolute `GIT_DIR`, index, common directory, refs, and configuration
cannot become fixture output.

## Skill Routing

`check-skill-routing.py` validates the static English/Chinese routing corpus.
Every routed skill has positive and near-miss coverage, cross-domain work has
composition coverage, and explicit workflow cases name their exact skill. The
[$epoch](../agent/skills/epoch/SKILL.md) skill's proposal guard separately proves target selection,
confirmation, baseline invalidation, no-op Epoch retention, and DAG acyclicity
without writing repository state.

```sh
nix develop . --ignore-environment --keep HOME --keep USER --command python3 -B tools/check-skill-routing.py .
nix develop . --ignore-environment --keep HOME --keep USER --command python3 -B tests/architecture/test-check-skill-routing.py
nix develop . --ignore-environment --keep HOME --keep USER --command python3 -B \
  agent/skills/epoch/scripts/test_check_route_proposal.py
```

## Component Dependency Graph

`check-component-graph.py` validates the component graph emitted by
`cmake/MetaFluxComponentGraph.cmake`. It rejects edges outside the role
whitelist, C-to-C++ language-wall violations, and client links into the daemon.

```sh
nix develop . --ignore-environment --keep HOME --keep USER --command python3 tools/check-component-graph.py \
  tmp/build/dev/metaflux-component-graph.json
```

The same check runs as `metaflux.architecture.component-graph` in test-enabled
CMake presets.

## Lifecycle Model

`check-lifecycle-model.py` verifies manifest imports and hashes, then explores
the bounded generation/epoch model, loss fence, and telemetry race model. Its
compact result belongs under `tmp/outputs/` (decision-0042).

```sh
nix develop . --ignore-environment --keep HOME --keep USER --command python3 tools/check-lifecycle-model.py \
  --base-manifest contracts/protocol/transport/v1/schema/manifest.json \
  --manifest contracts/protocol/transport/v1/schema/extensions/lifecycle/v1/manifest.json \
  --model contracts/protocol/transport/v1/schema/extensions/lifecycle/v1/model.json \
  --bounds tests/lifecycle/model-bounds.json \
  --output tmp/outputs/lifecycle/model-check.json
```

Other scripts remain owned by the plan, component, release, or toolchain surface
that invokes them. Cross-component focused tests are registered in the matching
`tests/<domain>/registration.cmake`, included by `tests/CMakeLists.txt`;
component-owned gates retain their existing component registration.
