---
name: main
description: Control MetaFlux repository work through evidence-bound stages, dispatch epoch/batch/iteration or specialist skills, and recover exact delivery/publication state. Use as the repository entrypoint, including read-only status, readiness, identity, maintenance, and publication diagnostics.
---

# Main

The controlling Agent owns task interpretation and calls the next skill. The
controller scripts validate transitions; they do not run an Agent, allocate an
execution context, or confer user authorization. Read the current request and
retain its limits, including a request to keep work local.

## Skill References

When naming a specific skill in explanations, progress updates, handoffs, or
repository prose, use a clickable `$name` link followed by `skill`, for example
[$main](SKILL.md) skill. Use repository-relative links in tracked Markdown and
the current checkout's absolute `SKILL.md` path in user-facing replies. Plain
text diagnostics and UI prompts use `$name skill`; machine identifiers,
frontmatter, paths, command arguments, and literal invocation examples retain
their syntax. Apply this convention to workflow, domain, and utility skills.

Epoch, Batch, and Iteration name execution concepts; Git's `main` names a
branch. Name the skill explicitly when discussing dispatch, and the concept or
branch explicitly when discussing execution state or Git. A request to govern
in the current task enters the governance skill here; it does not request a
new application task or worktree.

## Bootstrap

Before repository executables other than host Git/Nix, enter the Git-aware Nix
environment using the clean entry below. Shell grammar runs inside its bash.
Consume only the harness name already emitted in the conversation; never infer identity from processes,
PATH, a model, or repository prose.

```sh
nix develop . --ignore-environment --keep HOME --keep USER --command python3 -B agent/skills/main/scripts/detect_agent_tool.py --agent-tool HARNESS_NAME --json
nix develop . --ignore-environment --keep HOME --keep USER --command python3 -B agent/skills/main/scripts/check_git_topology.py --json
```

Every repository command submitted from the application uses this clean Nix
entry, including reads. Do not preserve ambient `PATH`, `LD_PRELOAD`, Python
paths, or shell startup variables. Nix supplies tools and their environment;
shell grammar uses `--command bash -c '...'`, without a login shell. Commands
already running inside that initialized environment may call its tools
directly, including its declared Nix for a named profile. Add any newly needed
tool to the Nix declaration instead of restoring the host search path.

