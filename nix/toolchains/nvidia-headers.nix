{ pkgs }:
let
  lib = pkgs.lib;
  index = builtins.fromJSON (builtins.readFile ../../toolchains/nvidia-headers-1.json);
  manifestFiles = {
    R535 = ../../toolchains/nvidia-headers/r535.json;
    R550 = ../../toolchains/nvidia-headers/r550.json;
    R570 = ../../toolchains/nvidia-headers/r570.json;
    R580 = ../../toolchains/nvidia-headers/r580.json;
    R610 = ../../toolchains/nvidia-headers/r610.json;
  };
  manifests = lib.mapAttrs (_: path: builtins.fromJSON (builtins.readFile path)) manifestFiles;
  indexEntries = lib.listToAttrs (
    map (entry: lib.nameValuePair entry.driver_family entry) index.manifests
  );
  fetchedPackages = lib.concatMap (
    family:
    map (package: {
      inherit family package;
      source = pkgs.fetchurl {
        inherit (package) url sha256;
      };
    }) manifests.${family}.packages
  ) index.required_driver_families;
  extractCommands = lib.concatMapStringsSep "\n" (
    item:
    let
      licenseDestination = "share/licenses/${item.package.name}.copyright";
      headerCommands = lib.concatMapStringsSep "\n" (header: ''
        install -m 0644 \
          "$work/${header.path}" \
          "$out/families/${item.family}/include/${header.install_name}"
        printf '%s  %s\n' \
          '${header.sha256}' \
          "$out/families/${item.family}/include/${header.install_name}" \
          | sha256sum --check --strict
      '') item.package.headers;
    in
    ''
      work="$TMPDIR/${item.family}-${item.package.name}"
      mkdir -p "$work" \
        "$out/families/${item.family}/include" \
        "$out/families/${item.family}/share/licenses"
      dpkg-deb -x ${item.source} "$work"
      ${headerCommands}
      install -m 0644 \
        "$work/${item.package.license.path}" \
        "$out/families/${item.family}/${licenseDestination}"
      printf '%s  %s\n' \
        '${item.package.license.sha256}' \
        "$out/families/${item.family}/${licenseDestination}" \
        | sha256sum --check --strict
    ''
  ) fetchedPackages;
in
assert lib.assertMsg (index.schema_version == 2) "unsupported NVIDIA header index schema";
assert lib.assertMsg (lib.all
  (
    family:
    manifests.${family}.driver_family == family
    && builtins.hashFile "sha256" manifestFiles.${family} == indexEntries.${family}.sha256
  )
  index.required_driver_families
) "NVIDIA header manifest identity does not match nvidia-headers-1.json";
pkgs.runCommand "metaflux-nvidia-header-matrix-1"
  {
    nativeBuildInputs = [ pkgs.dpkg ];
    passthru = {
      inherit index manifests;
    };
  }
  ''
    mkdir -p "$out/share/metaflux/nvidia-headers"
    ${extractCommands}
    ln -s families/R610/include "$out/include"
    cp ${../../toolchains/nvidia-headers-1.json} \
      "$out/share/metaflux/nvidia-headers-1.json"
    cp ${../../toolchains/nvidia-headers/README.md} \
      "$out/share/metaflux/nvidia-headers/README.md"
    cp ${../../toolchains/nvidia-headers/r535.json} \
      ${../../toolchains/nvidia-headers/r550.json} \
      ${../../toolchains/nvidia-headers/r570.json} \
      ${../../toolchains/nvidia-headers/r580.json} \
      ${../../toolchains/nvidia-headers/r610.json} \
      "$out/share/metaflux/nvidia-headers/"
  ''
