---
id: SC0008
status: Applied
created: 2026-09-01
updated: 2026-09-01
decision: D0029
session: S0100-20260901-002-agent-startup-resolution
scope: agent-startup-resolution
history_sync: automatic
effective_revision: a7e370d68c660a1d42210631641bef0d3406cf91
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
  supplied directly by system/developer runtime instruction context. For this
  harness it is exactly `codex`. Model, template, backend, build, CLI, session,
  thread, or prompt identifiers are invalid. Agents do not discover an agent CLI
  or infer identity from process/environment/filesystem state. Startup is
  Nix-first: after read-only repository inspection, every repository script,
  capability/version probe, compiler, build, test, packaging, and qualification
  command executes through the Git-aware
  `nix develop . --command ...` environment. Only Git and Nix are host
  bootstrap tools.
- Authority: the user's explicit destructive-governance instruction is executed
  under D0029. D0031 is the canonical startup-resolution decision; D0028 and
  D0022 retain their narrower identity-derivation and tool-ownership meanings as
  amended by D0031.
- Compatibility consequence: no compatibility, fallback, grandfather, alias,
  inference, or legacy-length acceptance is retained. Old commits remain factual
  Git evidence. Future sessions must use the migrated current files.

## Migration inventory

| Surface | Class | Disposition | Evidence |
| --- | --- | --- | --- |
| `AGENTS.md` | Current | Migrated | Revision `a7e370d` requires Nix-first startup/acquisition and strictly limits Nix to versioned tool provision |
| `agent/README.md` | Current | Migrated | Revision `73b471f` makes startup and missing-tool resolution part of the mandatory read-and-execute order |
| `agent/memory/constraints.md` | Current | Migrated | Revision `a7e370d` records harness, Nix-first/escalation, strict ownership, and governed version evolution constraints |
| `agent/memory/decisions-index.md` | Current | Migrated | Revision `7db7bcf` Add the startup-resolution decision and keep D0022/D0028 amendment relationships resolvable |
| `agent/progress/current.md` | Current | Migrated | Application projects D0031/Applied SC0008 and the post-governance W0112 owner |
| `agent/progress/focus.json` | Current | Migrated | Application atomically returns product focus to the newly scaffolded W0112 successor |
| `agent/progress/checkpoints/2026/P20260901-101-agent-startup-resolution.md` | Current | Migrated | Application records effective revision `a7e370d`, verification, roast, and handoff |
| `agent/semantic-changes/README.md` | Current | Migrated | Application indexes SC0008 as Applied |
| `agent/semantic-changes/SC0008-agent-startup-resolution.md` | Current | Migrated | This Applied record owns the destructive replacement, inventory, evidence, and reminder |
| `agent/sessions/README.md` | Current | Migrated | Application closes the migration owner and indexes the W0112 successor |
| `agent/skills/README.md` | Current | Migrated | Revision `7db7bcf` Keep start-work and manage-toolchain routing aligned with the new startup boundary |
| `agent/skills/start-work/SKILL.md` | Current | Migrated | Revision `a7e370d` adds stage zero, preflight, Nix-first acquisition, strict ownership, and version-evolution meaning |
| `agent/skills/start-work/scripts/commit_as_harness.py` | Tooling | Migrated | Revision `7db7bcf` Reject legacy-length and model/template-like subjects while retaining generic derivation |
| `agent/skills/start-work/scripts/test_commit_as_harness.py` | Tooling | Migrated | Revision `a7e370d` proves Codex identity, non-harness rejection, no inference, Nix-first ordering/acquisition, and strict Nix ownership text |
| `agent/skills/manage-toolchain/SKILL.md` | Current | Migrated | Revision `a7e370d` puts Nix entry/acquisition before escalation and limits Nix to evolvable versioned tool provision |
| `docs/architecture/agent-harness-commit-identity.md` | Current | Migrated | Revision `7db7bcf` Amend D0028 with stable runtime harness subject semantics and the new startup order |
| `docs/architecture/agent-startup-resolution.md` | Current | Migrated | Revision `a7e370d` creates D0031 with destructive startup, acquisition order, strict Nix ownership, and governed version evolution |
| `docs/architecture/README.md` | Current | Migrated | Revision `7db7bcf` Index the startup-resolution decision and its amendment relationship |
| `toolchains/README.md` | Current | Migrated | Revision `a7e370d` limits D0022 to clear, stable, evolvable tool identity/materialization/exposure |
| `tools/README.md` | Current | Migrated | Revision `a7e370d` documents Nix-first invocation/escalation without command, policy, or evidence transfer |
| `agent/sessions/2026/09/S0112-20260901-001-m0110-w0112-current-epoch/session.json` | Active session | Migrated | Activation terminally closes the prior product owner after checkpoint P100 |
| `agent/sessions/2026/09/S0112-20260901-001-m0110-w0112-current-epoch/events.jsonl` | Active session | Migrated | Event 12 records phase convergence and the direct governance handoff |
| `agent/sessions/2026/09/S0112-20260901-001-m0110-w0112-current-epoch/summary.md` | Active session | Migrated | Closing summary points only to the governance owner and later W0112 successor |
| `agent/sessions/2026/09/S0112-20260901-001-m0110-w0112-current-epoch/notes.md` | Active session | Migrated | Closing notes preserve the P100 boundary without retaining execution authority |
| `agent/sessions/2026/09/S0100-20260901-002-agent-startup-resolution/session.json` | Active session | Migrated | Application closes the sole migration owner at content revision `a7e370d` |
| `agent/sessions/2026/09/S0100-20260901-002-agent-startup-resolution/events.jsonl` | Active session | Migrated | Events record activation, exact verification, SC application, roast, and successor handoff |
| `agent/sessions/2026/09/S0100-20260901-002-agent-startup-resolution/summary.md` | Active session | Migrated | Closing summary records D0031, verification, cleanup, dark roast, and W0112 handoff |
| `agent/sessions/2026/09/S0100-20260901-002-agent-startup-resolution/notes.md` | Active session | Migrated | Closing notes record the enforced startup boundary and no-compatibility result |
| `agent/sessions/2026/09/S0112-20260901-003-m0110-w0112-post-startup-governance/session.json` | Active session | Migrated | Newly scaffolded schema version 2 session declares M0110/W0112 and receives product focus |
| `agent/sessions/2026/09/S0112-20260901-003-m0110-w0112-post-startup-governance/events.jsonl` | Active session | Migrated | Objective resumes W0112 only from current D0031-governed files and P100 product evidence |
| `agent/sessions/2026/09/S0112-20260901-003-m0110-w0112-post-startup-governance/summary.md` | Active session | Migrated | Handoff summary names the current W0112 boundary and Nix-first startup contract |
| `agent/sessions/2026/09/S0112-20260901-003-m0110-w0112-post-startup-governance/notes.md` | Active session | Migrated | Notes preserve the exact product resume boundary without importing old session authority |
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
the stable harness subject directly from active system/developer runtime
instruction context, and enter the Git-aware Nix development environment before
invoking repository executables or probing tool versions. For Codex, declare
`codex`; never submit a model/template/backend/build/CLI/session/thread/prompt
identifier, search for an agent CLI, or probe ambient host tools first. There is
no pre-SC0008 compatibility route. Add missing tools to the repository Nix
declaration first and report an exact host prerequisite only when Nix cannot
provide or materialize the tool. Nix is limited to tool-version identity,
materialization, and exposure. A fixed version is clear, reproducible, and
stable for the current revision; later governed manifest and lock updates may
evolve it.

