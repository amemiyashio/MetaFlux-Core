# AGENTS.md - MetaFlux-Core Agent Rules

These rules apply to every agent changing this repository and are enforced by
the repository gates.

1. **Enter the main workflow before repository work.** Follow main Bootstrap and invoke
   [`main`](agent/skills/main/SKILL.md) inside the
   Git-aware Nix environment. Consume only the harness name already emitted in
   this conversation (`zcode`, `codex`, `claude`, or another tool-shaped name)
   after it normalizes to a subject. Do not search PATH, processes, `/proc`,
   executables, Git configuration, or repository prose for identity, and do not
   probe `--version`. Model/provider/template/backend/build/prompt/session/
   thread data is never an identity input or output. Before any other
   executable except host `git` or `nix`, use
   `nix develop . --command ...`; never probe the ambient host first or use
   `path:`. Shell grammar for repository work runs under the Nix-provided
   bash via `nix develop . --command bash -c '...'`; the ambient host shell
   never executes repository tools.
2. **Read the current goal.** Read [`agent/README.md`](agent/README.md), durable
   memory, [`agent/goal.json`](agent/goal.json), the target milestone/work item
   and its Exit Gate, then the matching domain skill. Product work uses the assigned
   `epoch-NNNN / batch-NNNN / iteration-NNNN` identity and lane. Agent identity and execution
   ownership are not repository records. Before a mutating operation, begin its
   bounded request and use main `load-rules` to emit the current rule bodies into
   the executing context, then enter `prepared`. Main checks the request/base,
   HEAD, required workflow/domain skills, and exact rule file modes and blobs;
   a claimed reading or copied receipt is insufficient. Existing edits are
   captured at begin; edits made after begin and before preparation invalidate
   that preparation. The receipt records body emission, not comprehension or
   user authorization. Reload changed rules before review and verification,
   and reload at the committed HEAD before publication. The
   [controller interface](agent/skills/main/references/controller.md) owns this
   protocol. The [tool hooks](agent/skills/main/references/tool-hooks.md) apply it to
   covered tool calls when the user/application trusts the exact hook definition;
   repository files do not activate that host trust or provide a shell sandbox.
3. **Deliver the bounded scope.** Main handles read-only work and maintenance;
   maintenance leaves Goal unchanged and has no product Iteration identity.
   Product Iterations use the exact application-supplied context and base.
   Workers do not edit `agent/goal.json` or allocate execution contexts.
   Prefer bounded coding subagents for product source and test lookup, add,
   delete, and modify: the parent writes a self-contained briefing from loaded
   authority, then reviews returned diffs in conversation against that briefing
   and product boundaries before tests or commit. Do not start the next
   coding-subagent dispatch or Iteration cycle until that review accepts the
   dispatched briefing goal without drift. Coding subagents must not
   edit `goal.json`, integrate, govern, commit, push, or create execution contexts.
   Review is conversation output only; there is no review archive. Report the
   full Epoch/Batch/Iteration identity, base and tip revisions, tests,
   blockers, and knowledge candidates. Uncommitted or ambient worktree state is
   never an integration input. Linked worktrees share common Git state;
   temporary-repository tests must clear Git local environment variables before
   nested Git commands. Attribute an unexpected HEAD/index/config change to the
   exact reflog and invoking hook/test before claiming another agent changed it,
   and do not multiply worktrees as an unattributed retry.
4. **Accept and advance qualified deliveries automatically.** After a worker
   emits an exact committed delivery with passing focused tests and no blockers,
   the controlling parent invokes
   [`batch`](agent/skills/batch/SKILL.md) in the same
   turn without another user instruction. Its controller rejects empty or stale
   candidates, performs merge and fresh combined verification, then alone
   advances accepted state. A slice keeps the lane/work item/target and assigns
   the Batch maximum Iteration plus one. Whole-work-item acceptance requires
   its full Exit Gate. Next work follows DAG dependencies and lane array order.
   Batch completion does not complete a milestone or create an Epoch.
   It pushes the exact acceptance commit and exposes the next
   dependency-ready lane; execution-context creation remains application-owned.
5. **Govern Epochs directly.** Invoke
   [`epoch`](agent/skills/epoch/SKILL.md) only when the user
   explicitly requests destructive governance. Rewrite every affected current
   authority, remove obsolete semantics, and publish the next Epoch only after
   immediate full regression. There is no compatibility, historical record
   migration, or grandfather path. Work based before the active Epoch must be
   rebased and reverified before integration.
6. **Promote knowledge, not process.** At automatic acceptance and Epoch
   governance, use main's [knowledge promotion](agent/skills/main/references/knowledge-promotion.md). Material claims
   update exactly one canonical source, test, plan, decision, constraint, or
   experience owner. There is no knowledge archive, session ledger, checkpoint, or
   progress diary; Git preserves prior states.
