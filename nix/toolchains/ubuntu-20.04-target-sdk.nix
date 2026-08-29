{
  lib,
  pkgs,
}:
let
  canonicalUbuntu = "https://archive.ubuntu.com/ubuntu";
  fetchDeb =
    {
      path,
      hash,
    }:
    pkgs.fetchurl {
      url = "${canonicalUbuntu}/${path}";
      inherit hash;
    };

  packageSpecs = [
    {
      name = "libc6";
      path = "pool/main/g/glibc/libc6_2.31-0ubuntu9.18_amd64.deb";
      hash = "sha256-GyKBqsSTXf6h+J38GeRF/btFMDICr2BnnMuL8DXwgaA=";
    }
    {
      name = "libc6-dev";
      path = "pool/main/g/glibc/libc6-dev_2.31-0ubuntu9.18_amd64.deb";
      hash = "sha256-EQKZAPIxW5POSHeSrhvxXwkn5eUIgMCR5qRAsU1OFko=";
    }
    {
      name = "linux-libc-dev";
      path = "pool/main/l/linux/linux-libc-dev_5.4.0-26.30_amd64.deb";
      hash = "sha256-p9WUIBNKgwfrEe95to4rNcrceUpg+CyH9Fg+N8dj/QE=";
    }
    {
      name = "gcc-10-base";
      path = "pool/main/g/gcc-10/gcc-10-base_10.5.0-1ubuntu1~20.04_amd64.deb";
      hash = "sha256-j6wGeRBXsbumF4Rm4WC7PsKnlSl/EOiONNr3Ylcu1cA=";
    }
    {
      name = "libgcc-s1";
      path = "pool/main/g/gcc-10/libgcc-s1_10.5.0-1ubuntu1~20.04_amd64.deb";
      hash = "sha256-Sqe5yfMiXfZadQrg/1yJD8YQjIKmCbPFq9RdIRg4vzw=";
    }
    {
      name = "libgcc-10-dev";
      path = "pool/main/g/gcc-10/libgcc-10-dev_10.5.0-1ubuntu1~20.04_amd64.deb";
      hash = "sha256-6+XpZ/yjLqWBGUYYD3gi7nSYV4YeraopUU5lhXzjuJc=";
    }
    {
      name = "libstdc++6";
      path = "pool/main/g/gcc-10/libstdc++6_10.5.0-1ubuntu1~20.04_amd64.deb";
      hash = "sha256-f5IiNC01UdBjv2UTGew5fDknju65q1lQrg6MKO8K9DE=";
    }
    {
      name = "libstdc++-10-dev";
      path = "pool/universe/g/gcc-10/libstdc++-10-dev_10.5.0-1ubuntu1~20.04_amd64.deb";
      hash = "sha256-EGrkcMmzR4Xxsf8G+mM9CJnI3UFZeQVarYiZuUnKJJE=";
    }
    {
      name = "zlib1g";
      path = "pool/main/z/zlib/zlib1g_1.2.11.dfsg-2ubuntu1.5_amd64.deb";
      hash = "sha256-v2cBj1MDRm60aGgLY3pdPzuxe51E3s89gtQLNbq80+A=";
    }
    {
      name = "zlib1g-dev";
      path = "pool/main/z/zlib/zlib1g-dev_1.2.11.dfsg-2ubuntu1.5_amd64.deb";
      hash = "sha256-uwNrFGaPE8Nyk3Bzkm1D/sYSmuc9Zsfhmy6HWVsVZ9g=";
    }
  ];
  packages = map (spec: fetchDeb { inherit (spec) path hash; }) packageSpecs;
  recipeSha256 = builtins.hashFile "sha256" ./ubuntu-20.04-target-sdk.nix;
  packageSetSha256 = builtins.hashString "sha256" (builtins.toJSON packageSpecs);
  buildIdentity = builtins.hashString "sha256" (
    builtins.toJSON {
      schema = 1;
      distribution = "ubuntu-20.04";
      architecture = "x86_64";
      inherit packageSetSha256 recipeSha256;
    }
  );
  packageManifest = lib.concatMapStringsSep "\n" (spec: ''
    package.${spec.name}.path=${spec.path}
    package.${spec.name}.hash=${spec.hash}
  '') packageSpecs;
