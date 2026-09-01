---
id: SC0009
status: Active
created: 2026-09-01
updated: 2026-09-01
decision: D0029
session: S0100-20260901-004-host-package-bootstrap
scope: host-privilege-escalation
history_sync: automatic
effective_revision: null
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
| `AGENTS.md` | Current | Pending | Replace confirmed-Nix-gap interruption and repeated driver-debug prompts with bounded host escalation and no secret persistence |
| `agent/README.md` | Current | Pending | Put bounded host installation after failed Nix materialization and route driver debugging through its fixed helper |
| `agent/memory/constraints.md` | Current | Pending | Record Nix-first acquisition, allowlisted driver privilege, and no-secret durable constraints |
| `agent/memory/decisions-index.md` | Current | Pending | Add the canonical escalation decision and retain amendment links to D0022/D0031 |
| `agent/progress/current.md` | Current | Pending | Activation projects SC0009 governance; application returns W0112 to a successor |
| `agent/progress/focus.json` | Current | Pending | Activation transfers authority to SC0009; application returns product focus |
| `agent/progress/checkpoints/2026/P20260901-102-host-privilege-escalation.md` | Current | Pending | Application will record effective revision, host authorization verification, roast, and handoff |
| `agent/semantic-changes/README.md` | Current | Migrated | Activation indexes SC0009 as Active |
| `agent/semantic-changes/SC0009-host-package-escalation.md` | Current | Migrated | This Active permit owns replacement scope, inventory, evidence, and reminder |
| `agent/sessions/README.md` | Current | Pending | Activation closes the W0112 owner and installs governance; application installs a successor |
| `agent/skills/start-work/SKILL.md` | Current | Pending | Replace confirmed-gap and driver-debug prompts with bounded non-interactive host privilege paths |
| `agent/skills/start-work/scripts/host_privilege.py` | Tooling | Pending | Add deterministic package/action/path validation and bounded root-helper invocation |
| `agent/skills/start-work/scripts/metaflux-pacman-install` | Tooling | Pending | Add auditable root helper source with exact pacman operation and argument validation |
| `agent/skills/start-work/scripts/metaflux-driver-debug` | Tooling | Pending | Add auditable root helper source with enumerated module, log, kmemleak, and live-test actions |
| `agent/skills/start-work/scripts/test_host_privilege.py` | Tooling | Pending | Prove package/action/path validation, command bounds, dry checks, and secret-free behavior |
| `agent/skills/start-work/scripts/test_commit_as_harness.py` | Tooling | Pending | Extend static startup policy regression to the new escalation order and ownership boundary |
| `agent/skills/manage-toolchain/SKILL.md` | Current | Pending | Route only confirmed Nix gaps to the bounded host installer without changing Nix ownership |
| `docs/architecture/agent-startup-resolution.md` | Current | Pending | Amend D0031's terminal gap branch and preserve all other startup semantics |
| `docs/architecture/host-privilege-escalation.md` | Current | Pending | Create the canonical bounded package/driver escalation and credential boundary |
| `docs/architecture/README.md` | Current | Pending | Index the canonical escalation decision and its amendment relationship |
| `toolchains/README.md` | Current | Pending | Amend D0022 with the post-Nix host-install boundary while keeping Nix narrow |
| `tools/README.md` | Current | Pending | Document exact helper invocation and failure classification |
| `agent/sessions/2026/09/S0112-20260901-003-m0110-w0112-post-startup-governance/session.json` | Active session | Migrated | Activation terminally closes the product owner without product changes after P101 |
| `agent/sessions/2026/09/S0112-20260901-003-m0110-w0112-post-startup-governance/events.jsonl` | Active session | Migrated | Event 2 records phase convergence and direct governance handoff |
| `agent/sessions/2026/09/S0112-20260901-003-m0110-w0112-post-startup-governance/summary.md` | Active session | Migrated | Closing summary points to SC0009 governance and a later W0112 successor |
| `agent/sessions/2026/09/S0112-20260901-003-m0110-w0112-post-startup-governance/notes.md` | Active session | Migrated | Closing notes preserve P101 as the unchanged product resume boundary |
| `agent/sessions/2026/09/S0100-20260901-004-host-package-bootstrap/session.json` | Active session | Migrated | Newly scaffolded schema version 2 session becomes the sole SC0009 owner |
| `agent/sessions/2026/09/S0100-20260901-004-host-package-bootstrap/events.jsonl` | Active session | Migrated | Objective records exact escalation, credential, verification, and product-resume boundaries |
| `agent/sessions/2026/09/S0100-20260901-004-host-package-bootstrap/summary.md` | Active session | Migrated | Activation state and bounded migration are explicit |
| `agent/sessions/2026/09/S0100-20260901-004-host-package-bootstrap/notes.md` | Active session | Migrated | Notes distinguish persistent authorization from forbidden password storage |
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
host package path. After a confirmed Nix materialization gap, use only the
canonical package-name validator and root-owned pacman installer. For MetaFlux
driver debugging, use only the enumerated driver helper actions and exact
repository-root paths. Never pass pacman options, arbitrary commands, local
package files, URLs, or shell fragments; never store or print a sudo password.
When the bounded path can complete the requested operation, proceed without an
operator prompt. Otherwise report the exact unresolved package, unsupported
debug action, path violation, or authorization failure.

## Verification

| Gate | Result |
| --- | --- |
| User breaking-change authority | Passed: explicit instruction requires automatic sudo installation for pacman-resolvable Nix gaps and persistent root access for driver debugging |
| Credential boundary | Passed at activation: no password value is recorded in repository files or session records |
| Current owner convergence | Activation candidate closes the unchanged P101 W0112 owner and installs one schema version 2 SC0009 governance owner |
| Active-session handoff | Passed at activation boundary: no other current active owner requires guidance |
| Behavior and residual gates | Migration must still add the canonical decision, both bounded helpers/tests, host authorization, routing checks, record checks, and exact secret-residual checks before application |
