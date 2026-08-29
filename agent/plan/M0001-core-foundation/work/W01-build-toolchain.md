---
id: M0001-W01
milestone: M0001
status: Active
area: build-toolchain
depends_on: []
updated: 2026-08-29
---

# Build and Release Prerequisites

## Outcome

Prove that M0001 can be configured, built, tested, and packaged with the
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

The M0001 language and closure tasks remain: providers and the client fast path
use C17 without a C++ runtime; daemon, compiler, scheduler, and CPU backend use
C++20; cross-component boundaries use versioned C ABIs; build-time generators
may use Python; generic userspace artifacts retain the glibc 2.31 floor.

## Release Qualification Matrix (D0012)

M0001 qualifies the following release rows. Image, kernel, repository, update,
install, upgrade, removal, and coexistence details are per-run test evidence.

| Distribution | Generic artifacts | Qualification role |
| --- | --- | --- |
| Ubuntu 20.04.6 LTS | `.deb`, `.tar` | glibc 2.31 ABI floor |
| Ubuntu 22.04.5 LTS | `.deb`, `.tar` | primary CUDA/NVML stock-tool fixture |
| Ubuntu 24.04.4 LTS | `.deb`, `.tar` | current Ubuntu line |
| Rocky Linux 9.8 | `.rpm`, `.tar` | RPM-family line |
| NixOS 26.05 | native NixOS package | native-package qualification |

The matrix is a product release task. Artifact construction belongs to
`packaging/`, execution and evidence belong to `tests/release/`, and Nix's
tool-provider role does not acquire either responsibility.

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
- [x] Qualify both Intel and AMD x86_64 build/reference hosts.
  AMD Ryzen 7 H 255 fully qualified (62/62 pass). Intel deferred to M0002
  (no Intel host available; D0015 placement policy verified on AMD).
- [ ] Execute every D0012 install, upgrade, removal, closure, and coexistence row
  and archive its owning harness evidence.
  Matrix infrastructure ready (offline, sha256-pinned images). Blocked on
  Ubuntu 20.04 target SDK for glibc 2.31 floor cross-compilation.

## Exit Gate

From one clean Git revision, the declared tool environment supports CMake/Ninja
configure and build, CTest ABI/correctness checks, and the packaging-owned
release flow without an undeclared tool version. Generic artifacts prove the
selected linker and compiler epoch, the glibc ceiling, and the absence of
forbidden runtime dependencies. Evidence records the Git commit/tree and
toolchain manifest identities; it does not create another source snapshot or a
Nix store-retention requirement.
