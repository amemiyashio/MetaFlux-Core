# RPM Packaging

This directory defines the generic x86_64 provider RPM. The packaging workflow,
not Nix, owns construction of the product package; Nix may only supply its
pinned tools. The package requires glibc 2.31 or newer and installs CUDA,
bounded cuBLAS, and NVML compatibility libraries only below
`/usr/lib/metaflux/providers`; it never replaces a distribution or vendor-owned
library. DKMS artifacts begin in a later milestone.

The complete `metaflux` RPM contains the daemon, frozen target linker, static
non-glibc closure, target provenance, service integration, and providers. It
uses the distribution's glibc 2.31 or newer and does not ship a private loader.
It provides and obsoletes the provider-only package, allowing a
managed upgrade without transferring ownership to system or vendor CUDA/NVML
paths.

Both RPMs own the private MetaFlux include directories as well as their files,
so package removal leaves no empty `/usr/include/metaflux` hierarchy behind.

The complete RPM's pre/post and preun/postun scriptlets provide the same
idempotent account, directory, upgrade, activation, and removal behavior as the
DEB. Their source files, `pre`, `post`, `preun`, and `postun`, live in this
directory; the builder inserts their contents into the matching RPM spec
sections. Sysusers/tmpfiles are preferred when installed; `groupadd`/`useradd` and
explicit modes are the fallback, including on the frozen Rocky Linux 9 minimal
row where no systemd command is present. Erase retains the `metaflux` account,
`/var/lib/metaflux`, and `/var/cache/metaflux` by policy.

On a running systemd host, fresh install applies the socket preset and starts
it. Upgrade records active/enabled state before stopping the old units and
restores only those recorded bits after daemon-reload, preserving an inactive
or disabled administrator choice.

The canonical builder is [`packaging/build.py`](../build.py). It stages the
already-installed CMake tree into an explicit RPM spec and invokes the pinned
`rpmbuild` supplied by `nix develop .#release`.
