{
  pkgs,
  llvmPackages,
  toolPackages,
}:
let
  mkClangShell =
    {
      name,
      packages ? [ ],
    }:
    pkgs.mkShell.override { stdenv = llvmPackages.stdenv; } {
      inherit name;
      NIX_NO_SELF_RPATH = 1;
      buildInputs = [ pkgs.stdenv.cc.cc.lib ];
      packages = [
        toolPackages.toolchain
        pkgs.git
      ]
      ++ packages;
      shellHook = ''
        export CMAKE_GENERATOR=Ninja
        export CC=clang
        export CXX=clang++
        export LDFLAGS="''${LDFLAGS:+$LDFLAGS }-fuse-ld=lld"
        export LD_LIBRARY_PATH="${toolPackages.toolchain}/lib''${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
      '';
    };
in
{
  default = mkClangShell {
    name = "metaflux-core-tools";
    packages = [
      pkgs.ccache
      pkgs.gdb
      pkgs.strace
    ];
  };

  provider = mkClangShell {
    name = "metaflux-provider-tools";
    packages = [
      toolPackages.provider-headers
      toolPackages.provider-sysroot
    ];
  };

  runtime = mkClangShell {
    name = "metaflux-runtime-tools";
  };

  release = mkClangShell {
    name = "metaflux-release-tools";
    packages = [
      toolPackages.generic-llvm-toolchain
      toolPackages.nvidia-stock-tools
      toolPackages.provider-headers
      toolPackages."ubuntu-20.04-target-sdk"
      pkgs.binutils
      pkgs.dpkg
      pkgs.file
      pkgs.gnutar
      pkgs.gzip
      pkgs.jq
      pkgs.patchelf
      pkgs.podman
      pkgs.rpm
    ];
  };
}
