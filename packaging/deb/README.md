# Debian Packaging

This directory defines the generic amd64 provider DEB. The packaging workflow,
not Nix, owns construction of the product package; Nix may only supply its
pinned tools. The package requires glibc 2.31 or newer and installs CUDA,
bounded cuBLAS, and NVML compatibility libraries only below
`/usr/lib/metaflux/providers`; it never replaces a distribution or vendor-owned
library. DKMS artifacts begin in a later milestone.

The complete `metaflux` DEB contains the daemon, frozen target linker, static
non-glibc closure, target provenance, service integration, and providers. It
uses the distribution's glibc 2.31 or newer and does not ship a private loader.
It provides, conflicts with, and replaces `metaflux-provider`, so a
package-manager upgrade from the provider-only artifact has one owner for every
installed path. Vendor CUDA/NVML libraries remain untouched.

The complete DEB ships idempotent `preinst`, `postinst`, `prerm`, and `postrm`
hooks stored as files in this directory. The builder copies their contents into
`DEBIAN/` with executable mode `0755`. `postinst` creates the system account
and strict state/cache directories, preferring sysusers/tmpfiles and falling
back to Debian's shadow utilities.
When systemd is actually running, the hooks stop the old units for upgrade or
removal and reload the unit database. A fresh install applies the preset and
starts the socket through `deb-systemd-invoke` when available. An upgrade
records active/enabled state before stopping and restores only the recorded
state, using `deb-systemd-helper` when available; it does not activate a
previously inactive or disabled socket. The hooks do not require systemd on
minimal images. Both remove and purge retain the service account,
`/var/lib/metaflux`, and `/var/cache/metaflux` by policy.

The canonical builder is [`packaging/build.py`](../build.py). It invokes
`cmake --install --component Provider` for this package family and then calls
`dpkg-deb` with `SOURCE_DATE_EPOCH`; no Debian build system is duplicated here.
