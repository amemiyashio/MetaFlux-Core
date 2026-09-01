---
id: P20260901-102
status: Recorded
captured: 2026-09-01
milestone: M0110
workstream: W0112
branch: main
git_revision: c4cedfe2b87be9a8ed1c8022006815cfb16fb8c7
workspace: applied destructive D0032 host privilege governance
---

# Applied Destructive Host Privilege Governance

## Outcome

SC0009 applies D0032 at revision `c4cedfe`. `manage-host-privilege` is the sole
workflow owner for repository sudo/su, root helpers, persistent non-secret
authorization, post-Nix host package escalation, privileged MetaFlux driver
debugging, and revocation. `start-work` retains Nix-first startup and only
routes privilege needs; `manage-toolchain` owns Nix declarations and gap proof;
domain skills retain operation semantics and evidence.

The package helper accepts exact pacman repository package names only. The
driver helper accepts only enumerated module, log, kmemleak, and named live-test
actions under the configured repository root. Persistent host state contains
root-owned helper/configuration/sudoers objects, never a credential or generic
root command. There is no compatibility route through the previous start-work
implementation layout or repeated interactive elevation.

Product focus transfers to
`S0112-20260901-005-m0110-w0112-post-privilege-governance`; the W0112 Exit Gate
is unchanged and remains open.

## Verification evidence

| Gate | Result |
| --- | --- |
| Skill packages | Passed for manage-host-privilege, start-work, and manage-toolchain |
| Privilege and startup regressions | Passed 7/7 and 9/9; both root-helper shell syntax checks passed |
| Persistent authorization | Passed non-interactive checks after sudo timestamp invalidation; no package or driver mutation |
| Installed helper identity | Both root-owned helper hashes match revision `c4cedfe` sources |
| Skill routing | Passed 89-case corpus and 34/34 self-tests |
| Agent records | Passed current validation and 197 self-test cases |
| Semantic-change governance | Passed current/cached gates and 23/23 self-tests |
| Credential boundary | No credential persisted or emitted; exact temporary installation directory removed |
| Content revisions | Repository pre-commit passed at `65fb7ce`, `4558103`, and `c4cedfe` |

## Cleanup

- Removed: the exact session temporary installation directory and interactive
  sudo timestamp.
- Retained: bounded root-owned host authorization, current D0032/SC0009
  authority, P101 product evidence, and the current W0112 work boundary.

## roast

### light roasts

- none.

### medium roasts

- none.

### dark roasts

- Independent bounded host-privilege ownership ->
  `docs/architecture/host-privilege-escalation.md` (`c4cedfe`; skill validators,
  privilege 7/7, startup 9/9, host helper check/hash match, routing and record
  gates; authority: D0032, SC0009)

## session-only

- none.

## Handoff

Continue as `S0112-20260901-005-m0110-w0112-post-privilege-governance`. Read the
current focus, D0032, Applied SC0009, `$manage-host-privilege`, this checkpoint,
and the W0112 Exit Gate before resuming live device qualification.
