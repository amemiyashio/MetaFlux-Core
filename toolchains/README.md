# Toolchains

This directory owns the declared tool versions and immutable external inputs
used to develop and qualify MetaFlux. Milestones consume these declarations;
they do not redefine them.

## Ownership

| Concern | Owner |
| --- | --- |
| Project source identity and history | Git commits and trees |
| Tool versions, upstream identities, patches, and input hashes | `toolchains/` manifests and `flake.lock` |
| Materializing and exposing the declared tools | Nix |
| Configure and build graph | CMake and Ninja |
| Test selection and execution | CTest, component tests, and `tests/` harnesses |
| Release artifact construction and installation policy | `packaging/` |
| Qualification output | Owning test harnesses |
| Durable work evidence and cleanup accounting | `agent/sessions/` |
| Build-directory retention and cleanup execution | The invoking build or test tool |
| Nix store retention and garbage collection | The host Nix installation and its operator |

## Tool Provider Boundary (D0022)

Nix fixes and provides tool versions. It may resolve locked inputs, build a tool
closure, and expose that closure through a development shell or tool output. It
does not own MetaFlux source identity, the CMake build graph, CTest policy,
release packaging, qualification semantics, evidence retention, build-directory
cleanup, or Nix store garbage-collection policy.

A repeatable workflow names the tools it requires, and those tool versions are
added to the Nix-provided environment before the workflow relies on them. This
keeps ambient host installations out of recorded evidence without transferring
the workflow to Nix. D0022 supersedes D0021, whose broader "owning Nix
environment" wording blurred tool declaration with build, test, packaging, and
qualification ownership.

Under D0031, command resolution is Nix-first. Development enters the Git-aware
flake with `nix develop . --command ...` before any repository executable or
tool/version/capability probe. This includes shell inspection utilities,
Python/repository scripts, compilers, CMake, Ninja, CTest, packaging, and
qualification tools. Host Git and Nix are the only executable bootstrap
exceptions; repository file APIs may read tracked text directly. Agents do not
probe ambient PATH or search for an agent CLI first.

The raw-path flake form is not a project workflow because it treats the working
directory, including ignored build output, as a Nix source input. Git remains
the source snapshot authority. Qualification records a clean Git commit and
tree identity rather than a Nix source-store path or a second per-file source
snapshot. Nix-first resolution does not transfer CMake, CTest, packaging, or
qualification semantics to Nix.

A newly required repeatable tool is added to the narrow repository Nix
declaration before it is used. If Nix cannot provide or materialize that tool,
the workflow stops and reports the exact host installation prerequisite to the
operator. It does not silently consume an ambient executable or invoke a host
package manager without separate user authorization.

## Compiler Epoch 1 (D0018)

[`compiler-epoch-1.json`](compiler-epoch-1.json) is the machine-readable
compiler tool declaration. Compiler epoch 1 selects Clang, MLIR, and LLD 22.1.8
and the LLVM 22.1.8 libraries and tools with the single checked
[`LoopAccessAnalysis` correctness backport](patches/llvm-22.1.8/0001-laa-reject-same-invariant-address.patch).
The authoritative reconstruction inputs are the LLVM version and source
revision, the locked Nixpkgs input, the patch path and SHA-256, required targets,
and declared build flags.

Derivation paths, output store paths, NAR hashes, and upstream test totals are
qualification observations and therefore stay in the owning historical record,
not this descriptor. They do not create a source identity, retention rule,
garbage-collection root, or requirement to preserve a particular local store
path. The reproducer under [`reproducers/llvm-186922/`](reproducers/llvm-186922/)
remains the correctness gate for the selected patch.

Project PGO profiles are release optimization inputs, not compiler-tool
versions. Their generation, selection, evidence, and retention belong to the
performance and packaging workflows that consume them.

## Generic Linux ABI Floor (D0009)

Ubuntu 20.04 and glibc 2.31 are the mandatory minimum userspace compatibility
target for generic Linux artifacts. The fixed Ubuntu 20.04 target SDK supplies
the headers, startup objects, system libraries, loader contract, and GCC layout
used by the pinned unwrapped Clang and target LLD. A host distribution or a
development-shell compiler wrapper is never an ABI input for these artifacts.

The same target tuple applies to every executable used to make a generic
release claim, including package payloads, activation launchers, and acceptance
fixtures. The owning build or qualification workflow must reject a
`GLIBC_2.32` or newer reference, a non-system interpreter, an injected Nix store
path, or a host-only helper. Running the complete package and its fixtures in
the frozen Ubuntu 20.04 row is required evidence that the declared floor is
real; tool materialization alone is not that evidence.

## CUDA/NVML ABI Inputs (D0016)

[`nvidia-headers-1.json`](nvidia-headers-1.json) indexes the exact R535, R550,
R570, R580, and R610 CUDA Driver and NVML header inputs. The family manifests
under [`nvidia-headers/`](nvidia-headers/) pin signed repository metadata,
package identity and digest, extracted header digests, and license records.
[`nvidia-tools-1.json`](nvidia-tools-1.json) similarly pins the unmodified
`nvidia-smi` fixtures used by compatibility tests.

These manifests establish input identity only. Header compilation belongs to
the owning build/tests, and running stock tools against MetaFlux belongs to the
compatibility harness. Changing an existing family requires a new manifest
epoch rather than editing the meaning of an existing one.

