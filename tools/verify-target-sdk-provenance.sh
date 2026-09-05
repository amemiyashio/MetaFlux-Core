#!/usr/bin/env bash
# Drive the signed Ubuntu archive provenance verifier for the target SDK.
#
# This script reads toolchains/ubuntu-20.04-target-sdk-provenance.json,
# downloads every InRelease, Packages index, and DEB it references from the
# recorded Ubuntu snapshot, locates the keyring and external tools, then
# invokes the Python verifier with the complete argument set.
#
# Usage:
#   tools/verify-target-sdk-provenance.sh \
#     --sdk-path <NIX_SDK_OUTPUT> \
#     --keyring <UBUNTU_ARCHIVE_KEYRING_GPG> \
#     [--staging-dir <DOWNLOAD_CACHE_DIR>] \
#     [--output <PROVENANCE_EVIDENCE_JSON>] \
#     [--manifest <PROVENANCE_JSON>] \
#     [--gpgv <GPGV_BINARY>] \
#     [--dpkg-deb <DPKG_DEB_BINARY>]
#
# Requirements:
#   - jq on PATH
#   - curl on PATH
#   - gpgv (or pass --gpgv)
#   - dpkg-deb (or pass --dpkg-deb)
#   - python3

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

# --- Defaults ---
MANIFEST="${REPO_DIR}/toolchains/ubuntu-20.04-target-sdk-provenance.json"
SDK_PATH=""
KEYRING=""
STAGING_DIR=""
OUTPUT=""
GPGV=""
DPKG_DEB=""

# --- Parse arguments ---
while [[ $# -gt 0 ]]; do
  case "$1" in
    --manifest)     MANIFEST="$2"; shift 2 ;;
    --sdk-path)     SDK_PATH="$2"; shift 2 ;;
    --keyring)      KEYRING="$2"; shift 2 ;;
    --staging-dir)  STAGING_DIR="$2"; shift 2 ;;
    --output)       OUTPUT="$2"; shift 2 ;;
    --gpgv)         GPGV="$2"; shift 2 ;;
    --dpkg-deb)     DPKG_DEB="$2"; shift 2 ;;
    -h|--help)
      sed -n '3,18p' "$0"
      exit 0
      ;;
    *)
      echo "Unknown argument: $1" >&2
      exit 1
      ;;
  esac
done

# --- Validate required arguments ---
if [[ -z "$SDK_PATH" ]]; then
  echo "ERROR: --sdk-path is required (Nix store path to ubuntu-20.04-target-sdk)" >&2
  exit 1
fi
if [[ -z "$KEYRING" ]]; then
  echo "ERROR: --keyring is required (path to ubuntu-archive-keyring.gpg)" >&2
  exit 1
fi

SDK_MANIFEST="${SDK_PATH}/.metaflux-target-sdk-manifest"
if [[ ! -f "$SDK_MANIFEST" ]]; then
  echo "ERROR: SDK manifest not found at ${SDK_MANIFEST}" >&2
  exit 1
fi
if [[ ! -f "$KEYRING" ]]; then
  echo "ERROR: keyring file not found at ${KEYRING}" >&2
  exit 1
fi
if [[ ! -f "$MANIFEST" ]]; then
  echo "ERROR: provenance manifest not found at ${MANIFEST}" >&2
  exit 1
fi

# --- Resolve external tools ---
if [[ -z "$GPGV" ]]; then
  GPGV="$(command -v gpgv 2>/dev/null || true)"
  if [[ -z "$GPGV" ]]; then
    echo "ERROR: gpgv not found on PATH; pass --gpgv explicitly" >&2
    exit 1
  fi
fi
if [[ ! -x "$GPGV" ]]; then
  echo "ERROR: gpgv is not executable: ${GPGV}" >&2
  exit 1
fi

if [[ -z "$DPKG_DEB" ]]; then
  DPKG_DEB="$(command -v dpkg-deb 2>/dev/null || true)"
  if [[ -z "$DPKG_DEB" ]]; then
    echo "ERROR: dpkg-deb not found on PATH; pass --dpkg-deb explicitly" >&2
    exit 1
  fi
fi
if [[ ! -x "$DPKG_DEB" ]]; then
  echo "ERROR: dpkg-deb is not executable: ${DPKG_DEB}" >&2
  exit 1
fi

# --- Require jq and curl ---
if ! command -v jq &>/dev/null; then
  echo "ERROR: jq is required but not found on PATH" >&2
  exit 1
fi
if ! command -v curl &>/dev/null; then
  echo "ERROR: curl is required but not found on PATH" >&2
  exit 1
fi

# --- Create staging directory ---
if [[ -z "$STAGING_DIR" ]]; then
  STAGING_DIR="$(mktemp -d -t metaflux-provenance-XXXXXXXX)"
  echo "Staging directory: ${STAGING_DIR}"
else
  mkdir -p "$STAGING_DIR"
fi

# --- Set default output path ---
if [[ -z "$OUTPUT" ]]; then
  OUTPUT="${REPO_DIR}/tmp/outputs/target-sdk-provenance.json"
fi
mkdir -p "$(dirname "$OUTPUT")"

# --- Extract download URLs from provenance JSON ---
SNAPSHOT_ORIGIN="$(jq -r '.snapshot_origin' "$MANIFEST")"

echo "Snapshot origin: ${SNAPSHOT_ORIGIN}"
echo "Provenance manifest: ${MANIFEST}"
echo "SDK manifest: ${SDK_MANIFEST}"
echo "Keyring: ${KEYRING}"
echo "gpgv: ${GPGV}"
echo "dpkg-deb: ${DPKG_DEB}"
echo ""

