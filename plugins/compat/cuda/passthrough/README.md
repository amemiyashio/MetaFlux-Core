# CUDA/NVML Passthrough Helper

This C17 component implements the decision-0013 vendor-library discovery and mode
selection mechanism. It is a private provider dependency; it does not export
CUDA or NVML entry points and it does not choose a mode in a constructor.

## Modes

`mf_cuda_runtime_mode_parse_v1` accepts exactly `managed`, `passthrough`, or
`auto`. `managed` invokes only the managed callbacks. `passthrough` loads only
the vendor pair. `auto` first attempts the managed transaction and transfers to
the vendor pair only after rollback succeeds and `is_pristine` proves that no
managed state remains. A failed rollback or a dirty result returns
`MF_CUDA_PASSTHROUGH_PARTIAL_STATE`; the vendor stack is not entered.

The production entry points use fixed policy. Test policy injection is declared
only by `src/passthrough_internal.h` and is not installed.

Providers compile the install-root recursion guard from
`METAFLUX_RUNTIME_INSTALL_ROOT`. An empty cache value uses
`CMAKE_INSTALL_PREFIX`; packaging a relocatable `/usr` provider root sets the
value explicitly to `/usr`. The resolved value must be absolute.

## Discovery Policy

An optional `/etc/metaflux/vendor-libraries.conf` must contain exactly two
newline-delimited absolute paths:

```text
cuda=/absolute/path/libcuda.so.DRIVER_BUILD
nvml=/absolute/path/libnvidia-ml.so.DRIVER_BUILD
```

Without the file, the only candidate pairs are:

- `/usr/lib/x86_64-linux-gnu/libcuda.so.1` and
  `/usr/lib/x86_64-linux-gnu/libnvidia-ml.so.1`
- `/usr/lib64/libcuda.so.1` and `/usr/lib64/libnvidia-ml.so.1`

Production requires UID 0 ownership and rejects group/world-writable files or
ancestors, non-regular files, paths under the MetaFlux install root, and the
provider itself by inode or GNU Build-ID. Candidate files must be ELF64,
little-endian, x86-64 `ET_DYN` objects with the exact `libcuda.so.1` or
`libnvidia-ml.so.1` SONAME and a bounded GNU Build-ID. Their canonical filename
builds must be equal and must match the build parsed from
`/proc/driver/nvidia/version`.

Discovery never consults the working directory, loader environment variables,
RUNPATH, a bare library name, a shell, or `ldconfig` output.

## Loading And Lifetime

The CUDA object starts a new glibc link-map namespace with
`dlmopen(LM_ID_NEWLM, ..., RTLD_NOW | RTLD_LOCAL)`. NVML is loaded into that
same namespace. The helper validates the required versioned bootstrap symbols
before publishing the opaque pair:

- `cuInit`, `cuDriverGetVersion`, and `cuGetProcAddress` at `libcuda.so.1`
- `nvmlInit_v2`, `nvmlShutdown`, and `nvmlSystemGetDriverVersion` at
  `libnvidia-ml.so.1`

A successful load transfers one pair to the caller. Release it exactly once
with `mf_cuda_passthrough_pair_release_v1`; release closes NVML before CUDA and
frees all snapshots and descriptors. A pair belongs to the creating process.
Validation, including every dynamic symbol lookup, returns
`MF_CUDA_PASSTHROUGH_STALE` after a fork or after the PID namespace, mount
namespace, config, driver version, or either library path fingerprint changes.

## Qualification Scope

`metaflux.unit.cuda-passthrough` builds versioned fixture DSOs and covers strict
mode/config parsing, both distribution locations through injected paths,
same-namespace local loading, bootstrap lookup, wrong ownership/mode, provider
recursion, install-root rejection, malformed and wrong-architecture ELF input,
SONAME/build mismatches, symlink replacement, fingerprint invalidation, repeated
load failure cleanup, and managed-to-vendor transfer. Release qualification
also runs `metaflux.integration.provider.cuda-nvml-mode` through the real
provider DSO entries. It covers explicit modes, default-auto managed success,
clean fail-open, dirty rollback refusal, all manifest symbol lookups, PTDS/PTSZ
and versioned aliases, CUDA/NVML identity parity, fork/stale rejection, vendor
call counts, and NVML final teardown/reload. Co-loading the physical CUDA/NVML
pair and measuring the work-item-0.1.0.6 performance and coexistence gates remain host
qualification work on supported NVIDIA driver systems.
