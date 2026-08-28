{
  pkgs,
  llvmPackages,
  projectPackages,
}:
let
  mkClangShell =
    {
      name,
      inputsFrom,
      packages ? [ ],
    }:
    pkgs.mkShell.override { stdenv = llvmPackages.stdenv; } {
      inherit name inputsFrom;
      buildInputs = [ pkgs.stdenv.cc.cc.lib ];
      packages = [
        llvmPackages.lld
        pkgs.git
        pkgs.python3
      ]
      ++ packages;
      shellHook = ''
        export CMAKE_GENERATOR=Ninja
        export CC=clang
        export CXX=clang++
        export LDFLAGS="''${LDFLAGS:+$LDFLAGS }-fuse-ld=lld"
        export NIX_LDFLAGS="''${NIX_LDFLAGS:+$NIX_LDFLAGS }-rpath ${pkgs.stdenv.cc.cc.lib}/lib"
        if git rev-parse --is-inside-work-tree > /dev/null 2>&1; then
          git config core.hooksPath .githooks
        fi
      '';
    };
in
{
  default = mkClangShell {
    name = "metaflux-core";
    inputsFrom = [
      projectPackages.daemon
      projectPackages.provider
      projectPackages.runtime
    ];
    packages = [
      llvmPackages.clang-tools
      pkgs.ccache
      pkgs.gdb
    ];
  };

  provider = mkClangShell {
    name = "metaflux-provider";
    inputsFrom = [ projectPackages.provider ];
  };

  runtime = mkClangShell {
    name = "metaflux-runtime";
    inputsFrom = [ projectPackages.runtime ];
  };
}
