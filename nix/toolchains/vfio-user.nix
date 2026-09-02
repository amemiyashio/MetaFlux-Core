{
  lib,
  pkgs,
}:
let
  manifest = builtins.fromJSON (builtins.readFile ../../toolchains/vfio-user-1.json);
  packages = {
    qemu = pkgs.qemu;
    socat = pkgs.socat;
  };
  manifestPackages = lib.attrNames manifest.packages;
  actualVersions = lib.mapAttrs (_: package: package.version) packages;
  expectedVersions = lib.mapAttrs (_: package: package.version) manifest.packages;
in
assert lib.assertMsg (manifest.schema_version == 1 && manifest.epoch == 1)
  "unsupported vfio-user tool manifest epoch";
assert lib.assertMsg (lib.attrNames packages == manifestPackages)
  "vfio-user tool package set does not match toolchains/vfio-user-1.json";
assert lib.assertMsg (actualVersions == expectedVersions)
  "vfio-user tool versions do not match toolchains/vfio-user-1.json";
pkgs.buildEnv {
  name = "metaflux-vfio-user-tools-${manifest.packages.qemu.version}";
  paths = lib.attrValues packages;
  pathsToLink = [
    "/bin"
    "/share"
  ];
  ignoreCollisions = false;
  passthru = { inherit manifest packages; };
  postBuild = ''
    mkdir -p "$out/share/metaflux/vfio-user"
    cp ${../../toolchains/vfio-user-1.json} "$out/share/metaflux/vfio-user/vfio-user-1.json"
  '';
  meta = {
    description = "Pinned QEMU and Unix socket tools for MetaFlux vfio-user qualification";
    platforms = [ "x86_64-linux" ];
  };
}
