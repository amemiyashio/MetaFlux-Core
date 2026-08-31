# Common Packaging

Shared release metadata, udev/systemd policy templates, install layout, signing
inputs, coexistence rules, and reproducible packaging helpers live here.

`70-metaflux.rules` is the canonical node policy for the existing cdev
transport. It matches only the kernel-owned `misc` nodes `metafluxctl` and
`metaflux[0-9]*`, assigns group `metaflux`, and applies mode `0660`. It does not
create nodes, change their names, or install vendor/NVIDIA aliases; the kernel
and udev remain the authorities for creation and activation.

The shipped socket unit exposes `/run/metaflux/metafluxd.sock` only to members
of the `metaflux` group. The service runs as an unprivileged system user with
write access limited to its runtime, compiler-cache, and state directories.
`vendor-libraries.conf.example` documents the strict all-or-nothing absolute
path override; packages never install CUDA/NVML providers into a vendor-owned
search directory.

The complete package lifecycle prefers `systemd-sysusers` and
`systemd-tmpfiles`. Hosts without those tools use the packaged scripts' shadow
utilities fallback and the same exact directory modes: `0750` for the service
cache and state root, `0700` for the private compiler-cache users tree, and
`0755` for the root-owned AOT directory and runtime socket directory. The
socket itself is owned by
`metaflux:metaflux` with mode `0660`.

Removal drops package payload and stops activation, but keeps the `metaflux`
account plus `/var/lib/metaflux` and `/var/cache/metaflux`. Keeping the account
prevents a later unrelated account from inheriting its UID; keeping the
directories avoids destructive package-manager behavior. Permanent cleanup is
an explicit administrator operation after state is archived or declared
disposable.

Package upgrades write a root-only transient record under `/run` before
stopping the old units. Post-install reloads the unit database and restores only
the socket's recorded active/enabled state. Preset plus initial start is reserved
for a fresh installation; an upgrade never activates a socket that was inactive
or enables one that was disabled.
