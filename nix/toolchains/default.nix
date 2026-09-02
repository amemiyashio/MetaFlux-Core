{
  lib,
  pkgs,
  llvmPackages,
  epoch,
}:
let
  gccRuntime = pkgs.stdenv.cc.cc.lib;
  repositoryPython = pkgs.python3.withPackages (pythonPackages: [
    pythonPackages.pyyaml
  ]);
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
    pkgs.nixfmt
    pkgs.openssh
    pkgs.pkg-config
    repositoryPython
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
  passthru = {
    compilerEpoch = epoch;
  };
  meta = {
    description = "Pinned MetaFlux compiler epoch ${toString epoch.epoch} toolchain";
    platforms = [ "x86_64-linux" ];
  };
}
