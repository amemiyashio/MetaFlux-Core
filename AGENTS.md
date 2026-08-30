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
   python3 tools/new-session.py <MAJOR.MINOR.PATCH.WORK> <slug>
   ```

   The pre-commit hook rejects non-`agent/` changes while no session is in
   progress. The session closes by cleaning its disposable work, not by
   archiving a copy of the worktree.
   Product and delivery identities follow
   [`docs/release-versioning.md`](docs/release-versioning.md).
3. **Process session guidance at control boundaries.** On session resume, a
   specialist-completion notice, before the next coherent work unit, and before
   checkpoint or close, check the active session for a ready guidance packet.
   Load [`session-guidance`](agent/skills/session-guidance/SKILL.md) only when
   one exists or guidance is explicitly requested. While acting as guidance
   author, a specialist proposes direction or a candidate patch without editing
   product source; the session owner validates it, records any material
   disposition, and removes the transient packet. Duplicate or obsolete input
   is removed as no-material without adding a session event.
4. **Never replace established meaning silently.** Record the canonical
   decision and use `govern-semantic-change` for a breaking semantic,
   identifier, constraint, record-shape, or authority migration. Keep the
   decision index resolvable; compose `close-decision` when an open ledger row
   is being resolved.
5. **Verify before committing.** `python3 tools/check-agent-records.py .` must
   pass; run the relevant CTest preset for build-affecting changes. Content
   and records are committed separately.
6. **Keep tool ownership narrow.** Follow `manage-toolchain` for versions,
   manifests, shells, and Nix. Nix pins and provides tools only; Git, CMake,
   CTest, packaging, tests, and sessions keep their own semantics.
7. **Record outcomes and clean work.** Use `distill-project-knowledge` to route
   valuable claims to one durable owner, record session-owned cleanup (`##
   Cleanup`), refresh `progress/current.md`, and checkpoint material handoffs
   with `record-session`.

Product boundaries live in `contracts/README.md` and
`docs/architecture/repo-layout.md`; the language wall and dependency
whitelist are machine-checked by `metaflux.architecture.component-graph`.

The Claude Code bridge (`CLAUDE.md` plus `.claude/`) is repository-local and
optional: it never touches global configuration, it is inert for other tools,
and these rules plus the pre-commit gate remain authoritative for every
contributor.
