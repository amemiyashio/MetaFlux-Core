#!/usr/bin/env bash
# Build a generic Linux release targeting Ubuntu 20.04/glibc 2.31.
#
# This script resolves the Nix-materialized target SDK and generic LLVM
# toolchain, then invokes CMake with the target toolchain file.  It produces
# a build tree suitable for packaging with packaging/build.py.
#
# Usage: tools/build-generic-release.sh [--build-dir DIR] [--jobs N]
#
# Requirements:
#   - Nix with the MetaFlux flake available
#   - cmake, ninja on PATH (or inside nix develop)
#   - Clean Git worktree (the build embeds the HEAD revision)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

BUILD_DIR="${REPO_DIR}/../.metaflux-build/MetaFlux-Core/generic-release"
JOBS=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-dir) BUILD_DIR="$2"; shift 2 ;;
    --jobs) JOBS="$2"; shift 2 ;;
    *) echo "Unknown argument: $1" >&2; exit 1 ;;
  esac
done

# --- Resolve Nix store paths ---
echo "Resolving Nix materializations..."
SDK_PATH="$(nix build --no-link --print-out-paths "${REPO_DIR}#packages.x86_64-linux.\"ubuntu-20.04-target-sdk\"" 2>/dev/null)"
GENERIC_PATH="$(nix build --no-link --print-out-paths "${REPO_DIR}#packages.x86_64-linux.generic-llvm-toolchain" 2>/dev/null)"
TOOLCHAIN_PATH="$(nix build --no-link --print-out-paths "${REPO_DIR}#packages.x86_64-linux.toolchain" 2>/dev/null)"
VULKAN_PATH="$(nix build --no-link --print-out-paths "${REPO_DIR}#packages.x86_64-linux.vulkan-tools" 2>/dev/null)"
RAW_CLANG="$(nix build --no-link --print-out-paths "${REPO_DIR}#packages.x86_64-linux.generic-llvm-toolchain.rawCompiler" 2>/dev/null)"
RESOURCE_DIR="$(nix eval --raw "${REPO_DIR}#packages.x86_64-linux.generic-llvm-toolchain.resourceDir" 2>/dev/null)"

echo "  Target SDK:     ${SDK_PATH}"
echo "  Generic LLVM:   ${GENERIC_PATH}"
echo "  Toolchain:      ${TOOLCHAIN_PATH}"
echo "  Vulkan SDK:     ${VULKAN_PATH}"
echo "  Raw Clang:      ${RAW_CLANG}"
echo "  Resource Dir:   ${RESOURCE_DIR}"

# --- Verify materializations ---
test -f "${SDK_PATH}/.metaflux-target-sdk-manifest" || { echo "ERROR: SDK manifest missing" >&2; exit 1; }
test -f "${GENERIC_PATH}/.metaflux-generic-llvm-toolchain" || { echo "ERROR: generic LLVM manifest missing" >&2; exit 1; }
test -x "${GENERIC_PATH}/bin/ld.lld" || { echo "ERROR: ld.lld missing" >&2; exit 1; }
test -x "${RAW_CLANG}/bin/clang" || { echo "ERROR: raw clang missing" >&2; exit 1; }
test -f "${VULKAN_PATH}/include/vulkan/vulkan.h" || { echo "ERROR: vulkan headers missing" >&2; exit 1; }

# --- Clear host-wrapper flags that could leak host paths ---
unset CFLAGS CXXFLAGS CPPFLAGS LDFLAGS 2>/dev/null || true
unset NIX_CFLAGS_COMPILE NIX_CFLAGS_LINK NIX_LDFLAGS 2>/dev/null || true

# --- Export environment for toolchain file ---
export METAFLUX_TARGET_SDK="${SDK_PATH}"
export METAFLUX_GENERIC_LLVM="${GENERIC_PATH}"
export METAFLUX_TOOLCHAIN="${TOOLCHAIN_PATH}"
export METAFLUX_RAW_CLANG="${RAW_CLANG}"
export METAFLUX_RESOURCE_DIR="${RESOURCE_DIR}"

# --- Configure ---
echo "Configuring generic-release build at ${BUILD_DIR}..."
cmake -S "${REPO_DIR}" -B "${BUILD_DIR}" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="${REPO_DIR}/cmake/toolchains/ubuntu-20.04-generic.cmake" \
  -DCMAKE_BUILD_TYPE=Release \
  -DMETAFLUX_ENABLE_WERROR=ON \
  -DMETAFLUX_USE_LLD=ON \
  -DMETAFLUX_ENABLE_LTO=ON \
  -DMETAFLUX_COMPILER_LINK_SHARED_LLVM=OFF \
  -DMETAFLUX_RUNTIME_LLD_PATH=/usr/libexec/metaflux/ld.lld \
  -DMETAFLUX_BUILD_TESTS=OFF \
  -DBUILD_TESTING=OFF \
  -DMETAFLUX_BUILD_VULKAN_BACKEND=ON \
  -DMETAFLUX_VULKAN_BACKEND_SHARED=ON \
  -DMETAFLUX_VULKAN_SDK_DIR="${VULKAN_PATH}"

# --- Build ---
BUILD_JOBS="${JOBS:-$(nproc)}"
echo "Building with ${BUILD_JOBS} parallel jobs..."
cmake --build "${BUILD_DIR}" --parallel "${BUILD_JOBS}"

# --- Verify glibc ceiling ---
echo ""
echo "Verifying glibc symbol ceiling..."
MAX_GLIBC=""
for elf in $(find "${BUILD_DIR}" -name "*.so*" -o -name "metafluxd" 2>/dev/null | head -50); do
  if file "$elf" | grep -q "ELF"; then
    glibc_ver="$(readelf -V "$elf" 2>/dev/null | grep -oP 'GLIBC_\K[0-9]+\.[0-9]+' | sort -V | tail -1)"
    if [[ -n "$glibc_ver" ]]; then
      if [[ -z "$MAX_GLIBC" ]] || [[ "$(printf '%s\n' "$glibc_ver" "$MAX_GLIBC" | sort -V | tail -1)" == "$glibc_ver" ]]; then
        MAX_GLIBC="$glibc_ver"
      fi
    fi
  fi
done
echo "  Maximum glibc symbol version: GLIBC_${MAX_GLIBC}"

# Check against floor
FLOOR_MAJOR=2
FLOOR_MINOR=31
MAX_MAJOR="${MAX_GLIBC%%.*}"
MAX_MINOR="${MAX_GLIBC##*.}"
if [[ "$MAX_MAJOR" -gt "$FLOOR_MAJOR" ]] || [[ "$MAX_MAJOR" -eq "$FLOOR_MAJOR" && "$MAX_MINOR" -gt "$FLOOR_MINOR" ]]; then
  echo "  WARNING: GLIBC_${MAX_GLIBC} exceeds the 2.31 floor!" >&2
else
  echo "  OK: within glibc 2.31 floor"
fi

echo ""
echo "Generic release build complete: ${BUILD_DIR}"
echo "To package: python3 packaging/build.py --build-dir ${BUILD_DIR} --output-dir <OUT> --kind complete --target-sdk ${SDK_PATH} --generic-toolchain ${GENERIC_PATH}"
