# AGENTS.md — Agent Rules for MetaFlux-Core

Hard rules for any agent (human or AI) changing this repository. These are
enforced by repository checks.

1. **Read before changing.** Follow the daily read order in
   [`agent/README.md`](agent/README.md): agent rules, durable memory
   (constraints + [open decisions](agent/memory/open-decisions.md)),
   [current progress](agent/progress/current.md), the active milestone, and
   the matching [expert skill](agent/skills/README.md) if one exists.
2. **Scaffold a session before the first durable change.**

   ```sh
   python3 tools/new-session.py <slug>
   ```

   The pre-commit hook rejects non-`agent/` changes while no session is in
   progress. The session closes by cleaning its disposable work, not by
   archiving a copy of the worktree.
3. **Never relax a durable constraint silently.** Record the replacement and
   keep the decision index resolvable; use `close-decision` when an open ledger
   row is being resolved.
4. **Verify before committing.** `python3 tools/check-agent-records.py .` must
   pass; run the relevant CTest preset for build-affecting changes. Content
   and records are committed separately.
5. **Keep tool ownership narrow.** Follow `manage-toolchain` for versions,
   manifests, shells, and Nix. Nix pins and provides tools only; Git, CMake,
   CTest, packaging, tests, and sessions keep their own semantics.
6. **Record outcomes and clean work.** Distill promoted knowledge, record
   session-owned cleanup (`## Cleanup`), refresh `progress/current.md`, and
   checkpoint material handoffs (`record-session` skill).

Product boundaries live in `contracts/README.md` and
`docs/architecture/repo-layout.md`; the language wall and dependency
whitelist are machine-checked by `metaflux.architecture.component-graph`.

The Claude Code bridge (`CLAUDE.md` plus `.claude/`) is repository-local and
optional: it never touches global configuration, it is inert for other tools,
and these rules plus the pre-commit gate remain authoritative for every
contributor.