VERIFIER_ARGS=(
  --manifest "$MANIFEST"
  --sdk-manifest "$SDK_MANIFEST"
  --keyring "$KEYRING"
  --gpgv "$GPGV"
  --dpkg-deb "$DPKG_DEB"
  --output "$OUTPUT"
)

# --- Download InRelease files ---
echo "=== Downloading InRelease files ==="
RELEASE_COUNT="$(jq '.releases | length' "$MANIFEST")"
for (( i = 0; i < RELEASE_COUNT; i++ )); do
  RELEASE_ID="$(jq -r ".releases[$i].id" "$MANIFEST")"
  REL_PATH="$(jq -r ".releases[$i].inrelease.relative_path" "$MANIFEST")"
  EXPECTED_HASH="$(jq -r ".releases[$i].inrelease.sha256" "$MANIFEST")"
  DEST="${STAGING_DIR}/inrelease-${RELEASE_ID}"

  URL="${SNAPSHOT_ORIGIN}/${REL_PATH}"
  echo "  ${RELEASE_ID}: ${URL}"
  if [[ ! -f "$DEST" ]] || ! sha256sum -c <(echo "${EXPECTED_HASH}  ${DEST}") &>/dev/null; then
    curl -fSL -o "$DEST" "$URL"
  fi

  # Verify downloaded hash matches the manifest
  OBSERVED_HASH="$(sha256sum "$DEST" | cut -d' ' -f1)"
  if [[ "$OBSERVED_HASH" != "$EXPECTED_HASH" ]]; then
    echo "ERROR: InRelease ${RELEASE_ID} hash mismatch: ${OBSERVED_HASH} != ${EXPECTED_HASH}" >&2
    exit 1
  fi

  VERIFIER_ARGS+=(--inrelease "${RELEASE_ID}=${DEST}")
done

# --- Download Packages.xz index files ---
echo ""
echo "=== Downloading Packages.xz indexes ==="
for (( i = 0; i < RELEASE_COUNT; i++ )); do
  INDEX_COUNT="$(jq ".releases[$i].indexes | length" "$MANIFEST")"
  for (( j = 0; j < INDEX_COUNT; j++ )); do
    INDEX_ID="$(jq -r ".releases[$i].indexes[$j].id" "$MANIFEST")"
    REL_PATH="$(jq -r ".releases[$i].indexes[$j].relative_path" "$MANIFEST")"
    EXPECTED_HASH="$(jq -r ".releases[$i].indexes[$j].sha256" "$MANIFEST")"
    SUITE="$(jq -r ".releases[$i].suite" "$MANIFEST")"
    DEST="${STAGING_DIR}/index-${INDEX_ID}.xz"

    URL="${SNAPSHOT_ORIGIN}/dists/${SUITE}/${REL_PATH}"
    echo "  ${INDEX_ID}: ${URL}"
    if [[ ! -f "$DEST" ]] || ! sha256sum -c <(echo "${EXPECTED_HASH}  ${DEST}") &>/dev/null; then
      curl -fSL -o "$DEST" "$URL"
    fi

    OBSERVED_HASH="$(sha256sum "$DEST" | cut -d' ' -f1)"
    if [[ "$OBSERVED_HASH" != "$EXPECTED_HASH" ]]; then
      echo "ERROR: Index ${INDEX_ID} hash mismatch: ${OBSERVED_HASH} != ${EXPECTED_HASH}" >&2
      exit 1
    fi

    VERIFIER_ARGS+=(--index "${INDEX_ID}=${DEST}")
  done
done

# --- Download DEB packages ---
echo ""
echo "=== Downloading DEB packages ==="
PACKAGE_COUNT="$(jq '.packages | length' "$MANIFEST")"
for (( i = 0; i < PACKAGE_COUNT; i++ )); do
  PKG_NAME="$(jq -r ".packages[$i].name" "$MANIFEST")"
  PKG_FILENAME="$(jq -r ".packages[$i].filename" "$MANIFEST")"
  EXPECTED_HASH="$(jq -r ".packages[$i].sha256" "$MANIFEST")"
  # Sanitize package name for use as filename (replace colons, slashes)
  SAFE_NAME="${PKG_NAME//:/_}"
  DEST="${STAGING_DIR}/deb-${SAFE_NAME}.deb"

  URL="${SNAPSHOT_ORIGIN}/${PKG_FILENAME}"
  echo "  ${PKG_NAME}: ${URL}"
  if [[ ! -f "$DEST" ]] || ! sha256sum -c <(echo "${EXPECTED_HASH}  ${DEST}") &>/dev/null; then
    curl -fSL -o "$DEST" "$URL"
  fi

  OBSERVED_HASH="$(sha256sum "$DEST" | cut -d' ' -f1)"
  if [[ "$OBSERVED_HASH" != "$EXPECTED_HASH" ]]; then
    echo "ERROR: DEB ${PKG_NAME} hash mismatch: ${OBSERVED_HASH} != ${EXPECTED_HASH}" >&2
    exit 1
  fi

  VERIFIER_ARGS+=(--deb "${PKG_NAME}=${DEST}")
done

# --- Run the verifier ---
echo ""
echo "=== Running provenance verifier ==="
python3 "${REPO_DIR}/toolchains/tests/verify_ubuntu_target_sdk_provenance.py" \
  "${VERIFIER_ARGS[@]}"

echo ""
echo "Provenance verification passed."
echo "Evidence written to: ${OUTPUT}"
