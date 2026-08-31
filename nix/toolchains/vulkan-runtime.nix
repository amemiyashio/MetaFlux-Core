{
  lib,
  pkgs,
}:
let
  manifest = builtins.fromJSON (builtins.readFile ../../toolchains/vulkan-runtime-1.json);
  packages = {
    mesa = pkgs.mesa;
    vulkan_validation_layers = pkgs.vulkan-validation-layers;
  };
  manifestPackages = lib.attrNames manifest.packages;
  actualVersions = lib.mapAttrs (_: package: package.version) packages;
  expectedVersions = lib.mapAttrs (_: package: package.version) manifest.packages;
in
assert lib.assertMsg (
  manifest.schema_version == 1 && manifest.epoch == 1
) "unsupported Vulkan runtime tool manifest epoch";
assert lib.assertMsg (
  manifest.purpose == "vulkan-host-smoke"
) "Vulkan runtime profile must remain a host-smoke environment";
assert lib.assertMsg (
  manifestPackages == lib.attrNames actualVersions
) "Vulkan runtime manifest/package set diverged";
assert lib.assertMsg (
  actualVersions == expectedVersions
) "Vulkan runtime tool versions do not match toolchains/vulkan-runtime-1.json";
pkgs.buildEnv {
  name = "metaflux-vulkan-runtime-${manifest.packages.mesa.version}";
  paths = lib.attrValues packages;
  pathsToLink = [
    "/lib"
    "/share"
  ];
  ignoreCollisions = false;
  passthru = { inherit manifest packages; };
  postBuild = ''
    mkdir -p "$out/share/metaflux/vulkan"
    cp ${../../toolchains/vulkan-runtime-1.json} "$out/share/metaflux/vulkan/vulkan-runtime-1.json"
  '';
  meta = {
    description = "Pinned Mesa Vulkan ICD and validation layers for host smoke tests";
    platforms = [ "x86_64-linux" ];
  };
}