## PyTorch CUDA Client Epoch 1

[`pytorch-cuda-clients-1.json`](pytorch-cuda-clients-1.json) declares two
isolated, on-demand client environments for compatibility probing. The
`baseline` profile fixes CPython 3.13.15, PyTorch `2.11.0+cu126`, the
torch-visible CUDA identity `12.6`, and the exact `cuda-toolkit` 12.6.3 wheel
closure. The `frontier` profile fixes the same Python, PyTorch `2.13.0+cu132`,
the torch-visible CUDA identity `13.2`, and the exact `cuda-toolkit` 13.2.1
wheel closure. Their locks under [`pytorch-cuda-clients/`](pytorch-cuda-clients/)
pin every transitively required wheel by filename, byte size, and SHA-256. Each
lock also carries mirror-first candidate URLs ordered for the configured timezone
where that lock was generated or refreshed, with canonical upstream last. A
different execution timezone may reorder only those transport candidates while
preserving the filenames, sizes, and hashes that define profile identity.

Nix exposes these declarations only as `pytorch-baseline` and
`pytorch-frontier` tool packages and development shells. They are absent from
the default, provider, runtime, and release shells, so normal development does
not fetch their multi-gigabyte closures. Enter one explicitly when its owning
probe requires it:

```sh
nix develop .#pytorch-baseline
nix develop .#pytorch-frontier
nix shell .#pytorch-baseline --command python -c 'import torch; print(torch.__version__)'
```

These clients do not define product capability, framework qualification, test
selection, or release acceptance. The baseline is a current gap/regression
probe and the frontier is a future-target probe; success in either environment
does not change the PTX capability manifest or the CUDA-visible virtual compute
capability. The static lock gate is:

```sh
python3 toolchains/tests/verify_pytorch_cuda_clients.py
```

## Vulkan Compute Tool Epoch 1

[`vulkan-1.json`](vulkan-1.json) fixes the Vulkan 1.3 compute tool set used by
the M0130 capability and target-environment probe: Vulkan headers, loader,
`vulkaninfo`, `glslangValidator`, and `spirv-val` are taken from the pinned
nixpkgs input at the versions named by the manifest. They are exposed only by
the on-demand `vulkan-tools` package and `.#vulkan` shell; the default,
provider, runtime, release, and PyTorch shells do not fetch this closure.

Enter the shell when running the Vulkan capability slice:

```sh
nix develop .#vulkan
nix shell .#vulkan-tools --command vulkaninfo --summary
```

The package provides tools and headers only. Vulkan device selection,
capability truth, target-environment serialization, CMake configuration, build,
tests, and qualification remain owned by M0130/CMake/CTest. A host probe is
local evidence for the detected driver and does not satisfy the dual-driver
qualification gate.

## Vulkan Runtime Smoke Profile

[`vulkan-runtime-1.json`](vulkan-runtime-1.json) pins the Mesa Vulkan ICD and
Khronos validation layers used for an on-demand host smoke/probe environment.
Nix exposes the profile only as `vulkan-runtime` and `.#vulkan-runtime`; it is
separate from the lean `vulkan-tools` output so ordinary Vulkan builds do not
fetch the software ICD closure. The shell adds the runtime library, ICD, and
layer search paths but does not force a particular device; callers may select a
specific ICD with the Vulkan loader's normal environment variables.

This profile proves provisioning and can enable local RADV or lavapipe smoke
tests. Mesa is not a physical NVIDIA reference, and a single host smoke run
does not satisfy M0130's two-driver-family, performance, or release gates.

## Artifact Download Routing (D0020)

Downloads first try a mirror in the current execution environment's configured
timezone, then a mirror in the nearest adjacent timezone, and finally the
canonical upstream endpoint. A mirror is eligible only when it serves the exact
frozen bytes or an upstream-signed Nix store object. No country or region has a
permanent priority independent of the configured timezone.

Canonical upstream URLs, immutable revisions, package and extracted-file
digests, NAR hashes where applicable, and trusted signatures define identity.
The selected mirror and timezone are per-run evidence, not persistent toolchain
identity. A missing, stale, challenged, or mismatched mirror never changes a
frozen hash.

## Inventory and Updates

| Path | Role |
| --- | --- |
| `compiler-epoch-1.json` | Compiler version, source, patch, and target declaration |
| `patches/` | Checked downstream tool patches named by a manifest |
| `reproducers/` | Focused correctness reproducers for declared patches |
| `nvidia-headers-1.json`, `nvidia-headers/` | CUDA Driver/NVML ABI input manifests |
| `nvidia-tools-1.json`, `nvidia-tools/` | Stock compatibility-tool manifests |
| `pytorch-cuda-clients-1.json`, `pytorch-cuda-clients/` | On-demand PyTorch CUDA client profiles and complete wheel locks |
| `vulkan-runtime-1.json` | On-demand Mesa ICD and Vulkan validation-layer profile for host smoke tests |
| `ubuntu-20.04-target-sdk-provenance.json` | Generic Linux target SDK input provenance |
| `tests/` | Verification helpers for the declared inputs |

An update changes the smallest owning manifest, refreshes `flake.lock` only when
its resolved input changes, and runs the owning verifier. Build, test, packaging,
and qualification results are recorded by their owners rather than copied into
a milestone as a second toolchain specification.
