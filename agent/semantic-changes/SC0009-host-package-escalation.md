---
id: SC0009
status: Applied
created: 2026-09-01
updated: 2026-09-01
decision: D0029
session: S0100-20260901-004-host-package-bootstrap
scope: host-privilege-escalation
history_sync: automatic
effective_revision: c4cedfe2b87be9a8ed1c8022006815cfb16fb8c7
superseded_by: null
---

# SC0009: host privilege escalation

## Semantic replacement

- Classification: **Breaking (destructive) governance**. This migration
  replaces the confirmed Nix-gap terminal branch established by D0031 and the
  interactive host-privilege boundary for MetaFlux driver debugging. It retains
  neither branch as a compatibility path when a bounded helper can perform the
  exact operation.
- Old meaning: after a repository Nix declaration was attempted and Nix was
  confirmed unable to provide or materialize a required tool, the agent stopped
  and reported an exact host installation prerequisite. It never invoked a host
  package manager.
- New meaning: Nix remains first and authoritative for repeatable tool identity,
  materialization, and exposure. Only after a confirmed Nix gap may the agent
  resolve an exact Arch package and invoke a root-owned, package-name-only
  pacman installer through non-interactive sudo. If pacman cannot resolve the
  package or the bounded installer fails, the agent reports that exact blocker.
  A separate root-owned helper permits only enumerated MetaFlux driver-debug
  actions: current module lifecycle, kernel-log and kmemleak inspection, and the
  named live cdev qualification executable under the exact repository root. No
  password enters repository files, command arguments, environment variables,
  output, session records, or logs. Long-lived access persists as a
  least-privilege host authorization, not as stored credential material.
  `manage-host-privilege` is the sole workflow owner for sudo/su, root helpers,
  host authorization, package escalation, privileged driver debugging, and
  revocation. Startup, toolchain, and domain skills only route or compose it.
- Authority: the user's explicit instruction replaces the prior terminal branch
  under D0029. The migration will create the canonical escalation decision and
  amend D0031 and D0022 without transferring build, test, packaging,
  qualification, or task semantics to Nix, pacman, sudo, or the root helpers.
- Compatibility consequence: future sessions use the migrated current files.
  The pre-SC0009 stop-and-report branch is not accepted when the exact package
  is available through the bounded pacman path, and routine W0112 privileged
  debugging does not fall back to repeated password prompts. Historical Git
  evidence remains factual and no plaintext credential is retained.

## Migration inventory

