{
  nixpkgs,
  ...
}:
let
  system = "x86_64-linux";
  pkgs = import nixpkgs {
    inherit system;
  };
  lib = pkgs.lib;
  epoch = builtins.fromJSON (builtins.readFile ../toolchains/compiler-epoch-1.json);
  llvmMajor = builtins.head (lib.splitString "." epoch.llvm_version);
  llvmBasePackages = pkgs."llvmPackages_${llvmMajor}";
  llvmPatchSpec = builtins.head epoch.downstream_patches;
  llvmPatchRelative = lib.removePrefix "toolchains/" llvmPatchSpec.path;
  llvmPatch = ../toolchains + "/${llvmPatchRelative}";
  llvmPackages = llvmBasePackages.overrideScope (
    _final: previous: {
      llvm = previous.llvm.overrideAttrs (old: {
        patches = (old.patches or [ ]) ++ [ llvmPatch ];
        passthru = (old.passthru or { }) // {
          metafluxDownstreamPatches = epoch.downstream_patches;
        };
      });
    }
  );
  toolchain = import ./toolchains {
    inherit
      lib
      pkgs
      llvmPackages
      epoch
      ;
  };
  providerSysroot = import ./toolchains/ubuntu-20.04-sysroot.nix { inherit pkgs; };
  targetSdk = import ./toolchains/ubuntu-20.04-target-sdk.nix {
    inherit lib pkgs;
  };
  genericLlvmToolchain = import ./toolchains/generic-llvm-toolchain.nix {
    inherit
      lib
      pkgs
      llvmPackages
      epoch
      llvmPatch
      ;
    targetSdk = targetSdk;
  };
  providerHeaders = import ./toolchains/nvidia-headers.nix { inherit pkgs; };
  providerTools = import ./toolchains/nvidia-tools.nix { inherit pkgs; };
  vulkanTools = import ./toolchains/vulkan.nix {
    inherit lib pkgs;
  };
  pytorchBaseline = import ./toolchains/pytorch-cuda-client.nix {
    inherit lib pkgs;
    profileName = "baseline";
  };
  pytorchFrontier = import ./toolchains/pytorch-cuda-client.nix {
    inherit lib pkgs;
    profileName = "frontier";
  };
  toolPackages = {
    inherit toolchain;
    generic-llvm-toolchain = genericLlvmToolchain;
    provider-headers = providerHeaders;
    provider-sysroot = providerSysroot;
    "ubuntu-20.04-target-sdk" = targetSdk;
    nvidia-stock-tools = providerTools;
    vulkan-tools = vulkanTools;
    pytorch-baseline = pytorchBaseline;
    pytorch-frontier = pytorchFrontier;
  };
  projectShells = import ./shells {
    inherit
      pkgs
      llvmPackages
      toolPackages
      ;
  };
in
assert lib.assertMsg (llvmPackages.llvm.version == epoch.llvm_version)
  "MetaFlux compiler epoch ${toString epoch.epoch} requires LLVM ${epoch.llvm_version}, but llvmPackages_${llvmMajor} provides ${llvmPackages.llvm.version}";
assert lib.assertMsg (
  builtins.length epoch.downstream_patches == 1
) "MetaFlux compiler epoch ${toString epoch.epoch} must name its exact downstream patchset";
assert lib.assertMsg (
  builtins.match "toolchains/patches/[A-Za-z0-9._/-]+[.]patch" llvmPatchSpec.path != null
) "MetaFlux compiler epoch ${toString epoch.epoch} patch path must stay under toolchains/patches";
assert lib.assertMsg (
  builtins.hashFile "sha256" llvmPatch == llvmPatchSpec.sha256
) "MetaFlux compiler epoch ${toString epoch.epoch} patch digest does not match its manifest";
{
  packages.${system} = toolPackages // {
    default = toolchain;
  };
  devShells.${system} = projectShells;
  formatter.${system} = pkgs.nixfmt;
}
