{
  lib,
  pkgs,
  llvmPackages,
  epoch,
  llvmPatch,
  targetSdk,
}:
let
  targetTriple = targetSdk.targetTriple;
  rawClang = llvmPackages.clang-unwrapped;
  clangResourceDir = "${lib.getLib rawClang}/lib/clang/${lib.versions.major llvmPackages.llvm.version}";
  hostTablegen = llvmPackages.tblgen;
  libunwindSource = llvmPackages.libunwind.src;
  downstreamPatchSha256 = builtins.hashFile "sha256" llvmPatch;
  recipeSha256 = builtins.hashFile "sha256" ./generic-llvm-toolchain.nix;
  buildIdentity = builtins.hashString "sha256" (
    builtins.toJSON {
      schema = 1;
      compilerEpoch = epoch.epoch;
      llvmVersion = epoch.llvm_version;
      llvmSourceRevision = epoch.llvm_source_revision;
      inherit
        downstreamPatchSha256
        recipeSha256
        targetTriple
        ;
      targetSdkBuildIdentity = targetSdk.buildIdentity;
      targetSdkPackageSetSha256 = targetSdk.packageSetSha256;
    }
  );
in
pkgs.stdenvNoCC.mkDerivation {
  pname = "metaflux-generic-llvm-toolchain";
  version = epoch.llvm_version;

  dontUnpack = true;
  dontUseCmakeConfigure = true;
  strictDeps = true;

  nativeBuildInputs = [
    pkgs.binutils
    pkgs.cmake
    pkgs.ninja
    pkgs.patch
    pkgs.patchelf
    pkgs.python3
  ];

  configurePhase = ''
    runHook preConfigure

    mkdir source
    cp -a ${llvmPackages.llvm.src}/. source/
    chmod -R u+w source
    cp -a ${llvmPackages.mlir.src}/mlir source/
    cp -a ${llvmPackages.lld.src}/lld source/
    cp -a ${libunwindSource}/libunwind source/
    chmod -R u+w source
    patch -d source/llvm -p1 < ${llvmPatch}

    rawCFlags="--target=${targetTriple} --gcc-toolchain=${targetSdk}/usr"
    rawCFlags+=" -resource-dir=${clangResourceDir} -pthread"
    rawCFlags+=" -ffile-prefix-map=$PWD/source=. -fdebug-prefix-map=$PWD/source=."
    rawCxxFlags="$rawCFlags"
    rawLinkFlags="--ld-path=${llvmPackages.lld}/bin/ld.lld"
    rawLinkFlags+=" -static-libstdc++ -static-libgcc -pthread"
    rawLinkFlags+=" -Wl,--build-id=sha1 -Wl,-z,relro,-z,now,-z,noexecstack"
    rawExeLinkFlags="$rawLinkFlags -Wl,--dynamic-linker=/lib64/ld-linux-x86-64.so.2"

    export CC=${rawClang}/bin/clang
    export CXX=${rawClang}/bin/clang++
    export PKG_CONFIG_LIBDIR=${targetSdk}/usr/lib/x86_64-linux-gnu/pkgconfig:${targetSdk}/usr/share/pkgconfig
    unset CFLAGS CXXFLAGS CPPFLAGS LDFLAGS NIX_CFLAGS_COMPILE NIX_CFLAGS_LINK NIX_LDFLAGS

    cmake -S source/llvm -B build -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX="$out" \
      -DCMAKE_INSTALL_LIBDIR=lib \
      -DCMAKE_C_COMPILER="$CC" \
      -DCMAKE_CXX_COMPILER="$CXX" \
      -DCMAKE_AR=${llvmPackages.llvm}/bin/llvm-ar \
      -DCMAKE_RANLIB=${llvmPackages.llvm}/bin/llvm-ranlib \
      -DCMAKE_NM=${llvmPackages.llvm}/bin/llvm-nm \
      -DCMAKE_OBJCOPY=${llvmPackages.llvm}/bin/llvm-objcopy \
      -DCMAKE_STRIP=${llvmPackages.llvm}/bin/llvm-strip \
      -DCMAKE_C_COMPILER_AR=${llvmPackages.llvm}/bin/llvm-ar \
      -DCMAKE_C_COMPILER_RANLIB=${llvmPackages.llvm}/bin/llvm-ranlib \
      -DCMAKE_CXX_COMPILER_AR=${llvmPackages.llvm}/bin/llvm-ar \
      -DCMAKE_CXX_COMPILER_RANLIB=${llvmPackages.llvm}/bin/llvm-ranlib \
      -DCMAKE_C_COMPILER_TARGET=${targetTriple} \
      -DCMAKE_CXX_COMPILER_TARGET=${targetTriple} \
      -DCMAKE_C_COMPILER_EXTERNAL_TOOLCHAIN=${targetSdk}/usr \
      -DCMAKE_CXX_COMPILER_EXTERNAL_TOOLCHAIN=${targetSdk}/usr \
      -DCMAKE_SYSROOT=${targetSdk} \
      -DCMAKE_FIND_ROOT_PATH=${targetSdk} \
      -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER \
      -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY \
      -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY \
      -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ONLY \
      -DCMAKE_C_FLAGS="$rawCFlags" \
      -DCMAKE_CXX_FLAGS="$rawCxxFlags" \
      -DCMAKE_EXE_LINKER_FLAGS="$rawExeLinkFlags" \
      -DCMAKE_MODULE_LINKER_FLAGS="$rawLinkFlags" \
      -DCMAKE_SHARED_LINKER_FLAGS="$rawLinkFlags" \
      -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
      -DCMAKE_SKIP_BUILD_RPATH=ON \
      -DCMAKE_SKIP_INSTALL_RPATH=ON \
      -DLLVM_ENABLE_PROJECTS='mlir;lld' \
      -DLLVM_TARGETS_TO_BUILD=X86 \
      -DLLVM_EXPERIMENTAL_TARGETS_TO_BUILD= \
      -DLLVM_DEFAULT_TARGET_TRIPLE=${targetTriple} \
      -DLLVM_HOST_TRIPLE=${targetTriple} \
      -DLLVM_TABLEGEN=${hostTablegen}/bin/llvm-tblgen \
      -DLLVM_TABLEGEN_EXE=${hostTablegen}/bin/llvm-tblgen \
      -DMLIR_TABLEGEN_EXE=${hostTablegen}/bin/mlir-tblgen \
      -DMLIR_LINALG_ODS_YAML_GEN=${llvmPackages.mlir}/bin/mlir-linalg-ods-yaml-gen \
      -DBUILD_SHARED_LIBS=OFF \
      -DLLVM_BUILD_LLVM_DYLIB=OFF \
      -DLLVM_LINK_LLVM_DYLIB=OFF \
      -DLLVM_ENABLE_RTTI=ON \
      -DLLVM_ENABLE_EH=OFF \
      -DLLVM_ENABLE_PIC=ON \
      -DLLVM_ENABLE_THREADS=ON \
      -DLLVM_ENABLE_ASSERTIONS=OFF \
      -DLLVM_ENABLE_FFI=OFF \
      -DLLVM_ENABLE_LIBEDIT=OFF \
      -DLLVM_ENABLE_LIBPFM=OFF \
      -DLLVM_ENABLE_LIBXML2=OFF \
      -DLLVM_ENABLE_ZSTD=OFF \
      -DLLVM_ENABLE_ZLIB=FORCE_ON \
      -DZLIB_USE_STATIC_LIBS=ON \
      -DZLIB_INCLUDE_DIR=${targetSdk}/usr/include \
      -DZLIB_LIBRARY=${targetSdk}/usr/lib/x86_64-linux-gnu/libz.a \
      -DLLVM_INCLUDE_TESTS=OFF \
      -DLLVM_BUILD_TESTS=OFF \
      -DLLVM_INCLUDE_BENCHMARKS=OFF \
      -DLLVM_INCLUDE_DOCS=OFF \
      -DLLVM_BUILD_DOCS=OFF \
      -DLLVM_INCLUDE_EXAMPLES=OFF \
      -DLLVM_BUILD_EXAMPLES=OFF \
      -DLLVM_INCLUDE_UTILS=OFF \
      -DLLVM_INSTALL_UTILS=OFF \
      -DLLVM_BUILD_TOOLS=OFF \
      -DLLD_BUILD_TOOLS=ON \
      -DMLIR_INCLUDE_TESTS=OFF \
      -DMLIR_INCLUDE_INTEGRATION_TESTS=OFF \
      -DMLIR_ENABLE_BINDINGS_PYTHON=OFF \
      -DLLVM_ENABLE_BINDINGS=OFF \
      -DLLVM_ENABLE_OCAMLDOC=OFF \
      -DLLVM_ENABLE_CURL=OFF \
      -DLLVM_ENABLE_HTTPLIB=OFF \
      -DLLVM_ENABLE_Z3_SOLVER=OFF \
      -DLLVM_USE_RELATIVE_PATHS_IN_FILES=ON \
      -DLLVM_USE_RELATIVE_PATHS_IN_DEBUG_INFO=ON \
      -DLLVM_INSTALL_PACKAGE_DIR=lib/cmake/llvm \
      -DMLIR_INSTALL_PACKAGE_DIR=lib/cmake/mlir \
      -DLLD_INSTALL_PACKAGE_DIR=lib/cmake/lld

    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild
    cmake --build build --parallel "$NIX_BUILD_CORES"
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    cmake --install build

    test -x "$out/bin/ld.lld"
    test -f "$out/include/llvm/IR/Module.h"
    test -f "$out/include/mlir/IR/MLIRContext.h"
    test -f "$out/lib/cmake/llvm/LLVMConfig.cmake"
    test -f "$out/lib/cmake/mlir/MLIRConfig.cmake"
    test -f "$out/lib/cmake/lld/LLDConfig.cmake"
    test -f "$out/lib/libLLVMSupport.a"
    test -f "$out/lib/libMLIRIR.a"

    test "$(patchelf --print-interpreter "$out/bin/ld.lld")" = \
      /lib64/ld-linux-x86-64.so.2
    test -z "$(patchelf --print-rpath "$out/bin/ld.lld")"
    if readelf --version-info -W "$out/bin/ld.lld" \
      | grep -E 'GLIBC_2\.(3[2-9]|[4-9][0-9])|GLIBC_[3-9][0-9]*\.'; then
      echo "generic ld.lld requires glibc newer than 2.31" >&2
      exit 1
    fi
    if strings "$out/bin/ld.lld" | grep -F /nix/store/; then
      echo "generic ld.lld embeds a Nix store path" >&2
      exit 1
    fi
    ${targetSdk}/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2 \
      --library-path ${targetSdk}/lib/x86_64-linux-gnu:${targetSdk}/usr/lib/x86_64-linux-gnu \
      "$out/bin/ld.lld" --version \
      | grep -F 'LLD ${epoch.llvm_version}'

    cat > "$out/.metaflux-generic-llvm-toolchain" <<EOF
    compiler-epoch=${toString epoch.epoch}
    llvm-version=${epoch.llvm_version}
    llvm-source-revision=${epoch.llvm_source_revision}
    target-triple=${targetTriple}
    target-sdk-distribution=${targetSdk.distribution}
    target-sdk-build-identity=sha256-${targetSdk.buildIdentity}
    target-sdk-package-set-sha256=${targetSdk.packageSetSha256}
    downstream-patch-sha256=${downstreamPatchSha256}
    libunwind-source-revision=${epoch.llvm_source_revision}
    recipe-sha256=${recipeSha256}
    build-identity=sha256-${buildIdentity}
    EOF

    if grep -F /nix/store/ "$out/.metaflux-generic-llvm-toolchain"; then
      echo "generic LLVM provenance manifest embeds a Nix store path" >&2
      exit 1
    fi

    runHook postInstall
  '';

  enableParallelBuilding = true;
  requiredSystemFeatures = [ "big-parallel" ];

  passthru = {
    compilerEpoch = epoch;
    inherit targetSdk targetTriple;
    hostTablegen = hostTablegen;
    rawCompiler = rawClang;
    resourceDir = clangResourceDir;
    inherit
      buildIdentity
      downstreamPatchSha256
      recipeSha256
      ;
  };

  meta = {
    description = "LLVM/MLIR ${epoch.llvm_version} static target SDK for glibc 2.31";
    platforms = [ "x86_64-linux" ];
  };
}
