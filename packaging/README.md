# Packaging

Packaging owns NixOS integration and generic Linux release artifacts such as
`.deb`, `.rpm`, and relocatable tar archives. Product packaging definitions
live under this tree. The repository's top-level `nix/` tree only materializes
fixed tool versions and does not construct product packages.

Planned ownership is `common/` for shared release metadata, `nixos/` for NixOS
modules and exact-kernel integration, `dkms/` for generic external-module source,
and `deb/`, `rpm/`, and `tar/` for their respective generic artifacts. These
directories contain packaging integration rather than alternate build systems.

Generic artifacts use the Ubuntu 20.04 target SDK and the system glibc 2.31 ABI
floor. Their LLVM, MLIR, libstdc++, libgcc, and zlib dependencies are static,
and they must contain no Nix store path, private glibc runtime, RPATH, or
non-system dynamic dependency. Package-manager-owned vendor libraries and
device nodes are never overwritten.

Packaging is responsible for two generic artifact families:

- `generic-provider-{tar,deb,rpm}` packages the CUDA Driver and NVML providers,
  passthrough SDK, and public build manifest as `metaflux-provider`.
- `generic-release-{tar,deb,rpm}` packages the complete CPU-backed runtime as
  `metaflux`: provider payload, daemon, frozen target LLD, stable target
  toolchain manifests, and service integration. The target distribution's
  `/lib64/ld-linux-x86-64.so.2` and glibc provide the dynamic runtime.

The complete DEB provides, conflicts with, and replaces
`metaflux-provider`; the RPM provides and obsoletes its provider-only
predecessor. Neither family installs `libcuda.so` or `libnvidia-ml.so` into a
system or vendor library directory.

Release-artifact gates independently extract all three formats, compare their
payloads, verify package metadata, and repeat the store-path and
dynamic-dependency checks. Distribution install, real upgrade, removal,
vendor-coexistence, and packaged CUDA Add/Copy runs are owned by the offline
release matrix harnesses under `tests/release/`, not by flake checks.

Complete DEB and RPM packages apply `metaflux` sysusers and tmpfiles metadata,
with `useradd`/`groupadd` and explicit directory creation as the minimal-image
fallback. On a running systemd host their lifecycle hooks stop units for an
upgrade or removal and reload units. Fresh installation applies the socket
preset and starts activation; upgrade records the prior active/enabled state in
`/run` and restores only those bits, so an inactive or disabled socket stays
that way. Package removal deliberately retains the system account,
`/var/lib/metaflux`, and `/var/cache/metaflux`; this prevents service-UID reuse
and preserves compiler state. An administrator may archive and remove those
three retained resources explicitly when permanent data deletion is intended.
The Daemon component also installs the shared `70-metaflux.rules` policy under
`/usr/lib/udev/rules.d`; packages do not create device nodes or vendor aliases.

## Build entry point

[`build.py`](build.py) consumes an already configured and built CMake tree. It
does not configure or compile the product. The release shell provides the
`cmake`, `dpkg-deb`, and `rpmbuild` tools; the package script owns staging and
artifact construction:

```sh
nix develop .#release --command python3 packaging/build.py \
  --build-dir /path/to/milestone-0.1.0.0-generic-release \
  --output-dir /path/to/release-artifacts \
  --kind complete \
  --target-sdk /nix/store/...-metaflux-ubuntu-20.04-target-sdk \
  --generic-toolchain /nix/store/...-metaflux-generic-llvm-toolchain-22.1.8
```

The default emits one DEB, RPM, and deterministic gzip-compressed tar archive.
Use repeated `--format deb`, `--format rpm`, or `--format tar` for a subset.
The provider-only package uses `--kind provider` and does not require the
generic target inputs. Set `SOURCE_DATE_EPOCH` or `--source-date-epoch` to
rebuild byte-identical artifacts.
