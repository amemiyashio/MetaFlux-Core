# AGENTS.md — Agent Rules for MetaFlux-Core

Hard rules for any agent (human or AI) changing this repository. These are
enforced by repository checks.

1. **Resolve startup, then read.** Follow `start-work` stage zero before any
   shell executable except host `git` and `nix`. Read the stable harness
   product slug only from active system/developer runtime instruction context;
   for Codex it is exactly `codex`. Declare it immediately. Never search for
   an agent CLI or derive the subject from a model/template/backend/build,
   session/thread, user prompt, repository text, process, environment, PATH, or
   Git configuration. Use the Git-aware
   `nix develop . --command ...` entry for every other executable and every
   version/capability probe; never probe the ambient host first or use
   `path:.`.
   Then follow the daily read order in
   [`agent/README.md`](agent/README.md): agent rules, durable memory
   (constraints + [open decisions](agent/memory/open-decisions.md)),
   the machine [execution focus](agent/progress/focus.json),
   [current progress](agent/progress/current.md), the focused milestone/work
   item and its Exit Gate, and the matching
   [expert skill](agent/skills/README.md) if one exists.
   The current contract is D0029 schema version 2. The focus and its owner
   session must both declare `governance_epoch: D0029`; a schema version 1 or
   pre-epoch session is never resumed for durable work.
2. **Scaffold a session before the first durable change.**

   ```sh
   nix develop . --command python3 tools/new-session.py <MAJOR.MINOR.PATCH.WORK> <slug>
   ```

   Scaffolding creates a ledger and cleanup boundary; it does not claim
   execution focus. Durable content commits require
   `METAFLUX_SESSION_ID` to name the exact `owner_session` in the candidate
   `agent/progress/focus.json`, and that session must remain `in_progress`.
   A non-owner may only terminally close itself through the exact record-only
   path. A focus handoff is record-only and atomically closes the old owner,
   installs one newly scaffolded schema version 2 owner with the D0029 epoch,
   and updates the focus. There is no legacy-session upgrade, compatibility,
   fallback, or grandfather handoff. Sessions close by
   cleaning disposable work, not by archiving a copy of the worktree.
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
4. **Converge durable collaborator deliveries.** After a collaborator reports
   durable source or record changes, invoke
   [`converge-project-changes`](agent/skills/converge-project-changes/SKILL.md)
   before the next coherent work unit. Resolve the exact delivered change set,
   repair only compatible gaps owned by the current integration session, route
   another active session through `session-guidance`, and route a breaking
   replacement through `govern-semantic-change`. Checkpoint and close are the
   fallback boundary for any delivered batch not reviewed earlier.
5. **Never replace established meaning silently.** Record the canonical
   decision and use `govern-semantic-change` for a breaking semantic,
   identifier, constraint, record-shape, or authority migration. Keep the
   decision index resolvable; compose `close-decision` when an open ledger row
   is being resolved.
6. **Verify before committing.**
   `nix develop . --command python3 tools/check-agent-records.py .` must pass;
   run the relevant CTest preset inside the declared Nix environment for
   build-affecting changes. Content
   and records are committed separately. Every agent-created commit declares
   both `METAFLUX_AGENT_HARNESS` and the exact `METAFLUX_SESSION_ID`.
7. **Keep tool ownership narrow.** Follow `manage-toolchain` for versions,
   manifests, shells, and Nix. Nix-first command resolution is mandatory, while
   Nix is strictly limited to pinning, materializing, and exposing tools; Git,
   CMake, CTest, packaging, tests, focus, and sessions keep their own commands,
   policy, and evidence. "Fixed" means clear and reproducibly stable for the
   current revision, not permanently immutable; governed manifest/lock updates
   may advance a tool. Add a newly required tool to the repository Nix
   declaration first. After a confirmed Nix provision/materialization gap,
   compose `manage-host-privilege` for exact host package resolution and
   installation. Route every sudo/su, root-helper, persistent-grant, and
   privileged MetaFlux driver operation through that skill. It owns bounded
   elevation only; Nix and domain workflows retain tool, command, semantic, and
   evidence ownership. Never persist or print a credential.
8. **Record outcomes and clean work.** Invoke `$roast` explicitly to route each
   materially promoted claim to one durable owner and semantic-transformation
   depth; keep `session-only` as an independent disposition. Record
   session-owned cleanup (`## Cleanup`), refresh `progress/current.md`, and
   checkpoint material handoffs with `record-session`.
9. **Liquidate the legacy epoch.** SC0007 is destructive governance. Before it
   applies, remove every schema version 1 session's detailed ledger from the
   current tree. Preserve only already promoted medium/dark knowledge in its
   existing canonical owner plus a non-executable settlement tombstone; do not
   retain light/session-only detail or a compatibility archive.

Product boundaries live in `contracts/README.md` and
`docs/architecture/repo-layout.md`; the language wall and dependency
whitelist are machine-checked by `metaflux.architecture.component-graph`.

The Claude Code bridge (`CLAUDE.md` plus `.claude/`) is repository-local and
optional: it never touches global configuration, it is inert for other tools,
and these rules plus the pre-commit gate remain authoritative for every
contributor.
