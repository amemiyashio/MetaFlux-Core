---
id: W0101
delivery: 0.1.0.1
milestone: M0100
status: Complete
area: build-toolchain
depends_on: []
updated: 2026-08-30
---

# Build and Release Prerequisites

## Outcome

Prove that M0100 can be configured, built, tested, and packaged with the
repository's declared tools and release boundaries. This work item consumes the
canonical [toolchain declarations](../../../../toolchains/README.md); it does not
define tool versions or assign project workflows to Nix.

## Canonical Inputs

- Tool versions, immutable inputs, patches, download routing, and the Nix tool
  provider boundary: [`toolchains/README.md`](../../../../toolchains/README.md).
- Configure and build graph: [`CMakePresets.json`](../../../../CMakePresets.json)
  and the top-level CMake project.
- Cross-component and release qualification: [`tests/`](../../../../tests/README.md).
- Generic and native release integration: [`packaging/`](../../../../packaging/README.md).

The M0100 language and closure tasks remain: providers and the client fast path
use C17 without a C++ runtime; daemon, compiler, scheduler, and CPU backend use
C++20; cross-component boundaries use versioned C ABIs; build-time generators
may use Python; generic userspace artifacts retain the glibc 2.31 floor.

## Release Qualification Matrix (D0012)

M0100 / `v0.1.0` qualifies the following release rows. Image, kernel,
repository, update, install, upgrade, removal, and coexistence details are
per-run test evidence.

| Distribution | Generic artifacts | Qualification role |
| --- | --- | --- |
| Ubuntu 20.04.6 LTS | `.deb`, `.tar` | glibc 2.31 ABI floor |
| Ubuntu 22.04.5 LTS | `.deb`, `.tar` | primary CUDA/NVML stock-tool fixture |
| Ubuntu 24.04.4 LTS | `.deb`, `.tar` | current Ubuntu line |
| Rocky Linux 9.8 | `.rpm`, `.tar` | RPM-family line |

The matrix is a product release task. Artifact construction belongs to
`packaging/`, execution and evidence belong to `tests/release/`, and Nix's
tool-provider role does not acquire either responsibility.

Native NixOS VM/package qualification is a `v0.2.0` support-expansion task. It
does not add a fifth M0100 row and does not transfer package construction or VM
qualification semantics to Nix.

## Work

- [x] Provide the declared compiler, linker, build, inspection, and test tool
  versions through the locked development environment.
- [x] Maintain warning, sanitizer, coverage, LTO, profiling, and test presets.
- [x] Keep CUDA Driver, NVML, PTX frontend, backend compiler, and backend runtime
  roles independently selectable in CMake.
- [x] Enforce the source-language policy and format/lint configuration.
- [x] Emit build manifests containing the compiler epoch inputs used by each
  relevant artifact.
- [x] Qualify the D0018 LLVM correctness patch and reproducer and the D0016
  CUDA/NVML header input matrix.
- [x] Implement signed Ubuntu 20.04 target-SDK provenance verification and the
  generic glibc-floor toolchain path.
- [x] Qualify the M0100 AMD x86_64 build/reference host. AMD Ryzen 7 H 255
  passed the recorded integration suite and D0015 placement checks.
- [x] Execute every D0012 install, upgrade, removal, closure, and coexistence row
  and archive its owning harness evidence.
  The provider-only harness passed all eight digest-pinned rows once; the
  complete harness passed all eight rows twice from one clean revision.
  Independent packages matched for DEB, RPM, and tar bytes. These results
  remain bound to their recorded Git revision and evidence invocation.

D0027 supersedes D0023 only for the future destination: Intel x86_64 support
qualification belongs to M1000 / `v1.0.0`. The absence of an Intel host does
not leave this M0100 work item open.

## Exit Gate

From one clean Git revision, the declared tool environment supports CMake/Ninja
configure and build, CTest ABI/correctness checks, and the packaging-owned
release flow without an undeclared tool version. Generic artifacts prove the
selected linker and compiler epoch, the glibc ceiling, and the absence of
forbidden runtime dependencies. Evidence records the Git commit/tree and
toolchain manifest identities; it does not create another source snapshot or a
Nix store-retention requirement.
