# NVIDIA Header Manifests

Compiler epoch 1 carries one immutable CUDA Driver/NVML header manifest for each
stock-tool family required by M0001-W05. The headers are ABI source inputs only;
they do not prove that a stock `nvidia-smi` binary ran successfully.

## Update Procedure

1. Select an exact NVIDIA driver family, representative driver build, toolkit
   release, Ubuntu repository, architecture, package version, and filename.
2. Download the repository `InRelease`, verify it with the checked NVIDIA CUDA
   repository key fingerprint, and archive its acquisition date and SHA256.
3. Download `Packages.gz`, record its SHA256, and require the selected stanza's
   package filename/version/architecture/SHA256 to match the manifest.
4. Download each immutable package URL and verify the package SHA256 before
   extraction.
5. Extract only the named `cuda.h`, `cudaTypedefs.h`, `nvml.h`, and license
   records with `dpkg-deb --fsys-tarfile`; verify every extracted SHA256.
6. Generate C and C++ compile/layout checks plus symbol, alias, enum, and
   structure diffs for all five manifests. Archive the generator version and
   clean-build output digest.
7. Update an existing family only by an explicit manifest-epoch bump. A moving
   repository index or `latest` package selector is never an ABI input.

Release qualification separately records each real stock tool's binary SHA256,
ELF build ID, driver build, command line, and output for `-L`, summary, CSV,
`compute-apps`, and the selected `-q/-x` views.