## Verification

| Gate | Result |
| --- | --- |
| User breaking-change authority | Passed: explicit instruction requires destructive governance, no compatibility, exact Codex naming, and mandatory future use of migrated files |
| Current owner convergence | Passed: activation closed the P100 W0112 owner; application closes the migration owner and installs one schema version 2 W0112 successor |
| Active-session handoff | Passed at activation boundary: no other current active owner requires guidance |
| Harness helper and policy | Passed: 9/9 cases; the `github-gpt-5-6-sol-unrestricted-33b86c71` subject is rejected and `codex` resolves exactly |
| Skill package and routing | Passed: Codex skill validator, 89-case routing corpus, and 34/34 routing self-tests |
| Agent and semantic-change records | Passed: current records, 197 record self-tests, and 23/23 semantic-change edit tests |
| Nix-first tool probes | Passed: Nix-provided ripgrep 15.2.0, Python 3.13.15, CMake/CTest 4.1.6, and Ninja 1.13.2 |
| Tool acquisition and ownership | Passed: tools route to Nix first; confirmed gaps report host prerequisites; Nix remains limited to clear, stable, evolvable versioned provision |
| Residual and pre-commit gates | Passed: no host-first Python command remains on migrated startup surfaces; content revisions `7db7bcf`, `73b471f`, and `a7e370d` passed the repository pre-commit gate |
