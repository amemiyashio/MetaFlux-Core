---
name: manage-host-privilege
description: Manage MetaFlux host privilege through bounded sudo or su policy, root-owned helpers, non-secret persistent authorization, pacman escalation after confirmed Nix gaps, and enumerated driver debugging. Use whenever repository work needs root privilege or changes privilege setup.
---

# Manage Host Privilege

This skill is the sole workflow owner for `sudo`, `su`, root-owned helper
installation, persistent privilege grants, host package escalation, and
privileged MetaFlux driver debugging. It grants only the privilege needed by an
owning workflow; it does not take over that workflow's commands, semantics, or
evidence.

**When to invoke (decision-0036)**: When a required tool or dependency is
absent from the current environment, do NOT treat it as a task blocker.
Instead, attempt provisioning in this order: (1) add the tool to the repository
Nix declaration; (2) if Nix cannot provide or materialize it, invoke this
skill's package installation path for the exact pacman package. Only declare
inability to proceed after exhausting both provisioning paths.

## Inputs

- Read [decision-0032](../../../docs/architecture/host-privilege-escalation.md) before
  any privileged operation or change to the host authorization boundary.
- Read the active goal, assigned lane, and owning domain skill to identify the
  exact operation that needs privilege.
- For a missing executable, require `manage-toolchain` evidence that Nix cannot
  provide or materialize the tool before selecting a host package.
- For driver work, compose `linux-device-driver-uapi`; that skill owns driver
  behavior and qualification while this skill owns elevation only.

## Ownership And Routing

- `iteration` resolves the harness, enters Nix, and routes a privilege need
  here. It does not define sudo commands, package operations, driver actions,
  credential handling, helper installation, or revocation.
- `manage-toolchain` owns Nix declarations, locks, and the determination that a
  tool has a real Nix provision/materialization gap. This skill owns the
  subsequent host package mapping and privileged installation. The resulting
  host executable remains a local prerequisite, never Nix-owned tool identity
  or release evidence.
- The relevant domain skill owns the requested operation. This skill validates
  and performs only its bounded privilege transition.
- Routine work uses the root-owned helpers through non-interactive `sudo`.
  Direct `sudo`, a generic root shell, arbitrary command forwarding, and
  credential transport are outside the operational path.
- `su` is not a fallback execution channel. On a host that exposes only `su`,
  this skill owns classifying and provisioning a bounded helper arrangement;
  no task may retain an `su` session or persist its credential.

## Workflow

1. State the owning workflow and the exact privileged operation. Reject any
   request that has no bounded operation or asks for a generic root command.
2. Verify the installed non-secret authorization before mutation:

   ```sh
   nix develop . --command python3 \
     agent/skills/manage-host-privilege/scripts/host_privilege.py check
   ```

3. For a tool that `manage-toolchain` has proved unavailable from Nix, map it
   to exact pacman repository package names and invoke only:

   ```sh
   nix develop . --command python3 \
     agent/skills/manage-host-privilege/scripts/host_privilege.py \
     package PACKAGE [PACKAGE ...]
   ```

   The client accepts package names only. It does not accept package-manager
   flags, URLs, local package files, shell syntax, or arbitrary commands. After
   installation, invoke only the intended absolute host executable from inside
   `nix develop . --command ...` and record it as a host prerequisite.
4. For MetaFlux driver work, invoke one decision-0032 action:

   ```sh
   nix develop . --command python3 \
     agent/skills/manage-host-privilege/scripts/host_privilege.py \
     driver ACTION [ARTIFACT]
   ```

   Allowed actions are `check`, `load`, `reload`, `unload`, `logs`,
   `kmemleak-clear`, `kmemleak-scan`, `kmemleak-read`, and `live`. Module and
   live-test artifacts must resolve under the configured repository root and
   have their canonical names. The domain workflow retains build, execution,
   qualification, and evidence ownership.
5. If the persistent helpers are absent or need replacement, provision only
   these root-owned objects after validating their repository sources:

   - `/usr/local/libexec/metaflux-pacman-install`, owned by root and executable;
   - `/usr/local/libexec/metaflux-driver-debug`, owned by root and executable;
   - `/etc/metaflux-driver-debug-root`, owned by root with mode 0600 or 0644;
   - `/etc/sudoers.d/metaflux-host-privilege`, owned by root with mode 0440 and
     validated by `visudo -cf` before activation.

   Initial provisioning may accept a credential once through protected TTY
   input. Never place it in a command argument, environment variable, file,
   repository record, output, or log. Persistent state stores authorization in
   the exact helper and sudoers objects, not a credential. Invalidate the
   interactive sudo timestamp after provisioning.
6. Revoke access by removing the exact sudoers fragment first, then the two
   helper files and repository-root configuration when no governed workflow
   needs them. Treat revocation as a host-state change and verify the bounded
   client no longer succeeds.

## Failure Boundary

Fail closed and report the exact failed prerequisite when Nix absence is not
proved, a package is unresolved, a helper or authorization object is missing,
ownership or mode is invalid, an action is unknown, argument arity is wrong, an
artifact escapes the configured repository root, or a required kernel facility
is absent. Do not widen the command set to bypass a failure.

Render these failures through the `main` task-stop contract. Invalid
package names, actions, arity, and canonical in-repository artifact shape are
`current-agent / fix-and-retry`. An artifact outside the configured repository
is `host-privilege.artifact-outside-repository` with
`user-or-application / preserve-and-report`: request the canonical integrated
artifact and do not copy or reconfigure the boundary. A root-owned helper,
authorization, or kernel prerequisite rejection is
`host-privilege.helper-rejected` with `host-operator / stop-and-report`; retain
its raw output and never fall back to direct sudo, su, credential transport, or
a wider command.

## Verification

```sh
nix develop . --command python3 \
  agent/skills/manage-host-privilege/scripts/test_host_privilege.py
nix develop . --command sh -n \
  agent/skills/manage-host-privilege/scripts/metaflux-pacman-install
nix develop . --command sh -n \
  agent/skills/manage-host-privilege/scripts/metaflux-driver-debug
```

Passing proves the repository boundary is closed over package names, driver
actions, artifact paths, helper commands, and credential absence. A live host
check separately proves that installed root-owned authorization matches it.
