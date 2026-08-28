{
  self,
  nixpkgs,
}:
let
  system = "x86_64-linux";
  pkgs = import nixpkgs {
    inherit system;
  };
  lib = pkgs.lib;
  epoch = builtins.fromJSON (builtins.readFile ../toolchains/compiler-epoch-1.json);
  flakeLock = builtins.fromJSON (builtins.readFile ../flake.lock);
  lockedNixpkgs = flakeLock.nodes.nixpkgs.locked;
  llvmPackages = pkgs.${epoch.nixpkgs_package};
  source = import ./lib/source.nix { inherit lib; };
  cmakeOptions = import ./lib/cmake-options.nix { inherit lib; };
  mkMetafluxPackage = import ./lib/mk-metaflux-package.nix {
    inherit
      lib
      pkgs
      llvmPackages
      ;
  };
  toolchain = import ./toolchains {
    inherit
      lib
      pkgs
      llvmPackages
      epoch
      ;
  };
  projectPackages = import ./packages {
    inherit
      lib
      pkgs
      llvmPackages
      cmakeOptions
      mkMetafluxPackage
      source
      toolchain
      ;
  };
  projectChecks = import ./checks {
    inherit
      lib
      pkgs
      llvmPackages
      cmakeOptions
      mkMetafluxPackage
      projectPackages
      source
      ;
  };
  projectShells = import ./shells {
    inherit
      pkgs
      llvmPackages
      projectPackages
      ;
  };
in
assert lib.assertMsg (llvmPackages.llvm.version == epoch.llvm_version)
  "MetaFlux compiler epoch ${toString epoch.epoch} requires LLVM ${epoch.llvm_version}, but ${epoch.nixpkgs_package} provides ${llvmPackages.llvm.version}";
assert lib.assertMsg (
  lockedNixpkgs.rev == epoch.nixpkgs_revision
) "MetaFlux compiler epoch ${toString epoch.epoch} does not match the locked nixpkgs revision";
assert lib.assertMsg (
  lockedNixpkgs.narHash == epoch.nixpkgs_nar_hash
) "MetaFlux compiler epoch ${toString epoch.epoch} does not match the locked nixpkgs hash";
{
  packages.${system} = projectPackages // {
    default = projectPackages.runtime;
  };

  checks.${system} = projectChecks;
  devShells.${system} = projectShells;
  formatter.${system} = pkgs.nixfmt;
}