| Surface | Class | Disposition | Evidence |
| --- | --- | --- | --- |
| `AGENTS.md` | Current | Migrated | Revision `c4cedfe` routes every sudo/su and privileged operation to the independent skill after Nix-first resolution |
| `agent/README.md` | Current | Migrated | Revision `c4cedfe` makes host privilege a mandatory load-on-demand owner rather than startup implementation detail |
| `agent/memory/constraints.md` | Current | Migrated | Revision `c4cedfe` records sole privilege ownership, bounded helpers, and credential exclusion |
| `agent/memory/decisions-index.md` | Current | Migrated | Revision `65fb7ce` adds D0032 and keeps D0022/D0031 amendment relationships resolvable |
| `agent/progress/current.md` | Current | Migrated | Application projects Applied SC0009, D0032, P102, and the W0112 successor |
| `agent/progress/focus.json` | Current | Migrated | Application atomically returns product focus to the newly scaffolded W0112 successor |
| `agent/progress/checkpoints/2026/P20260901-102-host-privilege-escalation.md` | Current | Migrated | Application records effective revision `c4cedfe`, host verification, dark roast, and handoff |
| `agent/semantic-changes/README.md` | Current | Migrated | Application indexes SC0009 as Applied |
| `agent/semantic-changes/SC0009-host-package-escalation.md` | Current | Migrated | This Applied record owns replacement scope, inventory, evidence, and reminder |
| `agent/sessions/README.md` | Current | Migrated | Application closes the migration owner and indexes the W0112 successor |
| `agent/skills/README.md` | Current | Migrated | Revision `c4cedfe` indexes the independent privilege skill and its composition boundary |
| `agent/skills/manage-host-privilege/SKILL.md` | Current | Migrated | Revision `c4cedfe` creates the sole sudo/su, root-helper, authorization, package-escalation, driver-debug, and revocation owner |
| `agent/skills/manage-host-privilege/agents/openai.yaml` | Current | Migrated | Revision `c4cedfe` publishes precise Codex UI metadata for host-privilege routing |
| `agent/skills/manage-host-privilege/scripts/host_privilege.py` | Tooling | Migrated | Revision `c4cedfe` moves deterministic package/action/path validation and bounded helper invocation to the new owner |
| `agent/skills/manage-host-privilege/scripts/metaflux-pacman-install` | Tooling | Migrated | Revision `c4cedfe` moves the auditable package-name-only root helper source to the new owner |
| `agent/skills/manage-host-privilege/scripts/metaflux-driver-debug` | Tooling | Migrated | Revision `c4cedfe` moves the enumerated driver-debug root helper source to the new owner |
| `agent/skills/manage-host-privilege/scripts/metaflux-host-privilege.sudoers.in` | Tooling | Migrated | Revision `c4cedfe` moves the two-command sudoers template to the new owner |
| `agent/skills/manage-host-privilege/scripts/test_host_privilege.py` | Tooling | Migrated | Revision `c4cedfe` moves bounded privilege regressions to the new owner |
| `agent/skills/start-work/SKILL.md` | Current | Migrated | Revision `c4cedfe` retains Nix-first startup and routes privilege without owning its implementation |
| `agent/skills/start-work/scripts/host_privilege.py` | Tooling | Removed | Revision `c4cedfe` removes the client from start-work after migration to its sole owner |
| `agent/skills/start-work/scripts/metaflux-pacman-install` | Tooling | Removed | Revision `c4cedfe` removes the package helper source from start-work |
| `agent/skills/start-work/scripts/metaflux-driver-debug` | Tooling | Removed | Revision `c4cedfe` removes the driver helper source from start-work |
| `agent/skills/start-work/scripts/metaflux-host-privilege.sudoers.in` | Tooling | Removed | Revision `c4cedfe` removes the sudoers template from start-work |
| `agent/skills/start-work/scripts/test_host_privilege.py` | Tooling | Removed | Revision `c4cedfe` removes privilege regression ownership from start-work |
| `agent/skills/start-work/scripts/test_commit_as_harness.py` | Tooling | Migrated | Revision `c4cedfe` proves Nix-before-privilege routing and absence of implementation details in start-work |
| `agent/skills/manage-toolchain/SKILL.md` | Current | Migrated | Revision `c4cedfe` owns only Nix-gap proof and composes the privilege owner afterward |
| `docs/architecture/agent-startup-resolution.md` | Current | Migrated | Revision `c4cedfe` routes post-Nix gaps and all elevated operations without transferring ownership to startup |
| `docs/architecture/host-privilege-escalation.md` | Current | Migrated | Revision `c4cedfe` makes the independent skill the canonical privilege workflow owner |
| `docs/architecture/README.md` | Current | Migrated | Revision `65fb7ce` indexes D0032 and its D0022/D0031 amendment relationship |
| `kernel/tests/README.md` | Current | Migrated | Revision `c4cedfe` composes privilege elevation while retaining cdev test semantics |
| `toolchains/README.md` | Current | Migrated | Revision `c4cedfe` keeps Nix narrow and routes only a proved gap to the privilege skill |
| `tools/README.md` | Current | Migrated | Revision `c4cedfe` documents independent skill invocation and ownership |
| `agent/sessions/2026/09/S0112-20260901-003-m0110-w0112-post-startup-governance/session.json` | Active session | Migrated | Activation terminally closes the product owner without product changes after P101 |
| `agent/sessions/2026/09/S0112-20260901-003-m0110-w0112-post-startup-governance/events.jsonl` | Active session | Migrated | Event 2 records phase convergence and direct governance handoff |
| `agent/sessions/2026/09/S0112-20260901-003-m0110-w0112-post-startup-governance/summary.md` | Active session | Migrated | Closing summary points to SC0009 governance and a later W0112 successor |
| `agent/sessions/2026/09/S0112-20260901-003-m0110-w0112-post-startup-governance/notes.md` | Active session | Migrated | Closing notes preserve P101 as the unchanged product resume boundary |
| `agent/sessions/2026/09/S0100-20260901-004-host-package-bootstrap/session.json` | Active session | Migrated | Newly scaffolded schema version 2 session becomes the sole SC0009 owner |
| `agent/sessions/2026/09/S0100-20260901-004-host-package-bootstrap/events.jsonl` | Active session | Migrated | Objective records exact escalation, credential, verification, and product-resume boundaries |
| `agent/sessions/2026/09/S0100-20260901-004-host-package-bootstrap/summary.md` | Active session | Migrated | Activation state and bounded migration are explicit |
| `agent/sessions/2026/09/S0100-20260901-004-host-package-bootstrap/notes.md` | Active session | Migrated | Notes distinguish persistent authorization from forbidden password storage |
| `agent/sessions/2026/09/S0112-20260901-005-m0110-w0112-post-privilege-governance/session.json` | Active session | Migrated | Newly scaffolded schema version 2 session declares M0110/W0112 and receives product focus |
| `agent/sessions/2026/09/S0112-20260901-005-m0110-w0112-post-privilege-governance/events.jsonl` | Active session | Migrated | Objective resumes W0112 only from current D0032-governed files and P102 evidence |
| `agent/sessions/2026/09/S0112-20260901-005-m0110-w0112-post-privilege-governance/summary.md` | Active session | Migrated | Handoff summary names the current W0112 boundary and independent privilege owner |
| `agent/sessions/2026/09/S0112-20260901-005-m0110-w0112-post-privilege-governance/notes.md` | Active session | Migrated | Notes preserve the exact product resume boundary without importing old session authority |
| `agent/progress/checkpoints/2026/P20260901-101-agent-startup-resolution.md` | Historical | Retained evidence | Preserve the pre-SC0009 startup-resolution checkpoint byte-for-byte |
| `agent/semantic-changes/SC0008-agent-startup-resolution.md` | Historical | Retained evidence | Preserve the original D0031 migration and its verification byte-for-byte |

