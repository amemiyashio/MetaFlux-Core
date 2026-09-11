# Host Privilege Escalation

| Field | Value |
| --- | --- |
| Status | Verified |
| Decision | decision-0032 |
| Classification | Breaking (destructive) governance |
| Applies to | Every repository sudo/su boundary, confirmed Nix gaps, and MetaFlux driver debugging |
| Execution model | decision-0033 |

## Decision

`manage-host-privilege` is the sole workflow owner for repository `sudo`, `su`,
root-helper installation, persistent grants, host package escalation, privileged
driver debugging, and later revocation. Repository agents use its Nix-provided
`host_privilege.py` client to reach two root-owned host helpers:

1. `metaflux-pacman-install` accepts one or more validated Arch package names,
   first asks pacman to resolve them from configured repositories, and then
   performs `--sync --needed --noconfirm`. It accepts no package-manager option,
   URL, local package path, or shell fragment.
2. `metaflux-driver-debug` accepts only `check`, `load`, `reload`, `unload`,
   `logs`, `kmemleak-clear`, `kmemleak-scan`, `kmemleak-read`, and `live`.
   Module actions require a file named `metaflux_core.ko`; `live` requires an
   executable named `metaflux_transport_cdev_live_qualification`. Both must
   resolve under the repository root recorded in the root-owned host
   configuration.

The canonical client, helper, sudoers-template, and regression sources live in
[$manage-host-privilege](../../agent/skills/manage-host-privilege/SKILL.md) skill.
[$main](../../agent/skills/main/SKILL.md) skill only routes a privilege need
after Nix-first entry; `manage-toolchain` only establishes a Nix gap; the owning
domain skill retains operation semantics and evidence.

The client invokes only the fixed `/usr/local/libexec` helper paths through
`sudo --non-interactive`. A root-owned sudoers fragment grants the current host
user passwordless access to those helpers and no generic command. Normal task
execution never reads, prompts for, transports, stores, prints, or logs a sudo
password.

## Acquisition Order

Nix remains first. A newly required repeatable tool is added to the narrow
repository declaration and materialization is attempted before any host package
resolution. Only a confirmed Nix provision/materialization gap permits the
package helper. The agent maps the required executable to one exact repository
package name, installs it without an operator interruption, and invokes the
intended absolute host executable from inside
`nix develop . --ignore-environment --keep HOME --keep USER --command ...`.

The installed host copy is an explicit local prerequisite. It does not become a
Nix declaration, a reproducibly fixed tool identity, a generic ambient fallback,
or release/qualification evidence. If repeatable evidence later depends on the
tool, `manage-toolchain` still owns adding a portable declaration or recording
the remaining host-only boundary.

## Driver Debug Boundary

Driver privilege does not require a synthetic Nix failure: module lifecycle and
kernel debug facilities are inherently host-managed. The focused kernel
workflow may invoke the enumerated helper actions after building through Kbuild
and selecting exact artifacts. The helper clears the environment before running
the live qualification binary and passes no user-controlled command options.

Loading a user-built kernel module and running a repository-built live binary as
root are intentionally high-trust actions. decision-0032 bounds that trust to the exact
repository root, canonical artifact names, and fixed actions. It does not expose
an arbitrary root shell, generic executable runner, module parameter channel, or
mutable sudoers command.

The helper supplies privilege only. Kbuild owns module construction; the cdev
test owns its behavior and result; kernel qualification owns KUnit, sanitizer,
lockdep, fault, and kmemleak semantics; Git and the owning test harness retain
accepted evidence.

## Credential And Host State

Persistent operation uses root-owned helper files, one root-owned repository-
root configuration, and a mode-checked sudoers fragment. Those objects persist
authorization, not a credential. A credential supplied for initial provisioning
may enter sudo only through its protected terminal input and is discarded after
authentication. It never enters Git, arguments, environment variables, files,
command output, Agent records, or logs.

`manage-host-privilege` owns installation, ownership/mode, sudoers validation,
and later revocation of these host objects. The repository owns the auditable
helper sources and client behavior. Nix remains strictly limited to tool
identity, materialization, and exposure; it does not install or configure host
privilege.

Routine execution does not use `su` or retain a root shell. If a host exposes
only `su`, this skill may use that channel solely to provision the same bounded
helper arrangement through protected terminal input; no credential or elevated
session persists afterward.

## Failure And Compatibility

The package path fails closed when Nix absence is unconfirmed, a package name is
invalid, pacman cannot resolve it, or the root helper is unavailable. The driver
path fails closed for unknown actions, wrong arity, artifacts outside the
configured root, wrong names, missing facilities, or unavailable authorization.
The agent reports that exact blocker.

This is a destructive replacement of decision-0031's confirmed-gap stop-and-report
branch and repeated interactive driver-debug elevation. When a bounded helper
can perform the exact operation, future work proceeds through it without an
operator installation or password prompt. There is no arbitrary-sudo or
plaintext-credential compatibility route.

## Verification State

- The host-privilege suite covers exact sudo commands, package-name rejection,
  repository path confinement, canonical artifact names, action arity, closed
  helper command sets, and absence of credential transport or generic sudoers
  grants.
- The current host verifies both root-owned helpers through non-interactive sudo
  without installing an unnecessary package or executing a driver mutation.
- The Agent state checker, iteration identity regression, routing corpus, and
  repository pre-commit gate cover the synchronized workflow surfaces.
