# Ubuntu 20.04 Target SDK

Use this guide for the generic Linux ABI-floor path. It covers two distinct
operations:

1. constructing and materializing the fixed Ubuntu 20.04/glibc 2.31 SDK and
   matching generic LLVM closure;
2. explicitly consuming those tools from the CMake-owned product build before
   packaging and qualification.

Do not collapse those operations. A materialized SDK does not cause an ordinary
host build to target it.

Entering `nix develop .#release` also does not activate an Ubuntu 20.04 target
mode. That shell exposes pinned host-side tools; only an explicit CMake-owned
target entry point can select the SDK, target triple, unwrapped compiler,
startup objects, target linker, and generic LLVM/MLIR package roots together.

## Current State And Ownership

The repository already declares and materializes both required target inputs:

- `.#packages.x86_64-linux."ubuntu-20.04-target-sdk"` provides the target
  headers, startup objects, glibc 2.31 system libraries, GCC 10 layout, static
  libstdc++/libgcc inputs, zlib, loader contract, and a portable provenance
  manifest.
- `.#generic-llvm-toolchain` provides the target-built LLVM/MLIR static
  component closure and `ld.lld`, together with the SDK identity it consumed.

Keep three related inputs distinct:

| Input | Scope |
| --- | --- |
| `provider-sysroot` | Narrow C17 provider-only compile input |
| `ubuntu-20.04-target-sdk` | Complete Ubuntu 20.04 headers, startup objects, system libraries, and GCC runtime layout |
| `generic-llvm-toolchain` | LLVM/MLIR/LLD closure built against that complete SDK |

Nix owns only those fixed materializations and their exposure. The remaining
owners are unchanged:

| Operation | Owner |
| --- | --- |
| SDK and tool identity | `toolchains/` manifests and `flake.lock` |
| Fixed materialization | `nix/toolchains/` |
| Product target configuration and build | CMake and Ninja |
| Package construction | `packaging/build.py` |
| ABI and distribution qualification | `tests/` harnesses |
| Source identity | one clean Git revision and tree |

`packaging/build.py --target-sdk` does not compile or relink the product. It
installs an existing CMake build tree, copies and cross-checks provenance, adds
the target `ld.lld`, and rejects invalid payloads. A host-built provider that
requires `GLIBC_2.34` therefore fails correctly even when the SDK argument is
present.

The current `release` CMake preset selects Release and LTO but does not select
the target SDK. Until a repository-owned target toolchain configuration is
added, `cmake --preset release` is a host release build and cannot support a
generic glibc 2.31 release claim.

## SDK Construction Rules

The generic target remains `x86_64-unknown-linux-gnu` with the system loader
`/lib64/ld-linux-x86-64.so.2`. Ubuntu 20.04 and glibc 2.31 are a compatibility
floor, not the build host distribution.

When changing the SDK input:

1. Update `toolchains/ubuntu-20.04-target-sdk-provenance.json` first. Preserve
   exact Ubuntu package names, versions, architecture, paths, sizes, hashes, and
   signed archive provenance.
2. Keep `nix/toolchains/ubuntu-20.04-target-sdk.nix` package specifications
   identical to the verified provenance. The derivation may extract and
   normalize the fixed inputs; it may not define product build semantics.
3. Preserve the required target contents: C and C++ headers, startup objects,
   glibc linker scripts and DSOs, GCC 10 crt/static runtime archives, and static
   zlib. Reject broken or absolute symlinks.
4. Remove dangling selectors for optional target runtimes that are not part of
   the declared SDK instead of allowing a linker to find host copies.
5. Emit `.metaflux-target-sdk-manifest` without `/nix/store` paths. Its build
   identity must derive only from the portable recipe and package set.
6. Rebuild `generic-llvm-toolchain` from the same SDK identity. Its manifest
   must repeat the target SDK build identity and package-set hash.
7. Follow the configured-timezone, adjacent-timezone, canonical routing policy
   when acquiring bytes. Frozen hashes and signed upstream metadata remain the
   identity; the selected mirror remains per-run evidence.
8. Change `flake.lock` only when a declared flake input changes. A refreshed
   Ubuntu package with the same glibc floor still changes the package set and
   both derived identities.

