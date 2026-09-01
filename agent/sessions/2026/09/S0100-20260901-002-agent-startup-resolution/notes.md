# Notes

The recurring harness failure has two independent causes. D0028 validates only
slug syntax and length, so a long model/template label can masquerade as
workflow provenance. D0022 limits Nix ownership but does not currently make Nix
shell entry precede ambient PATH, CLI discovery, and version probes.

SC0008 removes both ambiguities. The repository stays product-table-free: the
agent reads its stable harness subject from active system/developer runtime
instruction context, and the generic helper rejects non-harness identity
classes by the stricter contract. Git and Nix remain host bootstrap tools; all
repository executables and capability/version probes run inside
`nix develop . --command ...`.

D0031 is Verified at content revision `a7e370d`. A fixed tool version means the
current revision names and reproduces a stable version; it does not prohibit a
later governed `manage-toolchain` manifest or lock update. The exact prior erroneous
model/template subject is now a negative regression fixture. It is retained only
to prove rejection, never as accepted provenance or compatibility input.
