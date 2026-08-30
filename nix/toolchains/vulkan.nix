{
  lib,
  pkgs,
}:
let
  manifest = builtins.fromJSON (builtins.readFile ../../toolchains/vulkan-1.json);
  packages = {
    vulkan_headers = pkgs.vulkan-headers;
    vulkan_loader = pkgs.vulkan-loader;
    vulkan_tools = pkgs.vulkan-tools;
    glslang = pkgs.glslang;
    spirv_tools = pkgs.spirv-tools;
  };
  manifestPackages = lib.attrNames manifest.packages;
  actualVersions = lib.mapAttrs (_: package: package.version) packages;
  expectedVersions = lib.mapAttrs (_: package: package.version) manifest.packages;
in
assert lib.assertMsg (manifest.schema_version == 1 && manifest.epoch == 1)
  "unsupported Vulkan tool manifest epoch";
assert lib.assertMsg (actualVersions == expectedVersions)
  "Vulkan tool versions do not match toolchains/vulkan-1.json";
pkgs.buildEnv {
  name = "metaflux-vulkan-tools-${manifest.packages.vulkan_loader.version}";
  paths = lib.attrValues packages;
  pathsToLink = [
    "/bin"
    "/include"
    "/lib"
    "/share"
  ];
  ignoreCollisions = false;
  passthru = { inherit manifest packages; };
  postBuild = ''
    mkdir -p "$out/share/metaflux/vulkan"
    cp ${../../toolchains/vulkan-1.json} "$out/share/metaflux/vulkan/vulkan-1.json"
  '';
  meta = {
    description = "Pinned Vulkan 1.3 compute headers, loader, and validation tools";
    platforms = [ "x86_64-linux" ];
  };
}