Changing the glibc floor or target distribution is not an SDK refresh. It
changes decision-0009 and requires a canonical decision before implementation.

Run the input and materialization gates:

```sh
python3 -m json.tool \
  toolchains/ubuntu-20.04-target-sdk-provenance.json >/dev/null

sdk_path="$(nix build --no-link --print-out-paths \
  '.#packages.x86_64-linux."ubuntu-20.04-target-sdk"')"
generic_path="$(nix build --no-link --print-out-paths \
  '.#packages.x86_64-linux.generic-llvm-toolchain')"

test -f "$sdk_path/.metaflux-target-sdk-manifest"
test -f "$generic_path/.metaflux-generic-llvm-toolchain"
test -x "$generic_path/bin/ld.lld"
```

These checks prove provisioning only. They do not prove that a product binary
used the target headers, startup objects, libraries, or linker.

Full signed-archive qualification additionally stages every `InRelease`,
Packages index, DEB, and keyring recorded by the provenance manifest, then runs
the verifier with one assignment for every manifest record:

```sh
python3 toolchains/tests/verify_ubuntu_target_sdk_provenance.py \
  --manifest toolchains/ubuntu-20.04-target-sdk-provenance.json \
  --sdk-manifest "$sdk_path/.metaflux-target-sdk-manifest" \
  --keyring "$ubuntu_archive_keyring" \
  --gpgv "$gpgv_path" \
  --dpkg-deb "$dpkg_deb_path" \
  --inrelease RELEASE_ID=INRELEASE_PATH \
  --index INDEX_ID=PACKAGES_XZ_PATH \
  --deb PACKAGE_NAME=DEB_PATH \
  --output "$provenance_evidence"
```

Repeat `--inrelease`, `--index`, and `--deb` for the complete sets in the JSON;
the verifier rejects missing, extra, unsigned, or hash-mismatched inputs. No
checked-in driver currently resolves and supplies those arguments, so this
signed-chain verifier is not yet a routine flake or CTest gate. Track that as an
activation gap rather than citing a bare, no-argument script invocation as a
passing check.

## Product Compile Contract

The CMake owner must encode a checked-in target configuration before the work-item-0.1.0.1
generic release gate closes. Do not make agents reconstruct a release command
from memory, and do not implement the product build as a Nix derivation.

The target configuration must select all of the following explicitly:

- unwrapped Clang and Clang++ 22.1.8 from compiler epoch 1;
- `x86_64-unknown-linux-gnu` for both C and C++;
- the Ubuntu 20.04 SDK as `CMAKE_SYSROOT`;
- `$SDK/usr` as the external GCC toolchain;
- the matching Clang resource directory;
- target `ld.lld`, LLVM binutils, and the generic LLVM/MLIR package configs;
- target-only library, include, and package search, while build programs remain
  host tools;
- `/lib64/ld-linux-x86-64.so.2` for executables;
- static libstdc++, libgcc, LLVM, MLIR, and zlib where required by the release
  closure;
- no build or install RPATH/RUNPATH;
- `/usr/libexec/metaflux/ld.lld` as the installed runtime LLD path, never a Nix
  store path;
- `/usr` as the release install prefix and the intended generic library layout.

Before configure, clear host-wrapper flags that could inject host startup
objects, headers, libraries, or linker options:

```sh
unset CFLAGS CXXFLAGS CPPFLAGS LDFLAGS
unset NIX_CFLAGS_COMPILE NIX_CFLAGS_LINK NIX_LDFLAGS
```

The following is the required configuration shape for diagnosing or
implementing the CMake-owned entry point. It is not a second supported build
interface; once the checked-in target configuration exists, use that interface
instead of duplicating this command.

