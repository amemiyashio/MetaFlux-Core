{
  description = "Pinned MetaFlux Core development tools";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";
  };

  outputs =
    {
      nixpkgs,
      ...
    }:
    import ./nix {
      inherit nixpkgs;
    };
}
