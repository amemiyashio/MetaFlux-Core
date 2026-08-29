{ pkgs }:
let
  libc6 = pkgs.fetchurl {
    url = "https://archive.ubuntu.com/ubuntu/pool/main/g/glibc/libc6_2.31-0ubuntu9.18_amd64.deb";
    hash = "sha256-GyKBqsSTXf6h+J38GeRF/btFMDICr2BnnMuL8DXwgaA=";
  };
  libc6Dev = pkgs.fetchurl {
    url = "https://archive.ubuntu.com/ubuntu/pool/main/g/glibc/libc6-dev_2.31-0ubuntu9.18_amd64.deb";
    hash = "sha256-EQKZAPIxW5POSHeSrhvxXwkn5eUIgMCR5qRAsU1OFko=";
  };
  linuxLibcDev = pkgs.fetchurl {
    url = "https://archive.ubuntu.com/ubuntu/pool/main/l/linux/linux-libc-dev_5.4.0-26.30_amd64.deb";
    hash = "sha256-p9WUIBNKgwfrEe95to4rNcrceUpg+CyH9Fg+N8dj/QE=";
  };
in
pkgs.runCommand "metaflux-ubuntu-20.04-glibc-2.31-sysroot"
  {
    nativeBuildInputs = [ pkgs.dpkg ];
    passthru = {
      distribution = "ubuntu-20.04";
      glibcVersion = "2.31-0ubuntu9.18";
      linuxLibcVersion = "5.4.0-26.30";
    };
  }
  ''
    mkdir -p "$out"
    dpkg-deb --extract ${libc6} "$out"
    dpkg-deb --extract ${libc6Dev} "$out"
    dpkg-deb --extract ${linuxLibcDev} "$out"

    # Debian development packages use root-relative linker symlinks. Rewrite
    # them so the extracted tree remains a self-contained compiler sysroot.
    while IFS= read -r link; do
      target="$(readlink "$link")"
      rootedTarget="$out$target"
      if [[ ! -e "$rootedTarget" ]]; then
        echo "sysroot symlink target is absent: $link -> $target" >&2
        exit 1
      fi
      relativeTarget="$(realpath --relative-to="$(dirname "$link")" "$rootedTarget")"
      ln -sfn "$relativeTarget" "$link"
    done < <(find "$out" -type l -lname '/*' -print)

    if brokenLink="$(find -L "$out" -type l -print -quit)" && [[ -n "$brokenLink" ]]; then
      echo "broken symlink remains in provider sysroot: $brokenLink" >&2
      exit 1
    fi
    if absoluteLink="$(find "$out" -type l -lname '/*' -print -quit)" && [[ -n "$absoluteLink" ]]; then
      echo "absolute symlink remains in provider sysroot: $absoluteLink" >&2
      exit 1
    fi

    printf '%s\n' \
      'distribution=ubuntu-20.04' \
      'architecture=x86_64' \
      'glibc=2.31-0ubuntu9.18' \
      'linux-libc-dev=5.4.0-26.30' \
      > "$out/.metaflux-sysroot-manifest"
  ''
