# Notes

The W0112 product boundary is unchanged by SC0008. P100 provides the current
source and fixture resume point, while live `/dev/metafluxctl` and
`/dev/metafluxN` evidence remains open.

D0031 controls startup for this successor. Resolve `codex` directly from the
system/developer harness context, use Git and Nix only as host bootstrap, and
run repository executables through `nix develop . --command ...`. Add missing
tools to the governed Nix declaration first. Nix owns only clear, reproducible,
stable tool-version identity, materialization, and exposure; a later
`manage-toolchain` revision may deliberately evolve versions.

No W0112 content changed after P101. SC0009 now owns the requested replacement;
this terminal session is not an execution or compatibility fallback.
