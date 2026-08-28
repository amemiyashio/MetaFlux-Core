{
  lib,
  pkgs,
  llvmPackages,
  cmakeOptions,
  mkMetafluxPackage,
  source,
  toolchain,
}:
let
  runtime = mkMetafluxPackage {
    pname = "metaflux-runtime";
    src = source.runtime;
    cmakeFlags = cmakeOptions {
      runtimeCore = true;
      clientFastpath = true;
      lto = true;
    };
    installComponents = [ "Runtime" ];
  };

  provider = mkMetafluxPackage {
    pname = "metaflux-provider";
    src = source.provider;
    cmakeFlags = cmakeOptions {
      cudaDriverProvider = true;
      nvmlProvider = true;
      clientFastpath = true;
      lto = true;
    };
    installComponents = [ "Provider" ];
    usesCxx = false;
  };

  daemon = mkMetafluxPackage {
    pname = "metaflux-daemon";
    src = source.daemon;
    cmakeFlags = cmakeOptions {
      runtimeCore = true;
      daemon = true;
      compiler = true;
      cpuBackendRuntime = true;
      cudaPtxFrontend = true;
      lto = true;
    };
    installComponents = [ "Daemon" ];
    extraBuildInputs = [
      llvmPackages.llvm
      llvmPackages.mlir
    ];
  };
in
{
  inherit
    daemon
    provider
    runtime
    toolchain
    ;
}