```sh
sdk_path="$(nix build --no-link --print-out-paths \
  '.#packages.x86_64-linux."ubuntu-20.04-target-sdk"')"
generic_path="$(nix build --no-link --print-out-paths \
  '.#packages.x86_64-linux.generic-llvm-toolchain')"
raw_clang_path="$(nix build --no-link --print-out-paths \
  '.#packages.x86_64-linux.generic-llvm-toolchain.rawCompiler')"
resource_dir="$(nix eval --raw \
  '.#packages.x86_64-linux.generic-llvm-toolchain.resourceDir')"
tool_path="$(nix build --no-link --print-out-paths \
  '.#packages.x86_64-linux.toolchain')"
build_dir="../.metaflux-build/MetaFlux-Core/generic-release"
target_triple=x86_64-unknown-linux-gnu

target_compile_flags="--target=$target_triple --sysroot=$sdk_path"
target_compile_flags="$target_compile_flags --gcc-toolchain=$sdk_path/usr"
target_compile_flags="$target_compile_flags -resource-dir=$resource_dir -pthread"
target_compile_flags="$target_compile_flags -ffile-prefix-map=$PWD=."
target_compile_flags="$target_compile_flags -ffile-prefix-map=$build_dir=."
target_compile_flags="$target_compile_flags -ffile-prefix-map=$sdk_path=/usr"
target_compile_flags="$target_compile_flags -ffile-prefix-map=$generic_path=/usr/libexec/metaflux/generic-llvm"
target_compile_flags="$target_compile_flags -ffile-prefix-map=$raw_clang_path=/usr/libexec/metaflux/clang"
target_compile_flags="$target_compile_flags -ffile-prefix-map=$resource_dir=/usr/lib/clang/22"
target_link_flags="--ld-path=$generic_path/bin/ld.lld"
target_link_flags="$target_link_flags -static-libstdc++ -static-libgcc -pthread"
target_link_flags="$target_link_flags -Wl,--build-id=sha1,-z,relro,-z,now,-z,noexecstack"

export PKG_CONFIG_SYSROOT_DIR="$sdk_path"
export PKG_CONFIG_LIBDIR="$sdk_path/usr/lib/x86_64-linux-gnu/pkgconfig:$sdk_path/usr/share/pkgconfig"

cmake -S . -B "$build_dir" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr \
  -DCMAKE_INSTALL_LIBDIR=lib \
  -DCMAKE_C_COMPILER="$raw_clang_path/bin/clang" \
  -DCMAKE_CXX_COMPILER="$raw_clang_path/bin/clang++" \
  -DCMAKE_C_COMPILER_TARGET="$target_triple" \
  -DCMAKE_CXX_COMPILER_TARGET="$target_triple" \
  -DCMAKE_C_COMPILER_EXTERNAL_TOOLCHAIN="$sdk_path/usr" \
  -DCMAKE_CXX_COMPILER_EXTERNAL_TOOLCHAIN="$sdk_path/usr" \
  -DCMAKE_SYSROOT="$sdk_path" \
  -DCMAKE_FIND_ROOT_PATH="$sdk_path;$generic_path" \
  -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER \
  -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY \
  -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY \
  -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ONLY \
  -DCMAKE_PREFIX_PATH="$generic_path" \
  -DLLVM_DIR="$generic_path/lib/cmake/llvm" \
  -DMLIR_DIR="$generic_path/lib/cmake/mlir" \
  -DZLIB_USE_STATIC_LIBS=ON \
  -DZLIB_INCLUDE_DIR="$sdk_path/usr/include" \
  -DZLIB_LIBRARY="$sdk_path/usr/lib/x86_64-linux-gnu/libz.a" \
  -DCMAKE_AR="$tool_path/bin/llvm-ar" \
  -DCMAKE_RANLIB="$tool_path/bin/llvm-ranlib" \
  -DCMAKE_NM="$tool_path/bin/llvm-nm" \
  -DCMAKE_OBJCOPY="$tool_path/bin/llvm-objcopy" \
  -DCMAKE_STRIP="$tool_path/bin/llvm-strip" \
  -DCMAKE_C_FLAGS="$target_compile_flags" \
  -DCMAKE_CXX_FLAGS="$target_compile_flags" \
  -DCMAKE_EXE_LINKER_FLAGS="$target_link_flags -Wl,--dynamic-linker=/lib64/ld-linux-x86-64.so.2" \
  -DCMAKE_MODULE_LINKER_FLAGS="$target_link_flags" \
  -DCMAKE_SHARED_LINKER_FLAGS="$target_link_flags" \
  -DCMAKE_SKIP_BUILD_RPATH=ON \
  -DCMAKE_SKIP_INSTALL_RPATH=ON \
  -DMETAFLUX_USE_LLD=ON \
  -DMETAFLUX_COMPILER_LINK_SHARED_LLVM=OFF \
  -DMETAFLUX_RUNTIME_LLD_PATH=/usr/libexec/metaflux/ld.lld \
  -DMETAFLUX_ENABLE_LTO=ON

cmake --build "$build_dir" --parallel
```

