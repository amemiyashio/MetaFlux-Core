# Notes

The authorization object and the credential are separate. The durable target is
a root-owned helper that accepts package names only, a second root-owned helper
that accepts enumerated MetaFlux driver-debug actions only, and narrowly scoped
passwordless sudo rules for those helpers. No password value belongs in Git,
session records, command arguments, environment variables, output, or logs.

The helper is reached only after the repository Nix declaration has been tried
and confirmed unable to provide or materialize the tool. Pacman does not become
a general ambient fallback and neither Nix nor pacman owns the consuming build,
test, packaging, qualification, focus, or evidence workflow.

The driver helper may load the current `metaflux_core.ko`, unload it, inspect
kernel logs and kmemleak state, and run the named live cdev qualification binary
from the exact repository root. It does not expose an arbitrary command or root
shell. Loading a user-built kernel module is inherently a root-trust boundary;
the helper makes that authority explicit and project-scoped.

SC0009 applied at `c4cedfe`. The client, helper sources, sudoers template, and
regression now live only under `agent/skills/manage-host-privilege/`.
`start-work` and `manage-toolchain` route that owner but do not reproduce its
commands or credential policy. The one-time authentication timestamp and exact
temporary installation directory were removed; only non-secret root-owned
authorization objects remain on the host.
