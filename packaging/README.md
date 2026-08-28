# Packaging

Packaging owns NixOS integration and generic Linux release artifacts such as
`.deb`, `.rpm`, and relocatable tar archives. Nix definitions themselves live in
the repository's top-level `nix/` tree.

Planned ownership is `common/` for shared release metadata, `nixos/` for NixOS
modules and exact-kernel integration, `dkms/` for generic external-module source,
and `deb/`, `rpm/`, and `tar/` for their respective generic artifacts. These
directories contain packaging integration rather than alternate build systems.

Native Nix packages may reference `/nix/store`; generic artifacts use the
release sysroot and must contain no required store interpreter, RPATH, or runtime
closure. Package-manager-owned vendor libraries and device nodes are never
overwritten.