For startup failures, distinguish the application launcher, Nix resolution or
shell initialization, and the invoked repository command. A loader's missing
library message alone does not prove a missing host package. Preserve its raw
output and compare clean entry with the relevant inherited variable before
attributing the cause. The [toolchain boundary](../../../toolchains/README.md#clean-tool-environment-decision-0057)
owns environment isolation and the verified preload diagnosis.

Read `agent/README.md`, memory, `agent/goal.json`, the relevant product Exit Gate,
and the domain skills owning the requested change. Nix supplies tools; CMake,
Ninja, Kbuild, CTest, and packaging keep their existing ownership. Provision
missing tools through [$manage-toolchain](../manage-toolchain/SKILL.md) skill;
confirmed host gaps and every privileged action additionally compose
[$manage-host-privilege](../manage-host-privilege/SKILL.md) skill.

For a mutating request, use `begin --request-json` to declare the bounded scope
without first writing a request file. Run `load-rules` and read its emitted
AGENTS, this skill, controller, workflow, and scope-owning skill bodies before
`step prepared`. A receipt binds their exact file modes/blobs, current HEAD,
request and base; it records body emission, not comprehension or authorization.
Begin records existing edits, and preparation rejects further edits made before
`prepared`. Read additional references required by the loaded skills yourself.
The [controller interface](references/controller.md) owns the exact protocol.

The project [tool hooks](references/tool-hooks.md) inject missing
rules and deny the current covered write until preparation succeeds. Their
enforcement depends on user/application trust in the exact hook definition;
the repository does not activate that trust. They guard supported tool calls,
not arbitrary shell execution.

## Dispatch

- Inspection, status, readiness, and identity requests stay read-only. Use
  [readiness](references/evidence-framework.md) for evidence classification.
  A question during active work does not replace its state or trigger a commit.
- Maintenance uses this controller's preparation, implementation, parent review,
  evaluation, guarded commit, and publication stages. It has no invented
  product Iteration identity and leaves Goal state unchanged.
- An assigned product candidate uses [$iteration](../iteration/SKILL.md) skill;
  its exact committed delivery goes automatically to [$batch](../batch/SKILL.md) skill
  in the same controlling turn.
- An explicit route or governance request uses [$epoch](../epoch/SKILL.md) skill. Discovering drift in
  ordinary work is evidence to report, not a new governance authorization.
- Standalone knowledge promotion uses [knowledge promotion](references/knowledge-promotion.md)
  within the requested scope. Identity and publication diagnostics use the
  shared helpers without manufacturing a delivery.

Read [the executable interface](references/controller.md) before mutating
controller state. `$main status` maps to `inspect`; `$main resume` revalidates
Git and evidence before selecting another operation. The script exposes
`inspect`, `begin`, `load-rules`, `step`, `resume`, `rescope`, and `supersede`.

The controller reports stage, evidence, next operation, and delivery target.
Preparation, implementation, review, evaluation, delivery, publication, and
handoff are stages, not new numbered execution levels. A required check that
fails returns to bounded repair. A declared optional environment skip is not
evidence for a product requirement that needed that environment.

## Shared Delivery Boundary

The parent reviews returned diffs against its briefing before verification or
another coding dispatch. Coding subagents never change Goal, accept, govern,
commit, publish, or create execution contexts. The application supplies worker
and integration contexts; do not manufacture a branch, worktree, clone, task,
thread, or chat to satisfy a gate.

Commit only through `scripts/commit_as_agent_tool.py`, with the conversation
subject and exact expected HEAD/tree. The helper checks the active verification
receipt, its rule certificate, and transaction before invoking the candidate-tree
commit gate. Pre-commit repeats the same input guard before and after candidate
checks, requiring full expected HEAD/tree, receipt, and operation kind. It never
modifies Git identity configuration. Both paths require this controller's active delivery
state and its current loaded rules; an old receipt does not revive a resumed or
replaced operation.

Review, evaluation, delivery, and publication require current loaded rules.
When rule content or mode changes, load the new bodies before reviewing and
verifying the candidate. After commit, load them again at the new HEAD before
publication. Candidate validation checks the rule versions in that candidate;
integration needs its own current rules, fresh parent review, and verification.

Maintenance, Batch acceptance, and Epoch activation publish their exact guarded
commit automatically unless the current user request limits publication. An
ordinary worker candidate goes to [$batch](../batch/SKILL.md) skill for acceptance.
Use [publication](references/publication.md); do not force, broaden the refspec,
or read private-key bytes. A publication failure preserves that commit and
resumes publication only. Next work starts only after exact publication and a
matching application-supplied context.

## State And Recovery

`agent/goal.json` owns product route and accepted progress. Ignored
`agent/tmp/main/` owns only the current worktree's running state, rule/context
evidence, verification receipts, and pending transaction. Build and product-test
artifacts stay under root `tmp/`. No temporary state is authorization or product
completion proof.

Use the common Git lock and expected HEAD/tree checks for shared mutations.
Recheck the received scope and Git facts after interruption. If pre-commit
state is lost, review and evaluate again; do not guess an accepted transition.
If a validated acceptance commit exists, recover it from Git before attempting
another acceptance. Preserve external edits detected during recovery.

Treat user intent, application assignment, and Agent-declared file paths as
distinct inputs. Before begin, trace tests, manifests, generated consumers and
their canonical summaries to include necessary companion files. If that impact
analysis missed a same-task file, use `rescope` with the current inspection
token and explain the dependency. Existing authorization covers necessary work
within that intent; do not request a new assignment just to correct your path
declaration. Reload rules, prepare, review and verify the revised candidate.
Goal ownership, objective, assignment, baseline and publication limits stay fixed.

An explicit confirmed governance request may use `supersede` to replace an
unfinished pre-commit operation with an Epoch operation. Preserve unrelated
candidate edits in Git before constructing the governance tree, and restore
their exact contents after publication. Never delete state to simulate cache
loss or edit a receipt to unlock work. The [controller interface](references/controller.md#scope-amendment-and-governance-replacement)
owns recovery inputs and failure boundaries.

## Task-Stop Diagnostics

The shared `tools/agent_diagnostics.py` contract is the sole diagnostic shape:
`code`, `source`, `summary`, `evidence[]`, `responsibility`, `disposition`,
`required_action`, `resume_when`, and an optional exact `retry_command`.
Responsibility is `current-agent`, `user-or-application`, `batch-integrator`,
`epoch-governor`, or `host-operator`. Disposition is `fix-and-retry`,
`stop-and-report`, or `preserve-and-report`.

Preserve child diagnostics and raw product output. A current-agent repair
retries only after evidence or prerequisites change. External responsibility
does not authorize scheduling, privilege, cleanup, or an unchanged retry.
An Agent's omitted path is a current-agent repair, not evidence that the user
withheld permission. A delivery gate failure blocks that delivery; continue
independent in-scope diagnosis and tests when they still produce useful evidence.
Report the exact failed phase and remaining useful work instead of treating
repeated messages as proof that the entire task is blocked. Bind each success
claim to the behavior and inputs its test actually exercised; old regression,
harness self-tests and builds do not qualify a newly added product path.
Diagnostics are output and current-operation recovery data, never a ledger.
