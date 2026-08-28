{
  lib,
  pkgs,
  llvmPackages,
  epoch,
}:
let
  gccRuntime = pkgs.stdenv.cc.cc.lib;
in
pkgs.buildEnv {
  name = "metaflux-toolchain-llvm-${epoch.llvm_version}";
  paths = [
    llvmPackages.clang
    llvmPackages.clang-tools
    llvmPackages.lld
    llvmPackages.llvm
    (lib.getLib llvmPackages.llvm)
    (lib.getDev llvmPackages.llvm)
    llvmPackages.mlir
    (lib.getDev llvmPackages.mlir)
    (lib.getLib pkgs.libffi)
    (lib.getDev pkgs.libffi)
    (lib.getLib pkgs.libxml2)
    (lib.getDev pkgs.libxml2)
    (lib.getLib pkgs.zlib)
    (lib.getDev pkgs.zlib)
    pkgs.cmake
    pkgs.ninja
    pkgs.pkg-config
    pkgs.python3
    gccRuntime
  ];
  pathsToLink = [
    "/bin"
    "/include"
    "/lib"
    "/lib/cmake"
    "/share"
  ];
  ignoreCollisions = false;
  nativeBuildInputs = [ pkgs.makeWrapper ];
  postBuild = ''
    wrapProgram "$out/bin/clang" \
      --prefix NIX_LDFLAGS " " "-rpath ${gccRuntime}/lib"
    wrapProgram "$out/bin/clang++" \
      --prefix NIX_LDFLAGS " " "-rpath ${gccRuntime}/lib"
  '';
  passthru = {
    compilerEpoch = epoch;
  };
  meta = {
    description = "Pinned MetaFlux compiler epoch ${toString epoch.epoch} toolchain";
    platforms = [ "x86_64-linux" ];
  };
}
