# AGENTS.md - MetaFlux-Core Agent Rules

These rules apply to every agent changing this repository and are enforced by
the repository gates.

1. **Resolve the executing tool before repository work.** Follow `start-work`
   Stage Zero and invoke
   [`detect-agent-tool`](agent/skills/detect-agent-tool/SKILL.md) inside the
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
   and its Exit Gate, then the matching domain skill. Work only on the assigned
   `epoch-NNNN / batch-NNNN / iteration-NNNN` identity and lane. Agent identity and execution
   ownership are not repository records.
3. **Deliver one committed Iteration.** Use a separate worktree based on an
   exact revision supplied by the assignment. Do not edit `agent/goal.json`.
   Prefer bounded coding subagents for product source and test lookup, add,
   delete, and modify: the parent writes a self-contained briefing from loaded
   authority, then reviews returned diffs in conversation against that briefing
   and product boundaries before tests or commit. Do not start the next
   coding-subagent dispatch or Iteration cycle until that review accepts the
   dispatched briefing goal without drift. Coding subagents must not
   edit `goal.json`, integrate, govern, push, or create execution contexts.
   Review is conversation output only; there is no review archive. Report the
   full Epoch/Batch/Iteration identity, base and tip revisions, tests,
   blockers, and roast candidates. Uncommitted or ambient worktree state is
   never an integration input. Linked worktrees share common Git state;
   temporary-repository tests must clear Git local environment variables before
   nested Git commands. Attribute an unexpected HEAD/index/config change to the
   exact reflog and invoking hook/test before claiming another agent changed it,
   and do not multiply worktrees as an unattributed retry.
4. **Integrate only on explicit request.** A user-created integration agent
   invokes [`integrate-batch`](agent/skills/integrate-batch/SKILL.md) with exact
   committed revisions. Only that agent may update Batch/lane state, and only
   in a product integration commit after focused and combined tests pass.
5. **Govern Epochs directly.** Invoke
   [`govern-epoch`](agent/skills/govern-epoch/SKILL.md) only when the user
   explicitly requests destructive governance. Rewrite every affected current
   authority, remove obsolete semantics, and publish the next Epoch only after
   immediate full regression. There is no compatibility, historical record
   migration, or grandfather path. Work based before the active Epoch must be
   rebased and reverified before integration.
6. **Promote knowledge, not process.** At Batch integration and Epoch
   governance, invoke [`roast`](agent/skills/roast/SKILL.md). Material claims
   update exactly one canonical source, test, plan, decision, constraint, or
   experience owner. There is no roast archive, session ledger, checkpoint, or
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
   CTest/domain gates. Agent commits use the `start-work` commit helper. The
   helper accepts the conversation-emitted harness name, derives
   `SUBJECT <SUBJECT@localhost>`, and passes that name to the
   candidate-tree commit gate. Epoch exists only in `agent/goal.json`; it is not
   part of Git identity or a command environment declaration. The helper never
   stores tool facts or changes Git configuration. Candidate checks and their
   self-tests are side-effect-free with respect to the invoking repository,
   including from linked worktrees.
9. **Report actionable task stops.** Follow the sole diagnostic contract in
   [`start-work`](agent/skills/start-work/SKILL.md#task-stop-diagnostics) whenever
   a gate, required verification, or loaded Skill prohibition blocks the next
   phase. Preserve child diagnostics and raw product-tool output; do not replace
   them with a generic rejection, retry unchanged evidence, or turn an external
   responsibility into implicit scheduling, source-copy creation, privilege,
   cleanup, or repository state.
10. **Push only through the governed transport.** An ordinary Iteration commit
    is not authorization to push. After an Epoch activation commit or a Batch
    integration commit that updated `goal.json` lane or Batch state is on disk,
    the governing or integrating parent must invoke
    [`push-repository`](agent/skills/push-repository/SKILL.md) with that
    commit's full object ID. Other pushes still require an explicit user or
    application request and one full commit object ID. The skill alone
    configures and validates the canonical GitHub remote, Nix Git/OpenSSH,
    external SSH-key path and public fingerprint, and fixed `refs/heads/main`
    destination. Never read or copy private-key bytes, infer a revision from a
    dirty tree or conversation, allocate another branch, broaden a refspec, or
    force a push. Only an explicit first push may initialize canonical main.
    Coding subagents never push.

11. **Replan routes only on explicit request.** Invoke
    [`replan-roadmap`](agent/skills/replan-roadmap/SKILL.md) only when the user
    explicitly requests a primary-objective or route replan. Stage 1 is
    read-only and models milestones, work items, decisions, evidence
    prerequisites, and Iteration lanes as a dependency DAG. Stage 2 requires
    confirmation of the unchanged proposal baseline and composes `roast` with
    `govern-epoch`; the latter remains the sole destructive writer and Epoch
    publisher. Never reopen a completed milestone or work item. Active and
    Queued nodes may move in the new Epoch, and existing IDs remain when their
    delivery coordinates and observable outputs remain unchanged. A no-op does
    not advance the Epoch.

Product boundaries live in `contracts/README.md` and
`docs/architecture/repo-layout.md`; the language wall and dependency whitelist
remain machine-checked by `metaflux.architecture.component-graph`.
