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

Development enters the Git-aware flake with `nix develop .`; the raw-path flake
form is not a project workflow because it treats the working directory,
including ignored build output, as a Nix source input. Git remains the source
snapshot authority. Qualification records a clean Git commit and tree identity
rather than a Nix source-store path or a second per-file source snapshot.

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
| `ubuntu-20.04-target-sdk-provenance.json` | Generic Linux target SDK input provenance |
| `tests/` | Verification helpers for the declared inputs |

An update changes the smallest owning manifest, refreshes `flake.lock` only when
its resolved input changes, and runs the owning verifier. Build, test, packaging,
and qualification results are recorded by their owners rather than copied into
a milestone as a second toolchain specification.
