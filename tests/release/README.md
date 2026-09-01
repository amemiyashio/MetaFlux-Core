# Release package matrices

Two offline harnesses qualify the generic artifacts against the four frozen
decision-0012 distribution rows. Images must already exist locally and are
required to use immutable `@sha256:` references. Tag-only images are rejected.
Every container runs with `--pull=never` and
`--network=none`, so image acquisition and its mirror provenance remain a
separate recorded step.

Before installing a package, each immutable image is also probed with a
read-only root filesystem and no network. The report records the host kernel,
the SHA-256 of every embedded APT or RPM repository configuration file, the
base package-list SHA-256 and package count, and an empty qualification-update
list. System updates are not applied during this offline matrix.

Run each repository-owned harness as the command inside the release development
shell. Nix supplies the pinned host Python, Podman, and RPM tools; the Python
harness remains the release workflow owner.

## Provider-only matrix

`run_provider_package_matrix.py` qualifies the generic provider `.deb`, `.rpm`,
and `.tar.gz` payloads.

The matrix verifies the exact distribution/glibc row, fresh install, a real
`0.0.0 -> 0.1.0` package-manager upgrade, removal, private provider paths,
dynamic loading, absence of Nix store strings, and unchanged sentinel vendor
libraries. The tar row exercises explicit extraction/overlay/removal because a
tar archive has no package-manager database. The host harness validates and
extracts the archive with Python's traversal-safe data filter before mounting
the payload read-only, so the Rocky minimal image needs no downloaded test tool.
Results, image identities, input
hashes, harness/Python fingerprints, commands, stdout, and stderr are written to
`provider-package-matrix.json`.

Example:

```sh
nix develop .#release --command python3 tests/release/run_provider_package_matrix.py \
  --deb /path/to/metaflux-provider_0.1.0_amd64.deb \
  --rpm /path/to/metaflux-provider-0.1.0-1.x86_64.rpm \
  --tar /path/to/metaflux-provider-0.1.0-x86_64.tar.gz \
  --ubuntu-20-image REGISTRY/ubuntu@sha256:DIGEST \
  --ubuntu-22-image REGISTRY/ubuntu@sha256:DIGEST \
  --ubuntu-24-image REGISTRY/ubuntu@sha256:DIGEST \
  --rocky-9-image REGISTRY/rockylinux@sha256:DIGEST \
  --output-dir ../.metaflux-evidence/MetaFlux-Core/milestone-0.1.0.0-package-matrix
```

## Complete release matrix

`run_release_package_matrix.py` qualifies the complete `metaflux` `.deb`,
`.rpm`, and `.tar.gz` payloads. In addition to the provider checks, it verifies
the daemon and frozen target LLD 22.1.8 executable against the system glibc
2.31 floor. LLVM, MLIR, libstdc++, libgcc, and zlib are linked statically; the
payload does not ship a private glibc runtime. Recursive checks reject Nix store
strings, non-system `DT_NEEDED` entries, private or post-2.31 glibc symbols,
non-system interpreters, and RPATH/RUNPATH entries in every payload ELF. Stable
target SDK, toolchain, compiler epoch, and fingerprint provenance remains in
the installed manifests.

Every installed and upgraded payload also starts the packaged daemon and runs
the glibc-2.31-floor CUDA Add/Copy acceptance binary through the packaged
provider. Its unique shutdown metrics record must prove one host-address-space
registration, nonzero direct source and destination operations and bytes, and
zero staged source and destination operations and bytes. Both Ubuntu 20.04
package rows additionally measure interpreter, cold-JIT, warm-JIT, and AOT
modes, including one cold cache artifact, a warm cache hit, idempotent AOT
prewarm, and execution from the read-only AOT tree.

The acceptance output directory also contains `metaflux-activation-launcher`,
a glibc-2.31-floor C fixture, and `metaflux-add-u32.ptx`, the exact AOT prewarm
input. Before creating any container, the matrix validates both ELF fixtures
with the release-shell `readelf`: they must use the system loader, contain no
RPATH/RUNPATH or Nix store path, stay within the system dependency closure, and
reference no glibc symbol newer than `GLIBC_2.31`. The matrix locates both next
to the path supplied by `--cuda-acceptance`, copies all three artifacts, and
uses the launcher as root to bind a real `SOCK_SEQPACKET` listener at fd 3,
set `LISTEN_PID`/`LISTEN_FDS`, set socket ownership and mode, drop to the
packaged `metaflux` UID/GID, and exec `metafluxd`. CUDA Add/Copy itself runs as
that account. The matrix does not treat the bound path as readiness: it waits
until `/proc` shows the packaged daemon executable, all real/effective/saved/fs
UID/GID values equal the service account, and fd 3 is a socket. It then checks
strict runtime/cache/state directory modes, socket mode/ownership, and
preservation of the account and state/cache markers after package removal.

The managed-package rows exercise fresh installation, removal, and a real
`0.0.0 -> 0.1.0` package-manager upgrade. The tar rows exercise extraction,
overlay upgrade, and removal against an explicit target root. Results are
written to `release-package-matrix.json`.

Digest-pinned references are syntax checked and their requested digest must
match the digest returned by the local image inspection before any container is
started.

```sh
nix develop .#release --command python3 tests/release/run_release_package_matrix.py \
  --deb /path/to/metaflux_0.1.0_amd64.deb \
  --rpm /path/to/metaflux-0.1.0-1.x86_64.rpm \
  --tar /path/to/metaflux-0.1.0-x86_64.tar.gz \
  --cuda-acceptance /path/to/metaflux-cuda-add-copy \
  --ubuntu-20-image REGISTRY/ubuntu@sha256:DIGEST \
  --ubuntu-22-image REGISTRY/ubuntu@sha256:DIGEST \
  --ubuntu-24-image REGISTRY/ubuntu@sha256:DIGEST \
  --rocky-9-image REGISTRY/rockylinux@sha256:DIGEST \
  --output-dir ../.metaflux-evidence/MetaFlux-Core/milestone-0.1.0.0-release-package-matrix
```

`metaflux-activation-launcher` and `metaflux-add-u32.ptx` must be in the same
directory as the `metaflux-cuda-add-copy` path passed above.
