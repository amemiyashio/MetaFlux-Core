#!/usr/bin/env bash
# Build the batch-0002 qualification kernel from the pinned linux-debug source.
#
# Produces an x86_64 bzImage with KASAN, kmemleak, lockdep, and KUnit enabled
# plus module support, so the real metaflux_core.ko and its KUnit suite can be
# loaded and soaked inside a QEMU guest without rebooting the host. The kernel
# build is owned by Kbuild; Nix only materializes the pinned source and tools.
#
# Usage: tools/build-debug-kernel.sh [--cache-dir DIR] [--jobs N]
# Requires METAFLUX_LINUX_SRC (nix develop .#linux-debug).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

CACHE_DIR="${REPO_DIR}/tmp/build/debug-kernel"
JOBS="$(nproc)"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --cache-dir) CACHE_DIR="$2"; shift 2 ;;
    --jobs) JOBS="$2"; shift 2 ;;
    *) echo "Unknown argument: $1" >&2; exit 1 ;;
  esac
done

: "${METAFLUX_LINUX_SRC:?METAFLUX_LINUX_SRC must point at the pinned linux source (nix develop .#linux-debug)}"
SRC_ROOT="$(realpath "${METAFLUX_LINUX_SRC}")"
test -f "${SRC_ROOT}/Makefile" || { echo "ERROR: no kernel Makefile at ${SRC_ROOT}" >&2; exit 1; }

OVERLAY="${CACHE_DIR}/src"
BUILD_DIR="${CACHE_DIR}/build"
mkdir -p "${CACHE_DIR}"

# --- Writable source overlay (symlinks; the store source is read-only) ---
if [[ -d "${OVERLAY}" ]]; then
  chmod -R u+w "${OVERLAY}" 2>/dev/null || true
  rm -rf "${OVERLAY}"
fi
mkdir -p "${OVERLAY}"
for entry in "${SRC_ROOT}"/*; do
  ln -s "${entry}" "${OVERLAY}/$(basename "${entry}")"
done

# --- Configuration: defconfig + qualification options via scripts/config ---
mkdir -p "${BUILD_DIR}"

make -C "${OVERLAY}" O="${BUILD_DIR}" ARCH=x86_64 defconfig
# Set/drop options through scripts/config: a "# CONFIG_x is not set"
# fragment line appended to .config would be ignored by olddefconfig.
"${SRC_ROOT}/scripts/config" --file "${BUILD_DIR}/.config" \
  -e KUNIT -e KASAN -e KASAN_GENERIC -e KCSAN -e DEBUG_KMEMLEAK \
  -e PROVE_LOCKING -e MODULES -e DEVTMPFS -e DEVTMPFS_MOUNT \
  -e SERIAL_8250 -e SERIAL_8250_CONSOLE -e DEBUG_INFO \
  -d SYSTEM_TRUSTED_KEYRING -d MODULE_SIG
make -C "${OVERLAY}" O="${BUILD_DIR}" ARCH=x86_64 olddefconfig

echo "--- Qualification config check ---"
MISSING=0
for cfg in CONFIG_KUNIT CONFIG_KASAN CONFIG_DEBUG_KMEMLEAK CONFIG_PROVE_LOCKING CONFIG_MODULES CONFIG_DEVTMPFS_MOUNT; do
  value="$(grep -E "^${cfg}=" "${BUILD_DIR}/.config" || true)"
  if [[ -z "${value}" ]]; then
    echo "  DROPPED: ${cfg} ($(grep "# ${cfg} " "${BUILD_DIR}/.config" || echo 'dependency unsatisfied'))" >&2
    MISSING=1
  else
    echo "  ${value}"
  fi
done
if [[ "${MISSING}" -ne 0 ]]; then
  echo "WARNING: some qualification configs were dropped by Kconfig dependencies" >&2
fi

# --- Build ---
make -C "${OVERLAY}" O="${BUILD_DIR}" ARCH=x86_64 -j"${JOBS}" bzImage modules

test -f "${BUILD_DIR}/arch/x86_64/boot/bzImage" || { echo "ERROR: bzImage missing" >&2; exit 1; }
echo "Debug kernel ready: ${BUILD_DIR}/arch/x86_64/boot/bzImage"
echo "Build tree: ${BUILD_DIR}"
