{
  lib,
  pkgs,
}:
let
  manifest = builtins.fromJSON (builtins.readFile ../../toolchains/linux-debug-1.json);
  linuxPkg = pkgs.linux_6_12;
  manifestVersion = manifest.packages.linux_6_12.version;
in
assert lib.assertMsg (manifest.schema_version == 1 && manifest.epoch == 1)
  "unsupported linux-debug tool manifest epoch";
assert lib.assertMsg (manifestVersion == linuxPkg.version)
  "linux-debug tool version ${linuxPkg.version} does not match toolchains/linux-debug-1.json (${manifestVersion})";
# Unpack the linux source tarball into a source-only output.  The tarball is
# ~140 MiB; unpacking is far cheaper than building the kernel and avoids
# encoding any CTest command in Nix.
pkgs.runCommand "metaflux-linux-debug-tools-${manifestVersion}" {
  nativeBuildInputs = [ pkgs.gnutar pkgs.xz ];
} ''
  mkdir -p "$out"
  tar -xJf ${linuxPkg.src} -C "$out" --strip-components=1
  # Verify kunit.py is present
  if [[ ! -f "$out/tools/testing/kunit/kunit.py" ]]; then
    echo "ERROR: kunit.py not found in linux source" >&2
    exit 1
  fi
  # Store manifest for downstream verification.
  mkdir -p "$out/share/metaflux/linux-debug"
  cp ${../../toolchains/linux-debug-1.json} "$out/share/metaflux/linux-debug/linux-debug-1.json"
  echo "metaflux-linux-debug-tools built: $out"
''
