{
  lib,
  pkgs,
  llvmPackages,
}:
{
  pname,
  src,
  cmakeFlags,
  installComponents ? [ ],
  buildTarget ? null,
  checkCommand ? null,
  extraNativeBuildInputs ? [ ],
  extraBuildInputs ? [ ],
  usesCxx ? true,
}:
let
  archiverFlags = [
    "-DCMAKE_INSTALL_LIBDIR=lib"
    "-DCMAKE_AR=${llvmPackages.bintools}/bin/ar"
    "-DCMAKE_RANLIB=${llvmPackages.bintools}/bin/ranlib"
    "-DCMAKE_C_COMPILER_AR=${llvmPackages.bintools}/bin/ar"
    "-DCMAKE_C_COMPILER_RANLIB=${llvmPackages.bintools}/bin/ranlib"
  ]
  ++ lib.optionals usesCxx [
    "-DCMAKE_CXX_COMPILER_AR=${llvmPackages.bintools}/bin/ar"
    "-DCMAKE_CXX_COMPILER_RANLIB=${llvmPackages.bintools}/bin/ranlib"
  ];
  renderedCmakeFlags = lib.concatStringsSep " " (
    map lib.escapeShellArg (cmakeFlags ++ archiverFlags)
  );
  renderedBuildTarget = lib.optionalString (buildTarget != null) (
    " --target ${lib.escapeShellArg buildTarget}"
  );
  installCommands = lib.concatMapStringsSep "\n" (
    component: "cmake --install build --prefix \"$out\" --component ${lib.escapeShellArg component}"
  ) installComponents;
in
llvmPackages.stdenv.mkDerivation {
  inherit pname;
  version = "0.1.0";
  inherit src;

  strictDeps = true;
  dontUseCmakeConfigure = true;

  nativeBuildInputs = [
    pkgs.cmake
    pkgs.ninja
    llvmPackages.lld
  ]
  ++ extraNativeBuildInputs;
  buildInputs = extraBuildInputs;

  configurePhase = ''
    runHook preConfigure
    export NIX_LDFLAGS="''${NIX_LDFLAGS:+$NIX_LDFLAGS }-rpath ${pkgs.stdenv.cc.cc.lib}/lib"
    cmake -S . -B build -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX="$out" \
      ${renderedCmakeFlags}
    grep -Fqx 'METAFLUX_USE_LLD:BOOL=ON' build/CMakeCache.txt
    printf '%s\n' 'int main(void) { return 0; }' > "$TMPDIR/metaflux-linker-probe.c"
    "$CC" -fuse-ld=lld -Wl,--version "$TMPDIR/metaflux-linker-probe.c" \
      -o "$TMPDIR/metaflux-linker-probe" 2>&1 \
      | grep -F 'LLD ${llvmPackages.lld.version}'
    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild
    cmake --build build --parallel "$NIX_BUILD_CORES"${renderedBuildTarget}
    runHook postBuild
  '';

  doCheck = checkCommand != null;
  checkPhase = lib.optionalString (checkCommand != null) ''
    runHook preCheck
    ${checkCommand}
    runHook postCheck
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p "$out"
    ${installCommands}
    touch "$out/.metaflux-${pname}"
    runHook postInstall
  '';

  enableParallelBuilding = true;

  meta = {
    description = "MetaFlux Core ${pname} build";
    platforms = [ "x86_64-linux" ];
  };
}
