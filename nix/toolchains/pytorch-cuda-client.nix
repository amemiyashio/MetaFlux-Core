{
  lib,
  pkgs,
  profileName,
}:
let
  index = builtins.fromJSON (builtins.readFile ../../toolchains/pytorch-cuda-clients-1.json);
  profile = index.profiles.${profileName};
  lockPath = ../../. + "/${profile.wheel_lock}";
  lock = builtins.fromJSON (builtins.readFile lockPath);
  python = pkgs.python313;
  installerPython = python.withPackages (packages: [ packages.pip ]);
  fetchedWheels = map (
    wheel:
    wheel
    // {
      source = pkgs.fetchurl {
        inherit (wheel) urls sha256;
        name = wheel.filename;
      };
    }
  ) lock.wheels;
  prepareWheels = lib.concatMapStringsSep "\n" (wheel: ''
    test "$(stat -c %s ${wheel.source})" = '${toString wheel.size}'
    ln -s ${wheel.source} "metaflux-wheels/${wheel.filename}"
  '') fetchedWheels;
  wheelArguments = lib.escapeShellArgs (
    map (wheel: "metaflux-wheels/${wheel.filename}") fetchedWheels
  );
in
assert lib.assertMsg (
  index.schema_version == 1 && index.epoch == 1
) "unsupported PyTorch CUDA client manifest epoch";
assert lib.assertMsg (index.python.version == python.version)
  "PyTorch CUDA clients require CPython ${index.python.version}, but nixpkgs provides ${python.version}";
assert lib.assertMsg (
  lock.schema_version == 1 && lock.profile == profileName
) "PyTorch CUDA wheel lock does not match profile ${profileName}";
pkgs.stdenvNoCC.mkDerivation {
  pname = "metaflux-pytorch-${profileName}";
  version = profile.torch_version;

  dontUnpack = true;
  dontStrip = true;
  strictDeps = true;
  nativeBuildInputs = [
    pkgs.addDriverRunpath
    pkgs.autoAddDriverRunpath
    pkgs.autoPatchelfHook
    pkgs.makeWrapper
  ];
  buildInputs = [
    pkgs.glibc
    pkgs.stdenv.cc.cc.lib
    pkgs.zlib
  ];
  autoPatchelfIgnoreMissingDeps = [
    "libcuda.so.1"
    "libnvidia-ml.so.1"
    # Optional cuFile/NVSHMEM transports use host fabric and MPI stacks.
    "libfabric.so.1"
    "libibverbs.so.1"
    "libmlx5.so.1"
    "libmpi.so.40"
    "liboshmem.so.40"
    "libpmix.so.2"
    "librdmacm.so.1"
    "libucp.so.0"
    "libucs.so.0"
  ];

  installPhase = ''
    runHook preInstall
    mkdir -p "$out"
    mkdir -p metaflux-wheels
    ${prepareWheels}
    PIP_ROOT_USER_ACTION=ignore ${installerPython}/bin/python -m pip install \
      --disable-pip-version-check \
      --no-cache-dir \
      --no-compile \
      --no-deps \
      --no-index \
      --prefix "$out" \
      ${wheelArguments}

    for executable in "$out/bin"/*; do
      test -e "$executable" || continue
      wrapProgram "$executable" \
        --set PYTHONNOUSERSITE 1 \
        --unset PYTHONPATH \
        --prefix PYTHONPATH : "$out/${python.sitePackages}"
    done
    makeWrapper ${python}/bin/python3 "$out/bin/python" \
      --set PYTHONNOUSERSITE 1 \
      --unset PYTHONPATH \
      --prefix PYTHONPATH : "$out/${python.sitePackages}"
    ln -s python "$out/bin/python3"
    ln -s python "$out/bin/python3.13"

    mkdir -p "$out/share/metaflux/pytorch-cuda-clients"
    cp ${../../toolchains/pytorch-cuda-clients-1.json} \
      "$out/share/metaflux/pytorch-cuda-clients/"
    cp ${lockPath} \
      "$out/share/metaflux/pytorch-cuda-clients/${profileName}-wheels.json"
    runHook postInstall
  '';

  passthru = {
    inherit index lock profile;
  };
  meta = {
    description = "Pinned MetaFlux ${profileName} PyTorch CUDA client";
    platforms = [ "x86_64-linux" ];
  };
}
