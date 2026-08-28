---
id: M0001-W01
milestone: M0001
status: Active
area: build-toolchain
depends_on: []
updated: 2026-08-28
---

# Build, Toolchain, and Reproducible Environment

## Outcome

Establish the reproducible CMake/Ninja/Nix foundation and release dependency
boundaries required by every later M0001 workstream.

## Locked Constraints

| Layer | Language | Constraint |
| --- | --- | --- |
| CUDA/NVML providers and client fast path | C17 | glibc-family dependencies only; no C++ runtime in the application process |
| `metafluxd`, compiler, scheduler, CPU backend | C++20 | LLVM/MLIR remain outside the application process |
| Backend/daemon boundary | Versioned C ABI | `abi_version`, `struct_size`, capability bits, extension chain |
| Provider/runtime boundary | Versioned client protocol ABI | Ecosystem-neutral and independent of backend ABI |
| ABI/manifest generators | Python | Build-time only |
| vPCI/DKMS beginning in M0002 | Linux GNU C | Target-kernel Kbuild |

Rust, Cargo, `.rs` sources, and any `rustc`/Cargo build requirement are excluded.
Assembly is excluded from v0.1 unless profiling, disassembly, and a hardware
benchmark jointly prove a specific compiler-codegen failure.

- The current compiler-epoch-1 descriptor selects stock
  Clang/LLVM/MLIR/LLD 22.1.8 with an empty downstream patch list. The epoch becomes
  release-frozen only after the exact correctness patchset and derivation hash are
  qualified and recorded; until then, neither is described as verified.
- `toolchains/compiler-epoch-1.json` is the single CMake/Nix epoch descriptor and
  records source revision, Nix derivation, patches, flags, targets, and PGO ID.
- Clang selects LLD through `-fuse-ld=lld`; configure and Nix builds verify the
  actual linker rather than relying on an installed `ld.lld` binary.
- LLVM 23 remains qualification-only and is never mixed with LLVM 22 in one daemon.
- LLVM 22 loop-vectorizer reproducers, including upstream #186922, gate epoch 1.
- Userspace feature-probes the host; Linux 6.12 is a later vPCI validation line,
  not a v0.1 userspace requirement.
- Ubuntu 20.04 LTS is the minimum supported userspace distribution baseline;
  its glibc 2.31 is the fixed userspace ABI floor (D0009), not a candidate.
- A dedicated Ubuntu 20.04/glibc 2.31 provider sysroot controls the release
  symbol ceiling. The remaining distribution matrix may add qualification
  targets but may not raise this floor.
- Providers do not link `libsystemd`; systemd supplies service/socket activation.

## Nix Outputs and Rules

```text
devShells.x86_64-linux.default
packages.x86_64-linux.toolchain
packages.x86_64-linux.provider
packages.x86_64-linux.daemon
packages.x86_64-linux.runtime
checks.x86_64-linux.format
checks.x86_64-linux.unit
checks.x86_64-linux.abi
checks.x86_64-linux.integration
checks.x86_64-linux.release-closure
checks.x86_64-linux.performance-smoke
checks.x86_64-linux.runtime-sdk
checks.x86_64-linux.toolchain-sdk
checks.x86_64-linux.agent-records
```

- The bootstrap contains `flake.nix` and commits `flake.lock`; the lock, not a
  moving branch, is authoritative.
- Select `llvmPackages_22`, use cached upstream packages first, and put later
  custom LLVM/MLIR/PGO builds in a MetaFlux binary cache.
- Use `strictDeps = true` and separate filesets for runtime, provider, daemon,
  tests, and formatting.
- Build generic providers against the Ubuntu 20.04/glibc 2.31 release sysroot,
  not Nix host glibc, and reject symbols versioned newer than `GLIBC_2.31`.
- Generic `.deb`, `.rpm`, and `.tar` artifacts contain no required `/nix/store`
  interpreter, RPATH, or runtime path; native NixOS packages may use the store.
- Release checks inspect `readelf -dW`, `readelf -sDW`,
  `readelf --version-info`, exported symbols, SONAMEs, and complete closures.

## Work

- [ ] Maintain warning, sanitizer, coverage, LTO, profiling, and test presets.
- [ ] Complete the locked flake, provider sysroot, and initial checks.
- [ ] Keep CUDA Driver, NVML, PTX frontend, backend compiler, and backend runtime
  roles independently selectable in CMake and Nix.
- [ ] Enforce the source-language policy and format/lint configuration.
- [ ] Emit a build manifest containing every compiler epoch input.
- [ ] Qualify both Intel and AMD x86_64 build/reference hosts.

## Exit Gate

A clean machine can run `nix develop path:.`, configure and build with
CMake/Ninja, and execute C/C++ ABI smoke tests without undeclared host
dependencies. Release artifacts prove the linker, compiler epoch, glibc ceiling,
and absence of forbidden runtime closures.
