{
  description = "MetaFlux Core development and build environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";
  };

  outputs =
    inputs@{
      self,
      nixpkgs,
      ...
    }:
    import ./nix {
      inherit self nixpkgs;
    };
}