in
pkgs.runCommand "metaflux-ubuntu-20.04-target-sdk"
  {
    nativeBuildInputs = [
      pkgs.coreutils
      pkgs.dpkg
      pkgs.findutils
    ];
    passthru = {
      distribution = "ubuntu-20.04";
      targetTriple = "x86_64-unknown-linux-gnu";
      glibcVersion = "2.31-0ubuntu9.18";
      gccVersion = "10.5.0-1ubuntu1~20.04";
      zlibVersion = "1.2.11.dfsg-2ubuntu1.5";
      gccToolchainRelative = "usr";
      inherit
        buildIdentity
        packageSetSha256
        packageSpecs
        recipeSha256
        ;
    };
  }
  ''
    mkdir -p "$out"
    for package in ${lib.escapeShellArgs packages}; do
      test "$(dpkg-deb --field "$package" Architecture)" = amd64
      dpkg-deb --extract "$package" "$out"
    done

    # Debian development packages use root-relative linker symlinks. Rewrite
    # them so this extracted tree remains a self-contained compiler sysroot.
    while IFS= read -r link; do
      target="$(readlink "$link")"
      rootedTarget="$out$target"
      if [[ ! -e "$rootedTarget" ]]; then
        echo "target SDK symlink target is absent: $link -> $target" >&2
        exit 1
      fi
      relativeTarget="$(realpath --relative-to="$(dirname "$link")" "$rootedTarget")"
      ln -sfn "$relativeTarget" "$link"
    done < <(find "$out" -type l -lname '/*' -print)

    # libgcc-10-dev ships linker symlinks for optional shared runtimes whose
    # packages are outside this SDK. Keep their static archives, but remove the
    # dangling shared-library selectors so target links cannot select host copies.
    for optionalRuntime in asan atomic gomp itm lsan quadmath tsan ubsan; do
      unlink "$out/usr/lib/gcc/x86_64-linux-gnu/10/lib$optionalRuntime.so"
    done

    for required in \
      usr/include/stdio.h \
      usr/include/c++/10/vector \
      usr/include/x86_64-linux-gnu/c++/10/bits/c++config.h \
      usr/include/zlib.h \
      usr/lib/x86_64-linux-gnu/crt1.o \
      usr/lib/x86_64-linux-gnu/libc.so \
      usr/lib/x86_64-linux-gnu/libpthread.so \
      usr/lib/x86_64-linux-gnu/libz.a \
      usr/lib/gcc/x86_64-linux-gnu/10/crtbegin.o \
      usr/lib/gcc/x86_64-linux-gnu/10/libgcc.a \
      usr/lib/gcc/x86_64-linux-gnu/10/libgcc_eh.a \
      usr/lib/gcc/x86_64-linux-gnu/10/libstdc++.a; do
      test -e "$out/$required"
    done

    if brokenLink="$(find -L "$out" -type l -print -quit)" && [[ -n "$brokenLink" ]]; then
      echo "broken symlink remains in target SDK: $brokenLink" >&2
      exit 1
    fi
    if absoluteLink="$(find "$out" -type l -lname '/*' -print -quit)" && \
      [[ -n "$absoluteLink" ]]; then
      echo "absolute symlink remains in target SDK: $absoluteLink" >&2
      exit 1
    fi

    cat > "$out/.metaflux-target-sdk-manifest" <<'EOF'
    distribution=ubuntu-20.04
    architecture=x86_64
    target-triple=x86_64-unknown-linux-gnu
    glibc=2.31-0ubuntu9.18
    linux-libc-dev=5.4.0-26.30
    gcc=10.5.0-1ubuntu1~20.04
    zlib=1.2.11.dfsg-2ubuntu1.5
    provenance-origin=${canonicalUbuntu}/
    recipe-sha256=${recipeSha256}
    package-set-sha256=${packageSetSha256}
    build-identity=sha256-${buildIdentity}
    ${packageManifest}
    EOF

    if grep -F /nix/store/ "$out/.metaflux-target-sdk-manifest"; then
      echo "target SDK provenance manifest embeds a Nix store path" >&2
      exit 1
    fi
  ''
