# AGENTS.md — Agent Rules for MetaFlux-Core

Hard rules for any agent (human or AI) changing this repository. These are
enforced; skipping them fails the pre-commit hook or the Nix checks.

1. **Read before changing.** Follow the daily read order in
   [`agent/README.md`](agent/README.md): agent rules, durable memory
   (constraints + [open decisions](agent/memory/open-decisions.md)),
   [current progress](agent/progress/current.md), the active milestone, and
   the matching [expert skill](agent/skills/README.md) if one exists.
2. **Scaffold a session before touching anything outside `agent/`.**

   ```sh
   python3 tools/new-session.py <slug>
   ```

   The pre-commit hook rejects non-`agent/` changes while no session is in
   progress.
3. **Never relax a durable constraint.** Anything in
   [`agent/memory/constraints.md`](agent/memory/constraints.md) changes only
   through a recorded decision (`close-decision` skill), never silently.
4. **Verify before committing.** `python3 tools/check-agent-records.py .` must
   pass; run the relevant CTest preset for build-affecting changes. Content
   and records are committed separately.
5. **Record outcomes.** Distill what was promoted (`## Distillation`), close
   decisions into the index, refresh `progress/current.md`, and checkpoint on
   durable changes (`record-session` skill).

Product boundaries live in `contracts/README.md` and
`docs/architecture/repo-layout.md`; the language wall and dependency
whitelist are machine-checked by `metaflux.architecture.component-graph`.