## Active-session handoff

| Session | Guidance | Status | Outcome |
| --- | --- | --- | --- |
| `none` | `none` | Not required | The sole affected product owner closes directly in this activation handoff; the migration owner does not receive self-guidance |

## Evidence preservation

Git retains the exact pre-SC0009 bytes and commit identities. P101 and SC0008
remain unchanged revision-bound evidence. This migration changes only future
confirmed-Nix-gap behavior and does not rewrite prior observations or retain a
credential.

## Future-agent reminder

Nix remains first. Add and try the narrow repository declaration before any
host package path. Load `manage-host-privilege` for every sudo/su, root helper,
persistent grant, post-Nix package escalation, privileged driver action, or
revocation. Do not reproduce those mechanics in start-work, manage-toolchain,
or a domain skill. Never pass pacman options, arbitrary commands, local package
files, URLs, shell fragments, or credentials. When the bounded path can complete
the requested operation, proceed without an operator prompt. Otherwise report
the exact unresolved package, unsupported action, path violation, or
authorization failure.

## Verification

| Gate | Result |
| --- | --- |
| User breaking-change authority | Passed: explicit instruction requires automatic sudo installation for pacman-resolvable Nix gaps and persistent root access for driver debugging |
| Credential boundary | Passed at activation: no password value is recorded in repository files or session records |
| Current owner convergence | Passed: activation closed the unchanged P101 W0112 owner; application closes the migration owner and installs one schema version 2 W0112 successor |
| Active-session handoff | Passed at activation boundary: no other current active owner requires guidance |
| Independent skill ownership | Passed: revision `c4cedfe` moves all privilege implementation and tests to `manage-host-privilege`; start-work and manage-toolchain only route it |
| Host authorization | Passed: both exact helpers succeed through non-interactive sudo after timestamp invalidation; installed helper hashes match revision `c4cedfe` without package or driver mutation |
| Skill package and behavior | Passed: three package validators, privilege 7/7, startup 9/9, and shell syntax checks |
| Routing and repository records | Passed: routing corpus 89, routing self-test 34/34, record self-test 197, and semantic-change self-test 23/23 |
| Credential and cleanup boundary | Passed: no credential was persisted or emitted; the sudo timestamp and exact session temporary directory were removed |
| Content revisions | Passed repository pre-commit at `65fb7ce`, `4558103`, and `c4cedfe` |
