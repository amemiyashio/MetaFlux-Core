# Tar Packaging

This directory defines the relocatable x86_64 provider archive. The packaging
workflow, not Nix, owns construction of the product archive; Nix may only supply
its pinned tools. Its paths are rooted at `usr/`, so an installer selects the
destination root explicitly. The artifact uses the release sysroot and contains
no required `/nix/store` interpreter, RPATH, or runtime dependency.

The complete x86_64 runtime archive includes `metafluxd`, target LLD 22.1.8,
providers, and stable target SDK/toolchain provenance. LLVM, MLIR, libstdc++,
libgcc, and zlib are linked statically; the ELF interpreter is the target
distribution's `/lib64/ld-linux-x86-64.so.2`. The archive is target-root
relative and requires glibc 2.31 or newer at that root.

A tar extraction is not an installation transaction. Its installer must apply
`usr/lib/sysusers.d/metaflux.conf` and `usr/lib/tmpfiles.d/metaflux.conf`; when
systemd tooling is absent, it must create the equivalent system account and
directories with the documented ownership and modes before starting the
daemon. The offline release matrix exercises this explicit fallback, starts the
daemon through a real fd-3 `SOCK_SEQPACKET` activation listener, and then removes
only archive-owned payload. Account, state, and cache cleanup remains an
explicit administrator action.
