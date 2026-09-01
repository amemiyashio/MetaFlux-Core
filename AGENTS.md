# AGENTS.md - MetaFlux-Core Agent Rules

These rules apply to every agent changing this repository and are enforced by
the repository gates.

1. **Resolve the runtime before repository work.** Follow `start-work` Stage
   Zero. Read the stable harness product slug only from active system/developer
   runtime instructions; for Codex it is exactly `codex`, and declare it
   immediately. Do not search for an agent CLI or infer identity from a model,
   template, backend, build, process, environment, PATH, prompt, or Git config.
   Before any executable other than host `git` or `nix`, enter the Git-aware
   `nix develop . --command ...` environment. Never probe the host first or use
   `path:.`.
2. **Read the current goal.** Read [`agent/README.md`](agent/README.md), durable
   memory, [`agent/goal.json`](agent/goal.json), the target milestone/work item
   and its Exit Gate, then the matching domain skill. Work only on the assigned
   `epoch-NNNN / batch-NNNN / iteration-NNNN` identity and lane. Agent identity and execution
   ownership are not repository records.
3. **Deliver one committed Iteration.** Use a separate worktree based on an
   exact revision supplied by the assignment. Do not edit `agent/goal.json`.
   Report the full Epoch/Batch/Iteration identity, base and tip revisions,
   tests, blockers, and roast candidates. Uncommitted or ambient worktree state
   is never an integration input.
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
   credential.
8. **Verify and identify commits.** Run
   `nix develop . --command python3 tools/check-agent-state.py .` plus relevant
   CTest/domain gates. Agent commits use the `start-work` commit helper with
   `METAFLUX_AGENT_HARNESS=codex` and
   `METAFLUX_AGENT_EPOCH=<active epoch>`; Author and Committer are exactly
   `codex <codex@localhost>`.

Product boundaries live in `contracts/README.md` and
`docs/architecture/repo-layout.md`; the language wall and dependency whitelist
remain machine-checked by `metaflux.architecture.component-graph`.
