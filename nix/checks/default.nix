{
  lib,
  pkgs,
  llvmPackages,
  cmakeOptions,
  mkMetafluxPackage,
  projectPackages,
  source,
}:
let
  allBuildFlags = cmakeOptions {
    runtimeCore = true;
    clientFastpath = true;
    cudaDriverProvider = true;
    nvmlProvider = true;
    cudaPtxFrontend = true;
    daemon = true;
    compiler = true;
    cpuBackendRuntime = true;
    tests = true;
  };

  testSuite = mkMetafluxPackage {
    pname = "metaflux-check-test-suite";
    src = source.tests;
    cmakeFlags = allBuildFlags;
    checkCommand = ''
      ctest --test-dir build --output-on-failure --no-tests=error
    '';
    extraBuildInputs = [
      llvmPackages.llvm
      llvmPackages.mlir
    ];
    extraNativeBuildInputs = [ pkgs.python3 ];
  };

  format = mkMetafluxPackage {
    pname = "metaflux-check-format";
    src = source.format;
    cmakeFlags = allBuildFlags;
    buildTarget = "metaflux-format-check";
    checkCommand = ''
      mapfile -t nixSources < <(find . -path ./build -prune -o -name '*.nix' -type f -print)
      nixfmt --check "''${nixSources[@]}"
    '';
    extraNativeBuildInputs = [
      llvmPackages.clang-tools
      pkgs.nixfmt
      pkgs.python3
    ];
  };

  providerClosure = pkgs.closureInfo {
    rootPaths = [ projectPackages.provider ];
  };

  release-closure =
    pkgs.runCommand "metaflux-check-release-closure"
      {
        nativeBuildInputs = [
          pkgs.binutils
          pkgs.file
          pkgs.findutils
          pkgs.gnugrep
        ];
      }
      ''
        mkdir -p "$out"

        if grep -E '/[a-z0-9]+-(clang|llvm|mlir|python[0-9]*|systemd|libcxx|libstdcxx|libatomic)(-|/|$)' \
          ${providerClosure}/store-paths; then
          echo "provider runtime closure contains a forbidden dependency" >&2
          exit 1
        fi

        while IFS= read -r candidate; do
          if file -b "$candidate" | grep -q '^ELF '; then
            dynamic="$TMPDIR/dynamic.$(basename "$candidate")"
            readelf -dW "$candidate" > "$dynamic" || true
            if grep -E 'NEEDED.*(libstdc\+\+|libc\+\+|libLLVM|libMLIR|libpython|libsystemd|libatomic)' "$dynamic"; then
              echo "forbidden DT_NEEDED entry in $candidate" >&2
              exit 1
            fi
          fi
        done < <(find ${projectPackages.provider} -type f)

        touch "$out/passed"
      '';

  runtime-sdk =
    pkgs.runCommand "metaflux-check-runtime-sdk"
      {
        buildInputs = [ pkgs.stdenv.cc.cc.lib ];
        nativeBuildInputs = [
          pkgs.cmake
          pkgs.ninja
          llvmPackages.clang
          llvmPackages.lld
        ];
      }
      ''
        export CC=clang
        export CXX=clang++
        export NIX_LDFLAGS="''${NIX_LDFLAGS:+$NIX_LDFLAGS }-rpath ${pkgs.stdenv.cc.cc.lib}/lib"
        cmake \
          -S ${source.tests}/tests/consumer/runtime \
          -B build \
          -G Ninja \
          -DCMAKE_PREFIX_PATH=${projectPackages.runtime} \
          -DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=lld
        cmake --build build
        build/metaflux_fastpath_consumer
        build/metaflux_runtime_consumer
        touch "$out"
      '';

  toolchain-sdk =
    pkgs.runCommand "metaflux-check-toolchain-sdk"
      {
        nativeBuildInputs = [
          pkgs.binutils
          projectPackages.toolchain
        ];
      }
      ''
        test -x ${projectPackages.toolchain}/bin/llvm-config
        test -f ${projectPackages.toolchain}/include/llvm/IR/Module.h
        test -f ${projectPackages.toolchain}/include/mlir/IR/MLIRContext.h
        test -f ${projectPackages.toolchain}/lib/cmake/llvm/LLVMConfig.cmake
        test -f ${projectPackages.toolchain}/lib/cmake/mlir/MLIRConfig.cmake

        builtTargets="$(${projectPackages.toolchain}/bin/llvm-config --targets-built)"
        for requiredTarget in ${lib.escapeShellArgs projectPackages.toolchain.compilerEpoch.required_targets}; do
          if ! grep -qw "$requiredTarget" <<< "$builtTargets"; then
            echo "LLVM target $requiredTarget is absent from: $builtTargets" >&2
            exit 1
          fi
        done

        cmake \
          -S ${source.tests}/tests/consumer/toolchain \
          -B build \
          -G Ninja \
          -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_CXX_COMPILER=${projectPackages.toolchain}/bin/clang++ \
          -DCMAKE_PREFIX_PATH=${projectPackages.toolchain}
        cmake --build build
        build/metaflux_toolchain_consumer
        readelf -dW build/metaflux_toolchain_consumer | grep -F 'RUNPATH'

        printf '%s\n' 'int main(void) { return 0; }' > compiler-probe.c
        ${projectPackages.toolchain}/bin/clang compiler-probe.c -o compiler-probe
        ./compiler-probe
        touch "$out"
      '';

  agent-records =
    pkgs.runCommand "metaflux-check-agent-records"
      {
        nativeBuildInputs = [ pkgs.python3 ];
      }
      ''
        ${pkgs.python3}/bin/python3 \
          ${source.agentRecords}/tools/check-agent-records.py \
          ${source.agentRecords}
        ${pkgs.python3}/bin/python3 -B \
          ${source.agentRecords}/tools/check-skill-routing.py \
          ${source.agentRecords}
        ${pkgs.python3}/bin/python3 -B \
          ${source.agentRecords}/tools/test-check-skill-routing.py
        touch "$out"
      '';

  entry-points =
    pkgs.runCommand "metaflux-check-entry-points"
      {
        nativeBuildInputs = [
          pkgs.python3
          pkgs.gitMinimal
        ];
      }
      ''
        cp -r ${source.agentRecords} repo
        chmod -R u+w repo
        git init -q repo
        ${pkgs.python3}/bin/python3 repo/tools/check-agent-records.py repo
        test -s repo/AGENTS.md
        test -L repo/.agents/skills
        test "$(readlink repo/.agents/skills)" = "../agent/skills"
        test -f repo/.agents/skills/start-work/SKILL.md
        test -s repo/CLAUDE.md
        grep -q '^@AGENTS.md' repo/CLAUDE.md
        test -x repo/.githooks/pre-commit
        test -s repo/.claude/settings.json
        test -f repo/.claude/hooks/pre_edit.py
        test -f repo/.claude/hooks/session_start.py
        touch "$out"
      '';
in
{
  inherit
    agent-records
    entry-points
    format
    release-closure
    runtime-sdk
    toolchain-sdk
    ;
  abi = testSuite;
  integration = testSuite;
  performance-smoke = testSuite;
  unit = testSuite;
}
