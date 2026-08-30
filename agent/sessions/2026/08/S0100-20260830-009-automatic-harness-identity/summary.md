# Session Summary

## Objective and outcome

Replaced the fixed Codex, Claude Code, and later-added ZCode commit-identity
table with agent-provided harness-subject derivation. The user rejected the
intermediate `/proc`/environment inference route before application. D0028 is
Verified and SC0004 is Applied at corrected content revision
`0101a4a549436e6dd8f4c94175685811d37c90d0`; the ZCode-specific and rejected
inference commits remain historical facts rather than current authority.

## Durable changes

- `docs/architecture/agent-harness-commit-identity.md`: D0028 owns generic
  agent self-declaration, deterministic identity derivation, and the workflow
  versus authentication boundary.
- `agent/skills/start-work/`: the helper has no product table or fixed selector;
  it consumes only the agent-provided command-local declaration and contains no
  process or product-environment inference.
- `agent/semantic-changes/SC0004-automatic-harness-identity.md`: synchronized
  current and protected historical interpretation while locking original Git
  objects and capture-time evidence.

## Verification

| Command/gate | Result |
| --- | --- |
| `python3 -B agent/skills/start-work/scripts/test_commit_as_harness.py` | Passed 7/7 |
| Real self-declared content commits | Passed at behavior revision `4c23b3f596badcb1c341780a93050917bd994c88` and effective revision `0101a4a549436e6dd8f4c94175685811d37c90d0` after the agent emitted `Agent harness subject: codex`; both roles are `Agent Harness (codex) <codex@localhost>` |
| No-inference and unseen-harness fixtures | Namespace-only Codex/ZCode/Claude signals fail closed; declared `future-agent` derives correctly without a mapping row |
| Cached-tree Agent record gate | Passed at closing-record commit: 30 sessions / 231 events / 219 Markdown files |
| `python3 -B tools/test-check-agent-records.py` | Passed 163/163 |
| Semantic-change staged gate and self-test | Passed; 21/21 self-test cases |
| Skill package and routing gates | Passed; 11 domain skills, 2 workflow skills, 82 corpus cases, routing self-test 34/34 |
| Guidance protocol | Passed 20/20; G005/G008 supersede G004/G007 |
| Protected-history audit | Passed; retained event blob `5b16d08e62765e483aa715bded55e954ba63794d` |
| Git diff checks | Passed |

## Cleanup

- Removed: session-generated Python bytecode cache after testing; no snapshot,
  build tree, download, or raw test log was retained.
- Retained: none owned by this session. G004/G005 and G007/G008 remain transient
  inbox packets owned by their target sessions, and concurrent P20260830-010
  progress work was preserved outside this session's commits.

## Decisions and experience

- D0028 owns agent-provided harness commit identity; SC0004 owns the complete
  replacement of both the earlier static authority and the rejected inference
  route. No separate experience record was needed because the behavior, tests,
  decision, and migration are canonical.

## roast

### light roasts

- none.

### medium roasts

- none.

### dark roasts

- Agent-created Git identity authority ->
  `docs/architecture/agent-harness-commit-identity.md` (agent self-declaration
  and no-inference derivation verified by revision
  `0101a4a549436e6dd8f4c94175685811d37c90d0`; authority: D0028, SC0004)

## session-only

- Concurrent M0100 P20260830-010 worktree state was excluded - reason: it is
  owned by another active session and is unrelated to the identity migration.

## Unresolved items

- Target-session owners still need to disposition G005/G008 and remove both
  those packets and superseded G004/G007 at their next guidance control
  boundary; publication completes this SC owner's handoff.

## Handoff

Read D0028 and `agent/skills/start-work/SKILL.md`. First surface `Agent harness
subject: <subject>` from the active AI/harness context, then validate it with
`METAFLUX_AGENT_HARNESS=HARNESS_SUBJECT python3 -B
agent/skills/start-work/scripts/commit_as_harness.py --print-identity`. Use the
same command-local declaration for agent-created commits; do not infer from
process state, add a product-specific row, or alter human Git configuration.
