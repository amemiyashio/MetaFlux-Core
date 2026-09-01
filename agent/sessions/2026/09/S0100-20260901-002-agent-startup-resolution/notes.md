# Notes

The recurring harness failure has two independent causes. D0028 validates only
slug syntax and length, so a long model/template label can masquerade as
workflow provenance. D0022 limits Nix ownership but does not currently make Nix
shell entry precede ambient PATH, CLI discovery, and version probes.

SC0008 removes both ambiguities. The repository stays product-table-free: the
agent reads its stable harness subject from active runtime instruction context,
and the generic helper rejects non-harness identity classes by the stricter
contract. Git and Nix remain host bootstrap tools; all repository executables
and capability/version probes run inside `nix develop . --command ...`.
