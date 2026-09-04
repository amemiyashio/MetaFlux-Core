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
# Unpack the linux source tarball into $out/src (not $out) so that the package
# root does not expose kernel-internal headers like include/linux/limits.h.
# Those headers leak onto the cc-wrapper include path when the package root is
# on the compiler search path, causing glibc headers to pull in kernel types
# (loff_t, dev_t, nlink_t) that conflict with glibc 2.42 + gcc 15.2.0.
pkgs.runCommand "metaflux-linux-debug-tools-${manifestVersion}" {
  nativeBuildInputs = [ pkgs.gnutar pkgs.xz ];
} ''
  mkdir -p "$out/src"
  tar -xJf ${linuxPkg.src} -C "$out/src" --strip-components=1
  # Verify kunit.py is present under src/
  if [[ ! -f "$out/src/tools/testing/kunit/kunit.py" ]]; then
    echo "ERROR: kunit.py not found in linux source" >&2
    exit 1
  fi
  # Verify no kernel-internal headers are at the package root (regression check
  # against the old unpack-to-$out layout).
  if [[ -d "$out/include" ]]; then
    echo "ERROR: package root must not contain include/ — unpack into \$out/src" >&2
    exit 1
  fi
  # Store manifest for downstream verification.
  mkdir -p "$out/share/metaflux/linux-debug"
  cp ${../../toolchains/linux-debug-1.json} "$out/share/metaflux/linux-debug/linux-debug-1.json"
  echo "metaflux-linux-debug-tools built: $out (source at $out/src)"
''
