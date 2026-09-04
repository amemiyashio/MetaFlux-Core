{
  pkgs,
  llvmPackages,
  toolPackages,
}:
let
  mkClangShell =
    {
      name,
      prependPackages ? [ ],
      packages ? [ ],
      extraShellHook ? "",
    }:
    pkgs.mkShell.override { stdenv = llvmPackages.stdenv; } {
      inherit name;
      NIX_NO_SELF_RPATH = 1;
      buildInputs = [ pkgs.stdenv.cc.cc.lib ];
      packages =
        prependPackages
        ++ [
          toolPackages.toolchain
          pkgs.git
        ]
        ++ packages;
      shellHook = ''
        export CMAKE_GENERATOR=Ninja
        export CC=clang
        export CXX=clang++
        export LDFLAGS="''${LDFLAGS:+$LDFLAGS }-fuse-ld=lld"
        export LD_LIBRARY_PATH="${toolPackages.toolchain}/lib''${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
        ${extraShellHook}
      '';
    };
in
{
  default = mkClangShell {
    name = "metaflux-core-tools";
    packages = [
      pkgs.ccache
      pkgs.gdb
      pkgs.strace
      # Frozen official CUDA/NVML headers for the compatibility acceptance
      # rows; the shell declares METAFLUX_NVIDIA_HEADER_DIR so the default
      # dev build never silently drops those qualification tests.
      toolPackages.provider-headers
    ];
    extraShellHook = ''
      : "''${METAFLUX_NVIDIA_HEADER_DIR:=${toolPackages.provider-headers}/families/R610/include}"
      export METAFLUX_NVIDIA_HEADER_DIR
    '';
  };

  provider = mkClangShell {
    name = "metaflux-provider-tools";
    packages = [
      toolPackages.provider-headers
      toolPackages.provider-sysroot
    ];
  };

  runtime = mkClangShell {
    name = "metaflux-runtime-tools";
  };

  release = mkClangShell {
    name = "metaflux-release-tools";
    packages = [
      toolPackages.generic-llvm-toolchain
      toolPackages.nvidia-stock-tools
      toolPackages.provider-headers
      toolPackages."ubuntu-20.04-target-sdk"
      pkgs.binutils
      pkgs.dpkg
      pkgs.file
      pkgs.gnutar
      pkgs.gzip
      pkgs.jq
      pkgs.patchelf
      pkgs.podman
      pkgs.rpm
    ];
  };

  pytorch-baseline = mkClangShell {
    name = "metaflux-pytorch-baseline-tools";
    prependPackages = [ toolPackages.pytorch-baseline ];
    extraShellHook = ''
      export PYTHONNOUSERSITE=1
      unset PYTHONHOME
      unset PYTHONPATH
      export LD_LIBRARY_PATH="${toolPackages.toolchain}/lib"
    '';
  };

  pytorch-frontier = mkClangShell {
    name = "metaflux-pytorch-frontier-tools";
    prependPackages = [ toolPackages.pytorch-frontier ];
    extraShellHook = ''
      export PYTHONNOUSERSITE=1
      unset PYTHONHOME
      unset PYTHONPATH
      export LD_LIBRARY_PATH="${toolPackages.toolchain}/lib"
    '';
  };

  vulkan = mkClangShell {
    name = "metaflux-vulkan-tools";
    prependPackages = [ toolPackages.vulkan-tools ];
    extraShellHook = ''
      export VULKAN_SDK="${toolPackages.vulkan-tools}"
      export CMAKE_PREFIX_PATH="${toolPackages.vulkan-tools}''${CMAKE_PREFIX_PATH:+:$CMAKE_PREFIX_PATH}"
      export LD_LIBRARY_PATH="${toolPackages.vulkan-tools}/lib:${toolPackages.toolchain}/lib''${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    '';
  };

  vulkan-runtime = mkClangShell {
    name = "metaflux-vulkan-runtime-tools";
    prependPackages = [
      toolPackages.vulkan-tools
      toolPackages.vulkan-runtime
    ];
    extraShellHook = ''
      export VULKAN_SDK="${toolPackages.vulkan-tools}"
      export VULKAN_RUNTIME="${toolPackages.vulkan-runtime}"
      export CMAKE_PREFIX_PATH="${toolPackages.vulkan-tools}''${CMAKE_PREFIX_PATH:+:$CMAKE_PREFIX_PATH}"
      export VK_LAYER_PATH="${toolPackages.vulkan-runtime}/share/vulkan/explicit_layer.d"
      export XDG_DATA_DIRS="${toolPackages.vulkan-runtime}/share''${XDG_DATA_DIRS:+:$XDG_DATA_DIRS}"
      export LD_LIBRARY_PATH="${toolPackages.vulkan-runtime}/lib:${toolPackages.vulkan-tools}/lib:${toolPackages.toolchain}/lib''${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    '';
  };

  vfio-user = mkClangShell {
    name = "metaflux-vfio-user-tools";
    prependPackages = [ toolPackages.vfio-user-tools ];
    packages = [
      # Static guest initramfs userspace for the work-item-0.1.1.3 live
      # bring-up qualification runner; enableStatic keeps it self-contained
      # in the guest (no host ELF interpreter).
      (pkgs.busybox.override { enableStatic = true; })
    ];
  };

  linux-debug = pkgs.mkShell {
    name = "metaflux-linux-debug-tools";
    packages = [
      toolPackages.linux-debug-tools
      pkgs.gcc
      pkgs.gnumake
      pkgs.bc
      pkgs.bison
      pkgs.flex
      pkgs.pkg-config
      # Kernel objtool's HOSTCC needs elfutils libelf (gelf.h with the
      # symshndx API); the standalone libelf package is too old.
      pkgs.elfutils
      # The x86_64 defconfig keeps the system keyring, so the kernel's
      # certs/extract-cert host tool needs OpenSSL headers.
      pkgs.openssl
    ];
    shellHook = ''
      export METAFLUX_LINUX_SRC="${toolPackages.linux-debug-tools}/src"
      # Add the kunit source tree to PATH so that kunit.py can be found
      # directly (e.g. $METAFLUX_LINUX_SRC/tools/testing/kunit/kunit.py).
      export PATH="$METAFLUX_LINUX_SRC/tools/testing/kunit''${PATH:+:$PATH}"
    '';
  };
}
