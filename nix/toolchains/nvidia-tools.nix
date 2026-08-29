{ pkgs }:
let
  lib = pkgs.lib;
  manifest = builtins.fromJSON (builtins.readFile ../../toolchains/nvidia-tools-1.json);
  headerManifests = {
    R535 = builtins.fromJSON (builtins.readFile ../../toolchains/nvidia-headers/r535.json);
    R550 = builtins.fromJSON (builtins.readFile ../../toolchains/nvidia-headers/r550.json);
    R570 = builtins.fromJSON (builtins.readFile ../../toolchains/nvidia-headers/r570.json);
    R580 = builtins.fromJSON (builtins.readFile ../../toolchains/nvidia-headers/r580.json);
    R610 = builtins.fromJSON (builtins.readFile ../../toolchains/nvidia-headers/r610.json);
  };
  manifestRowsAgree = lib.all (
    entry:
    let
      headers = headerManifests.${entry.driver_family};
      expectedManifest = "toolchains/nvidia-headers/${lib.toLower entry.driver_family}.json";
    in
    headers.schema_version == 1
    && headers.driver_family == entry.driver_family
    && headers.representative_driver_version == entry.driver_version
    && headers.repository_snapshot.packages_sha256 == entry.packages_sha256
    && headers.repository_snapshot.signer_fingerprint == manifest.repository_signer_fingerprint
    && entry.repository_manifest == expectedManifest
  ) manifest.tools;
  fetchedTools = map (entry: {
    inherit entry;
    source = pkgs.fetchurl {
      inherit (entry.package) url sha256;
    };
  }) manifest.tools;
  extractCommands = lib.concatMapStringsSep "\n" (
    item:
    let
      family = item.entry.driver_family;
      binary = item.entry.binary;
      license = item.entry.license;
      buildIdCheck =
        if binary.gnu_build_id == null then
          ''
            if readelf -n "$out/families/${family}/bin/nvidia-smi" | grep -q 'Build ID:'; then
              echo "unexpected GNU build ID for ${family}" >&2
              exit 1
            fi
          ''
        else
          ''
            readelf -n "$out/families/${family}/bin/nvidia-smi" \
              | grep -F 'Build ID: ${binary.gnu_build_id}'
          '';
    in
    ''
      work="$TMPDIR/${family}-${item.entry.package.name}"
      mkdir -p "$work" \
        "$out/families/${family}/bin" \
        "$out/families/${family}/share/licenses"
      dpkg-deb -x ${item.source} "$work"
      test "$(stat -c %s ${item.source})" = '${toString item.entry.package.size}'
      install -m 0755 \
        "$work/${binary.path}" \
        "$out/families/${family}/bin/nvidia-smi"
      test "$(stat -c %s "$out/families/${family}/bin/nvidia-smi")" = \
        '${toString binary.size}'
      printf '%s  %s\n' \
        '${binary.sha256}' \
        "$out/families/${family}/bin/nvidia-smi" \
        | sha256sum --check --strict
      ${buildIdCheck}
      install -m 0644 \
        "$work/${license.path}" \
        "$out/families/${family}/share/licenses/nvidia-smi.copyright"
      printf '%s  %s\n' \
        '${license.sha256}' \
        "$out/families/${family}/share/licenses/nvidia-smi.copyright" \
        | sha256sum --check --strict
    ''
  ) fetchedTools;
in
assert lib.assertMsg (manifest.schema_version == 1) "unsupported NVIDIA tool manifest schema";
assert lib.assertMsg (
  map (entry: entry.driver_family) manifest.tools == manifest.required_driver_families
) "NVIDIA tool rows do not match the required family order";
assert lib.assertMsg manifestRowsAgree "NVIDIA tool and header manifests disagree";
pkgs.runCommand "metaflux-nvidia-stock-tools-1"
  {
    nativeBuildInputs = [
      pkgs.binutils
      pkgs.dpkg
    ];
    passthru = { inherit manifest; };
  }
  ''
    mkdir -p "$out/bin" "$out/share/metaflux/nvidia-tools"
    ${extractCommands}
    ln -s ../families/R610/bin/nvidia-smi "$out/bin/nvidia-smi"
    cp ${../../toolchains/nvidia-tools-1.json} \
      "$out/share/metaflux/nvidia-tools-1.json"
    cp ${../../toolchains/nvidia-tools/README.md} \
      "$out/share/metaflux/nvidia-tools/README.md"
  ''