The complete release, provider libraries, activation launcher, CUDA acceptance
fixture, and every other executable used to support a generic compatibility
claim must use the same target tuple. A clean payload plus a host-built test
helper remains an incomplete qualification.

A build-tree-only test may receive the materialized LLD path so it can execute
before installation. That test binary is not package payload and does not
replace the installed-path check. Every installed CPU compiler consumer must
embed `/usr/libexec/metaflux/ld.lld`; `packaging/build.py` installs the matching
target LLD at that path.

`provider-sysroot` is the narrower C17 provider input. Its existence or a
provider-only pass does not replace the complete SDK and generic LLVM closure
required by the full CPU-backed release.

## Driver And Cache Audit

Before a long build, inspect one compiler-driver invocation with `-###`. It must
show startup objects and libraries under the target SDK and the selected target
LLD. Any host `/usr/lib`, Nix glibc startup object, or wrapper-injected path is a
configuration failure.

After configure, inspect `CMakeCache.txt` for the exact compiler, target,
sysroot, external toolchain, LLVM/MLIR directories, runtime LLD path, and RPATH
settings. Do not infer these values from the development shell name.

## Packaging And Qualification

Package only a clean target build:

```sh
output_dir="../.metaflux-evidence/MetaFlux-Core/milestone-0.1.0.0-generic-packages"
source_date_epoch="$(git show -s --format=%ct HEAD)"

SOURCE_DATE_EPOCH="$source_date_epoch" \
python3 packaging/build.py \
  --build-dir "$build_dir" \
  --output-dir "$output_dir" \
  --kind complete \
  --target-sdk "$sdk_path" \
  --generic-toolchain "$generic_path"
```

The package gate must reject every payload ELF with any of these properties:

- a requested interpreter other than `/lib64/ld-linux-x86-64.so.2`;
- RPATH or RUNPATH;
- a dynamic dependency outside the release allowlist;
- a referenced glibc symbol newer than `GLIBC_2.31`;
- an embedded `/nix/store` path;
- a generic toolchain manifest built from a different SDK identity.

Then run the digest-pinned offline release matrix from
`tests/release/README.md`. Ubuntu 20.04 execution proves the floor; newer Ubuntu
and Rocky rows prove forward runtime compatibility. Rebuild independently from
the same clean Git revision and compare DEB, RPM, and tar bytes before claiming
single-revision reproducibility.

## Failure Diagnosis

| Observation | Meaning | Correction owner |
| --- | --- | --- |
| `GLIBC_2.34` or newer | Host glibc, startup objects, headers, or wrapper flags entered the link | CMake target configuration |
| SDK and generic manifests exist but package validation fails | Provisioning succeeded; the existing product build is invalid | CMake/Ninja build, then packaging retry |
| `libstdc++.so`, `libgcc_s.so`, `libLLVM`, or `libMLIR` is required | Static release closure is incomplete | CMake target links |
| `/nix/store` appears in payload | RPATH, absolute runtime helper path, debug string, or imported build path leaked | Build/install configuration |
| Wrong ELF interpreter | Executable link flags did not freeze the system loader | CMake executable linker flags |
| Manifest identities differ | SDK and generic LLVM were materialized from different input identities | Toolchain materialization |
| Ubuntu 20.04 execution fails after clean ELF checks | The declared floor is not operationally qualified | Release harness and owning implementation |

Do not respond to `GLIBC_2.34` by declaring another SDK, raising the floor,
weakening the package gate, shipping a private glibc, or moving the product build
into Nix. The first diagnostic question is which compiler, sysroot, startup
objects, libraries, and linker the CMake build actually selected.
