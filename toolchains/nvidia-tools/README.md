# NVIDIA Stock Tool Matrix

`nvidia-tools-1.json` pins the unmodified `nvidia-smi` binaries used by the M0001
NVML compatibility gate. The repository stores only provenance and fingerprints;
vendor packages and extracted binaries are never committed.

Each row is selected from the signed `Packages.gz` snapshot recorded by the
matching NVIDIA header manifest. The Nix output fetches the exact DEB, verifies
its package hash, extracts only `nvidia-smi` and its license, then verifies the
binary size, SHA-256, and GNU build ID. A missing build ID is explicit for R535.

Download routing follows the repository
[toolchain policy](../README.md#artifact-download-routing-d0020). Routing never
changes the frozen package or binary identity and is recorded only by the run
that performed the transfer.

The extraction gate is acquisition evidence only. W05 completes a row after the
stock binary runs every required view against the built MetaFlux NVML provider
and archives stdout, stderr, exit status, provider build identity, and daemon
registry revision.