7. **Keep tool and privilege ownership narrow.** Nix pins, materializes, and
   exposes clear, reproducibly stable tool versions; it does not own builds,
   tests, packaging, product semantics, or host cleanup. Add required tools to
   Nix first. After a confirmed Nix gap, compose `manage-host-privilege` for
   bounded pacman/root operations. Route every sudo/su, persistent grant, and
   privileged driver action through that skill. Never persist or print a
   credential. When a required tool or dependency is absent from the current
   environment, treat it as a provisioning task (Nix first, then
   manage-host-privilege), not as a task blocker. Only declare inability to
   proceed after exhausting both provisioning paths (decision-0036).
8. **Verify and identify commits.** Run
   `nix develop . --command python3 tools/check-agent-state.py .` plus relevant
   CTest/domain gates. Agent commits use main's shared commit helper, requiring
   full expected HEAD, exact staged tree, operation kind, and an actual
   content-bound receipt with its rule-loading certificate. Pre-commit invokes
   the same commit guard before and after candidate-tree checks; direct Git
   invocation does not replace those inputs. The
   helper accepts the conversation-emitted harness name, derives
   `SUBJECT <SUBJECT@localhost>`, and passes that name to the
   candidate-tree commit gate. Epoch exists only in `agent/goal.json`; it is not
   part of Git identity or a command environment declaration. The helper never
   stores tool facts or changes Git configuration. Candidate checks and their
   self-tests are side-effect-free with respect to the invoking repository,
   including from linked worktrees.
9. **Report actionable task stops.** Follow the sole diagnostic contract in
   [`main`](agent/skills/main/SKILL.md#task-stop-diagnostics) whenever
   a gate, required verification, or loaded Skill prohibition blocks the next
   phase. Preserve child diagnostics and raw product-tool output; do not replace
   them with a generic rejection, retry unchanged evidence, or turn an external
   responsibility into implicit scheduling, source-copy creation, privilege,
   cleanup, or repository state.
10. **Publish exact deliveries through the governed transport.** Maintenance,
    Epoch activation, and Batch acceptance automatically publish unless the user
    limits publication. Worker candidates go to Batch without pushing.
    After the guarded commit is on disk, the controlling parent invokes
    [`main`](agent/skills/main/SKILL.md) with that
    commit's full object ID. Standalone pushes require an explicit user or
    application request and one full commit object ID. The skill alone
    configures and validates the canonical GitHub remote, Nix Git/OpenSSH,
    external SSH-key path and public fingerprint, and fixed `refs/heads/main`
    destination. Never read or copy private-key bytes, infer a revision from a
    dirty tree or conversation, allocate another branch, broaden a refspec, or
    force a push. Only an explicit first push may initialize canonical main.
    Verify remote revision equality or actual ancestry. Publication recovery
    retains the same commit; remote advancement needs a new application base.
    Coding subagents never push.

11. **Replan routes only on explicit request.** Invoke
    [`epoch`](agent/skills/epoch/SKILL.md) only when the user
    explicitly requests a primary-objective or route replan. Stage 1 is
    read-only and models milestones, work items, decisions, evidence
    prerequisites, and Iteration lanes as a dependency DAG. Stage 2 requires
    confirmation of the unchanged proposal baseline; an already approved full
    implementation plan supplies confirmation. Epoch remains the sole destructive
    writer and publisher. Never reopen a completed milestone or work item. Active and
    Queued nodes may move in the new Epoch, and existing IDs remain when their
    delivery coordinates and observable outputs remain unchanged. A no-op does
    not advance the Epoch.
12. **Materialize reference sources only on demand.** `references/` owns
    research-only catalog manifests, notes, and exact upstream submodule
    gitlinks. A normal clone, build, test, package, or release must not recurse
    into them. When the assigned lane names a reference prerequisite in
    `agent/goal.json`, use `references/tools/reference.py` to materialize and
    verify that entry before implementation. Treat the detached checkout as
    read-only. A relied-upon product claim must be promoted to one canonical
    source, test, contract, decision, constraint, or plan; a build or
    qualification input must instead be pinned by `toolchains/`.

13. **Keep recovery state separate from product authority.** Goal schema v4 is
    the sole product route and accepted progress authority. Git-ignored
    `agent/tmp/main/` holds current operation state, rule/context evidence,
    verification receipts, and pending transactions only; none may be tracked
    or used as authorization. Build and product-test outputs remain under root
    `tmp/`. Use main's common Git lock,
    exact before/after blobs and modes, and validated commit records for recovery.
    Lost pre-commit state requires new review and evaluation. Existing matching
    contexts continue automatically; otherwise emit an exact application
    assignment request without manufacturing a context.

Product boundaries live in `contracts/README.md` and
`docs/architecture/repo-layout.md`; the language wall and dependency whitelist
remain machine-checked by `metaflux.architecture.component-graph`.
