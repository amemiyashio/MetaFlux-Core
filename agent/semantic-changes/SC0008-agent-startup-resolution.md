---
id: SC0008
status: Active
created: 2026-09-01
updated: 2026-09-01
decision: D0029
session: S0100-20260901-002-agent-startup-resolution
scope: agent-startup-resolution
history_sync: automatic
effective_revision: null
superseded_by: null
---

# SC0008: agent startup resolution

## Semantic replacement

- Classification: **Breaking (destructive) governance**. This migration replaces
  the accepted agent-startup and harness-subject workflow without a compatibility
  path.
- Old meaning: D0028 accepted any normalized harness slug up to 48 characters,
  so a model, prompt template, backend build, session, or thread label could be
  submitted as Git provenance. D0022 required Nix to materialize tools but did
  not explicitly prohibit agents from probing the ambient host PATH or searching
  for an agent CLI before entering the repository development shell.
- New meaning: the harness subject is the stable active harness product slug
  supplied directly by runtime instruction context. For this harness it is
  exactly `codex`. Model, template, backend, build, session, thread, or prompt
  identifiers are invalid. Agents do not discover an agent CLI or infer identity
  from the process/environment/filesystem. Startup is Nix-first: after read-only
  repository inspection, every repository script, capability/version probe,
  compiler, build, test, packaging, and qualification command executes through
  the Git-aware `nix develop . --command ...` environment. Only Git and Nix are
  host bootstrap tools.
- Authority: the user's explicit destructive-governance instruction is executed
  under D0029. The migration will create the canonical startup-resolution
  decision; D0028 and D0022 retain their narrower identity-derivation and
  tool-ownership meanings as amended by that decision.
- Compatibility consequence: no compatibility, fallback, grandfather, alias,
  inference, or legacy-length acceptance is retained. Old commits remain factual
  Git evidence. Future sessions must use the migrated current files.

## Migration inventory

| Surface | Class | Disposition | Evidence |
| --- | --- | --- | --- |
| `AGENTS.md` | Current | Pending | Put Nix-first startup and exact harness-subject resolution before durable work |
| `agent/README.md` | Current | Pending | Make the startup command boundary part of the mandatory read-and-execute order |
| `agent/memory/constraints.md` | Current | Pending | Record the no-compatibility harness and Nix-first constraints |
| `agent/memory/decisions-index.md` | Current | Pending | Add the startup-resolution decision and keep D0022/D0028 amendment relationships resolvable |
| `agent/progress/current.md` | Current | Pending | Activation transfers governance focus; application records the completed startup policy |
| `agent/progress/focus.json` | Current | Pending | Activation transfers authority to SC0008; application returns W0112 to a successor |
| `agent/semantic-changes/README.md` | Current | Migrated | Activation indexes SC0008 as Active |
| `agent/semantic-changes/SC0008-agent-startup-resolution.md` | Current | Migrated | This Active permit records the destructive replacement and complete migration inventory |
| `agent/sessions/README.md` | Current | Pending | Activation closes the W0112 owner and indexes the migration owner; application installs the successor |
| `agent/skills/README.md` | Current | Pending | Keep start-work and manage-toolchain routing aligned with the new startup boundary |
| `agent/skills/start-work/SKILL.md` | Current | Pending | Add stage-zero harness resolution, mandatory identity preflight, and Nix-first execution |
| `agent/skills/start-work/scripts/commit_as_harness.py` | Tooling | Pending | Reject legacy-length and model/template-like subjects while retaining generic derivation |
| `agent/skills/start-work/scripts/test_commit_as_harness.py` | Tooling | Pending | Prove codex identity, legacy model/template rejection, and absence of inference |
| `agent/skills/manage-toolchain/SKILL.md` | Current | Pending | Put Nix shell entry before all repository executable and host version probes |
| `docs/architecture/agent-harness-commit-identity.md` | Current | Pending | Amend D0028 with stable runtime harness subject semantics and the new startup order |
| `docs/architecture/agent-startup-resolution.md` | Current | Pending | Create the canonical destructive startup-resolution decision |
| `docs/architecture/README.md` | Current | Pending | Index the startup-resolution decision and its amendment relationship |
| `toolchains/README.md` | Current | Pending | Amend D0022 with the Nix-first command-resolution boundary without transferring workflow ownership |
| `tools/README.md` | Current | Pending | Document the Nix-first invocation pattern for repository validation tools |
| `agent/sessions/2026/09/S0112-20260901-001-m0110-w0112-current-epoch/session.json` | Active session | Migrated | Activation terminally closes the prior product owner after checkpoint P100 |
| `agent/sessions/2026/09/S0112-20260901-001-m0110-w0112-current-epoch/events.jsonl` | Active session | Migrated | Event 12 records phase convergence and the direct governance handoff |
| `agent/sessions/2026/09/S0112-20260901-001-m0110-w0112-current-epoch/summary.md` | Active session | Migrated | Closing summary points only to the governance owner and later W0112 successor |
| `agent/sessions/2026/09/S0112-20260901-001-m0110-w0112-current-epoch/notes.md` | Active session | Migrated | Closing notes preserve the P100 boundary without retaining execution authority |
| `agent/sessions/2026/09/S0100-20260901-002-agent-startup-resolution/session.json` | Active session | Migrated | Newly scaffolded schema version 2 session becomes the sole SC0008 owner |
| `agent/sessions/2026/09/S0100-20260901-002-agent-startup-resolution/events.jsonl` | Active session | Migrated | Objective records the exact destructive replacement and product resume target |
| `agent/sessions/2026/09/S0100-20260901-002-agent-startup-resolution/summary.md` | Active session | Migrated | Activation state and bounded migration are explicit |
| `agent/sessions/2026/09/S0100-20260901-002-agent-startup-resolution/notes.md` | Active session | Migrated | Notes record the diagnosed harness and host-resolution failure modes |
| `agent/progress/checkpoints/2026/P20260901-100-m0110-cdev-live-qualification.md` | Historical | Retained evidence | Preserve the pre-migration W0112 checkpoint byte-for-byte |
| `agent/semantic-changes/SC0004-automatic-harness-identity.md` | Historical | Retained evidence | Preserve the original D0028 migration and its seven-case evidence byte-for-byte |

## Active-session handoff

| Session | Guidance | Status | Outcome |
| --- | --- | --- | --- |
| `none` | `none` | Not required | The sole affected product owner closes directly in the activation handoff; the migration owner does not receive self-guidance |

## Evidence preservation

Git retains the exact pre-SC0008 bytes and commit identities. P100 and SC0004
remain unchanged revision-bound evidence. This migration corrects future
resolution order and accepted provenance; it does not rewrite historical Git
objects or relabel old verification results.

## Future-agent reminder

This governance is destructive. Read current repository instructions, resolve
the stable harness subject directly from active runtime instruction context, and
enter the Git-aware Nix development environment before invoking repository
executables or probing tool versions. For Codex, declare `codex`; never submit
a model/template/backend/session/thread identifier, search for an agent CLI, or
probe ambient host tools first. There is no pre-SC0008 compatibility route.

## Verification

| Gate | Result |
| --- | --- |
| User breaking-change authority | Passed: explicit instruction requires destructive governance, no compatibility, exact Codex naming, and mandatory future use of migrated files |
| Current owner convergence | Activation candidate closes the P100 W0112 owner and installs one schema version 2 SC0008 governance owner |
| Active-session handoff | Passed at activation boundary: no other current active owner requires guidance |
| Behavior and residual gates | Migration must still enforce helper validation, Nix-first skill ordering, canonical documentation, routing checks, and repository regressions before application |
