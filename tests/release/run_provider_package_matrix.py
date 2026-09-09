#!/usr/bin/env python3
"""Qualify generic provider packages in digest-pinned distribution images."""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import os
from pathlib import Path
from pathlib import PurePosixPath
import shutil
import subprocess
import sys
import tarfile
import tempfile
import textwrap
from typing import Any


UBUNTU_CASES = (
    ("ubuntu-20.04.6", "ubuntu_20_image", "Ubuntu 20.04.6 LTS", "glibc 2.31"),
    ("ubuntu-22.04.5", "ubuntu_22_image", "Ubuntu 22.04.5 LTS", "glibc 2.35"),
    ("ubuntu-24.04.4", "ubuntu_24_image", "Ubuntu 24.04.4 LTS", "glibc 2.39"),
)
ROCKY_CASE = ("rocky-9.8", "rocky_9_image", "Rocky Linux 9.8 (Blue Onyx)", "glibc 2.34")


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--deb", required=True, type=Path)
    parser.add_argument("--rpm", required=True, type=Path)
    parser.add_argument("--tar", required=True, dest="tarball", type=Path)
    parser.add_argument("--ubuntu-20-image", required=True)
    parser.add_argument("--ubuntu-22-image", required=True)
    parser.add_argument("--ubuntu-24-image", required=True)
    parser.add_argument("--rocky-9-image", required=True)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--podman", default="podman")
    parser.add_argument(
        "--rpmbuild",
        help="rpmbuild used to create the old-version upgrade fixture (defaults to PATH)",
    )
    return parser.parse_args()


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def image_reference_digest(image: str) -> str | None:
    prefix, marker, digest = image.rpartition("@sha256:")
    if marker == "":
        return None
    if not prefix or len(digest) != 64 or any(character not in "0123456789abcdef" for character in digest):
        raise ValueError(f"invalid sha256 image reference: {image}")
    return "sha256:" + digest


def require_digest_pinned_images(images: list[str]) -> None:
    mutable = [image for image in images if image_reference_digest(image) is None]
    if mutable:
        raise ValueError(
            "qualification images must use @sha256 references: " + ", ".join(mutable)
        )


def harness_fingerprint(*scripts: Path) -> dict[str, Any]:
    return {
        "python_executable": str(Path(sys.executable).resolve(strict=True)),
        "python_version": sys.version.split()[0],
        "scripts": [
            {"path": str(script.resolve(strict=True)), "sha256": sha256(script.resolve(strict=True))}
            for script in scripts
        ],
    }


def command_record(
    arguments: list[str], *, environment: dict[str, str] | None = None
) -> dict[str, Any]:
    process = subprocess.run(
        arguments,
        check=False,
        capture_output=True,
        text=True,
        env=environment,
    )
    return {
        "argv": arguments,
        "returncode": process.returncode,
        "stdout": process.stdout.splitlines(),
        "stderr": process.stderr.splitlines(),
    }


def require_file(path: Path, label: str) -> Path:
    resolved = path.resolve(strict=True)
    if not resolved.is_file():
        raise ValueError(f"{label} is not a regular file: {resolved}")
    return resolved


def extract_tar_payload(tarball: Path, destination: Path) -> list[str]:
    destination.mkdir()
    with tarfile.open(tarball, mode="r:gz") as archive:
        members = archive.getmembers()
        for member in members:
            normalized = PurePosixPath(member.name)
            if normalized.is_absolute() or ".." in normalized.parts:
                raise ValueError(f"unsafe tar member: {member.name}")
        archive.extractall(destination, members=members, filter="data")
    return [member.name for member in members]


def build_prior_rpm(rpmbuild: str, workspace: Path) -> tuple[Path, dict[str, Any]]:
    top = workspace / "rpmbuild"
    for name in ("BUILD", "BUILDROOT", "RPMS", "SOURCES", "SPECS", "SRPMS", "rpmdb", "tmp"):
        (top / name).mkdir(parents=True, exist_ok=True)
    spec = top / "SPECS" / "metaflux-provider-prior.spec"
    spec.write_text(
        textwrap.dedent(
            """\
            Name: metaflux-provider
            Version: 0.0.0
            Release: 1
            Summary: MetaFlux provider package upgrade fixture
            License: NOASSERTION
            BuildArch: x86_64

            %description
            Package-manager upgrade fixture containing one obsolete marker.

            %prep

            %build

            %install
            mkdir -p %{buildroot}/usr/share/metaflux
            printf '%s\\n' prior-version > %{buildroot}/usr/share/metaflux/prior-version

            %files
            /usr/share/metaflux/prior-version

            %changelog
            * Sat Aug 29 2026 MetaFlux Project <noreply@metaflux.invalid> - 0.0.0-1
            - Deterministic package-upgrade fixture
            """
        ),
        encoding="ascii",
    )
    command = [
        rpmbuild,
        "--define",
        f"_topdir {top}",
        "--define",
        f"_dbpath {top / 'rpmdb'}",
        "--define",
        f"_tmppath {top / 'tmp'}",
        "--define",
        "_buildhost metaflux.invalid",
        "--define",
        "_build_id_links none",
        "--define",
        "_binary_payload w9.gzdio",
        "--define",
        "source_date_epoch_from_changelog 0",
        "--define",
        "use_source_date_epoch_as_buildtime 1",
        "--define",
        "build_mtime_policy clamp_to_source_date_epoch",
        "--define",
        "__os_install_post %{nil}",
        "-bb",
        str(spec),
    ]
    environment = dict(os.environ)
    environment["SOURCE_DATE_EPOCH"] = "1787932800"
    record = command_record(command, environment=environment)
    if record["returncode"] != 0:
        raise RuntimeError("rpmbuild failed while creating the prior-version fixture")
    candidates = sorted((top / "RPMS").glob("**/metaflux-provider-0.0.0-1.*.rpm"))
    if len(candidates) != 1:
        raise RuntimeError(f"expected one prior-version RPM, found {len(candidates)}")
    return candidates[0], record


IMAGE_RUNTIME_EVIDENCE_SCRIPT = r"""
set -eu
printf 'METAFLUX_IMAGE_EVIDENCE kernel_sysname '
uname -s
printf 'METAFLUX_IMAGE_EVIDENCE kernel_release '
uname -r
printf 'METAFLUX_IMAGE_EVIDENCE kernel_version '
uname -v
printf 'METAFLUX_IMAGE_EVIDENCE kernel_machine '
uname -m

if command -v dpkg-query >/dev/null 2>&1; then
  package_manager=dpkg
  package_count=$(dpkg-query -W | wc -l)
  package_set_sha256=$(
    dpkg-query -W -f='${binary:Package}\t${Version}\n' \
      | LC_ALL=C sort \
      | sha256sum \
      | cut -d ' ' -f 1
  )
  repository_files=$(
    find /etc/apt -type f \
      \( -path '*/sources.list' -o -name '*.list' -o -name '*.sources' \) \
      -print \
      | LC_ALL=C sort
  )
elif command -v rpm >/dev/null 2>&1; then
  package_manager=rpm
  package_count=$(rpm -qa | wc -l)
  package_set_sha256=$(
    rpm -qa --qf '%{NAME}\t%{EPOCH}:%{VERSION}-%{RELEASE}.%{ARCH}\n' \
      | LC_ALL=C sort \
      | sha256sum \
      | cut -d ' ' -f 1
  )
  repository_files=$(
    find /etc/yum.repos.d -type f -name '*.repo' -print \
      | LC_ALL=C sort
  )
else
  echo 'no supported package database is present in the qualification image' >&2
  exit 1
fi

printf 'METAFLUX_IMAGE_EVIDENCE package_manager %s\n' "$package_manager"
printf 'METAFLUX_IMAGE_EVIDENCE package_count %s\n' "$package_count"
printf 'METAFLUX_IMAGE_EVIDENCE package_set_sha256 %s\n' "$package_set_sha256"
printf '%s\n' "$repository_files" | while IFS= read -r repository_file; do
  test -n "$repository_file" || continue
  repository_sha256=$(sha256sum "$repository_file" | cut -d ' ' -f 1)
  printf 'METAFLUX_REPOSITORY_FILE %s %s\n' \
    "$repository_sha256" "$repository_file"
done
"""


def _require_sha256(value: str, label: str) -> str:
    if len(value) != 64 or any(character not in "0123456789abcdef" for character in value):
        raise RuntimeError(f"invalid {label} SHA-256: {value}")
    return value


def parse_image_runtime_evidence(record: dict[str, Any]) -> dict[str, Any]:
    fields: dict[str, str] = {}
    repository_files: list[dict[str, str]] = []
    repository_paths: set[str] = set()
    for line in record["stdout"]:
        if line.startswith("METAFLUX_IMAGE_EVIDENCE "):
            _, key, value = line.split(" ", maxsplit=2)
            if key in fields:
                raise RuntimeError(f"duplicate image evidence field: {key}")
            fields[key] = value
        elif line.startswith("METAFLUX_REPOSITORY_FILE "):
            _, digest, path = line.split(" ", maxsplit=2)
            _require_sha256(digest, "repository file")
            if not path.startswith("/") or path in repository_paths:
                raise RuntimeError(f"invalid or duplicate repository path: {path}")
            repository_paths.add(path)
            repository_files.append({"path": path, "sha256": digest})

    required_fields = {
        "kernel_machine",
        "kernel_release",
        "kernel_sysname",
        "kernel_version",
        "package_count",
        "package_manager",
        "package_set_sha256",
    }
    missing = sorted(required_fields - fields.keys())
    if missing:
        raise RuntimeError("image evidence is missing fields: " + ", ".join(missing))
    if fields["package_manager"] not in {"dpkg", "rpm"}:
        raise RuntimeError(f"unsupported package manager: {fields['package_manager']}")
    try:
        package_count = int(fields["package_count"].strip())
    except ValueError as error:
        raise RuntimeError("image package count is not an integer") from error
    if package_count <= 0:
        raise RuntimeError("image package database is empty")
    _require_sha256(fields["package_set_sha256"], "package set")
    if not repository_files:
        raise RuntimeError("image has no package repository configuration files")

    return {
        "host_kernel": {
            "sysname": fields["kernel_sysname"],
            "release": fields["kernel_release"],
            "version": fields["kernel_version"],
            "machine": fields["kernel_machine"],
        },
        "repository_snapshot": {
            "scope": "repository configuration embedded in the immutable image",
            "files": repository_files,
        },
        "base_package_state": {
            "manager": fields["package_manager"],
            "package_count": package_count,
            "package_set_sha256": fields["package_set_sha256"],
        },
        "qualification_updates": [],
        "update_policy": "no system updates; qualification containers use --network=none",
        "probe_isolation": {
            "network": "none",
            "pull": "never",
            "root_filesystem": "read-only",
        },
        "probe_command": record,
    }


def image_runtime_evidence(podman: str, image: str) -> dict[str, Any]:
    record = command_record(
        [
            podman,
            "run",
            "--rm",
            "--pull=never",
            "--network=none",
            "--read-only",
            image,
            "/bin/sh",
            "-c",
            IMAGE_RUNTIME_EVIDENCE_SCRIPT,
        ]
    )
    if record["returncode"] != 0:
        raise RuntimeError(f"image evidence probe failed for {image}")
    return parse_image_runtime_evidence(record)


def image_inspect(podman: str, image: str) -> dict[str, Any]:
    requested_digest = image_reference_digest(image)
    record = command_record([podman, "image", "inspect", image])
    if record["returncode"] != 0:
        raise RuntimeError(f"image is not available locally: {image}")
    parsed = json.loads("\n".join(record["stdout"]))
    if not isinstance(parsed, list) or len(parsed) != 1:
        raise RuntimeError(f"unexpected image inspect result for {image}")
    item = parsed[0]
    if requested_digest is not None and item.get("Digest") != requested_digest:
        raise RuntimeError(
            f"image digest mismatch for {image}: observed {item.get('Digest')}"
        )
    return {
        "reference": image,
        "id": item.get("Id"),
        "digest": item.get("Digest"),
        "repo_digests": item.get("RepoDigests", []),
        "created": item.get("Created"),
        "runtime_evidence": image_runtime_evidence(podman, image),
        "inspect_command": record,
    }


NEGATIVE_ASSERTION_HELPERS = r"""
assert_no_fixed_match() {
  pattern=$1
  shift
  if grep -F -- "$pattern" "$@"; then
    printf 'forbidden fixed-string match: %s\n' "$pattern" >&2
    return 1
  else
    status=$?
  fi
  if test "$status" -ne 1; then
    printf 'fixed-string scan failed with status %s: %s\n' "$status" "$pattern" >&2
    return "$status"
  fi
  return 0
}

assert_no_extended_match() {
  pattern=$1
  shift
  if grep -E -- "$pattern" "$@"; then
    printf 'forbidden extended-regexp match: %s\n' "$pattern" >&2
    return 1
  else
    status=$?
  fi
  if test "$status" -ne 1; then
    printf 'extended-regexp scan failed with status %s: %s\n' "$status" "$pattern" >&2
    return "$status"
  fi
  return 0
}

assert_no_recursive_match() {
  pattern=$1
  shift
  if grep -R -a -l -- "$pattern" "$@"; then
    printf 'forbidden recursive match: %s\n' "$pattern" >&2
    return 1
  else
    status=$?
  fi
  if test "$status" -ne 1; then
    printf 'recursive scan failed with status %s: %s\n' "$status" "$pattern" >&2
    return "$status"
  fi
  return 0
}
"""


VERIFY_PAYLOAD = NEGATIVE_ASSERTION_HELPERS + r"""
verify_payload() {
  test -f /usr/lib/metaflux/providers/libcuda.so.1.0.0
  test -f /usr/lib/metaflux/providers/libmetaflux-cublas-provider.so.1.0.0
  test -f /usr/lib/metaflux/providers/libnvidia-ml.so.1.0.0
  test -L /usr/lib/metaflux/providers/libcuda.so.1
  test -L /usr/lib/metaflux/providers/libmetaflux-cublas-provider.so.1
  test -L /usr/lib/metaflux/providers/libnvidia-ml.so.1
  test "$(readlink /usr/lib/metaflux/providers/libcuda.so.1)" = libcuda.so.1.0.0
  test "$(readlink /usr/lib/metaflux/providers/libmetaflux-cublas-provider.so.1)" = \
    libmetaflux-cublas-provider.so.1.0.0
  test "$(readlink /usr/lib/metaflux/providers/libnvidia-ml.so.1)" = libnvidia-ml.so.1.0.0
  LD_LIBRARY_PATH=/usr/lib/metaflux/providers ldd \
    /usr/lib/metaflux/providers/libcuda.so.1 > /tmp/metaflux-ldd-cuda
  LD_LIBRARY_PATH=/usr/lib/metaflux/providers ldd \
    /usr/lib/metaflux/providers/libmetaflux-cublas-provider.so.1 > /tmp/metaflux-ldd-cublas
  LD_LIBRARY_PATH=/usr/lib/metaflux/providers ldd \
    /usr/lib/metaflux/providers/libnvidia-ml.so.1 > /tmp/metaflux-ldd-nvml
  assert_no_fixed_match 'not found' \
    /tmp/metaflux-ldd-cuda /tmp/metaflux-ldd-cublas /tmp/metaflux-ldd-nvml
  assert_no_recursive_match /nix/store/ \
    /usr/lib/metaflux /usr/include/metaflux /usr/share/metaflux
}

verify_platform() {
  grep -Fx "PRETTY_NAME=\"$EXPECTED_PRETTY\"" /etc/os-release
  test "$(getconf GNU_LIBC_VERSION)" = "$EXPECTED_GLIBC"
}

prepare_vendor_sentinels() {
  mkdir -p "$VENDOR_DIR"
  printf '%s\n' vendor-cuda-sentinel > "$VENDOR_DIR/libcuda.so.1"
  printf '%s\n' vendor-nvml-sentinel > "$VENDOR_DIR/libnvidia-ml.so.1"
  CUDA_SENTINEL=$(sha256sum "$VENDOR_DIR/libcuda.so.1" | cut -d ' ' -f 1)
  NVML_SENTINEL=$(sha256sum "$VENDOR_DIR/libnvidia-ml.so.1" | cut -d ' ' -f 1)
}

verify_vendor_sentinels() {
  test "$(sha256sum "$VENDOR_DIR/libcuda.so.1" | cut -d ' ' -f 1)" = "$CUDA_SENTINEL"
  test "$(sha256sum "$VENDOR_DIR/libnvidia-ml.so.1" | cut -d ' ' -f 1)" = "$NVML_SENTINEL"
}

verify_removed() {
  for path in \
    /usr/include/metaflux \
    /usr/lib/libmetaflux_cuda_passthrough.a \
    /usr/lib/metaflux \
    /usr/share/metaflux; do
    test ! -e "$path"
  done
}
"""


DEB_SCRIPT = VERIFY_PAYLOAD + r"""
set -eu
VENDOR_DIR=/usr/lib/x86_64-linux-gnu
verify_platform
prepare_vendor_sentinels

# Fresh install and removal.
dpkg -i /artifacts/current.deb
test "$(dpkg-query -W -f='${Version}' metaflux-provider)" = 0.1.0
verify_payload
verify_vendor_sentinels
dpkg -r metaflux-provider
verify_removed
verify_vendor_sentinels

# A real version transition proves package-manager upgrade cleanup.
mkdir -p /tmp/prior/DEBIAN /tmp/prior/usr/share/metaflux
cat > /tmp/prior/DEBIAN/control <<'EOF'
Package: metaflux-provider
Version: 0.0.0
Architecture: amd64
Maintainer: MetaFlux Project <noreply@metaflux.invalid>
Section: libs
Priority: optional
Description: MetaFlux provider upgrade fixture
EOF
printf '%s\n' prior-version > /tmp/prior/usr/share/metaflux/prior-version
dpkg-deb --build --root-owner-group /tmp/prior /tmp/metaflux-provider-prior.deb
dpkg -i /tmp/metaflux-provider-prior.deb
test "$(dpkg-query -W -f='${Version}' metaflux-provider)" = 0.0.0
test -f /usr/share/metaflux/prior-version
dpkg -i /artifacts/current.deb
test "$(dpkg-query -W -f='${Version}' metaflux-provider)" = 0.1.0
test ! -e /usr/share/metaflux/prior-version
verify_payload
verify_vendor_sentinels
dpkg -r metaflux-provider
verify_removed
verify_vendor_sentinels
"""


RPM_SCRIPT = VERIFY_PAYLOAD + r"""
set -eu
VENDOR_DIR=/usr/lib64
verify_platform
prepare_vendor_sentinels

# Fresh install and removal.
rpm -ivh /artifacts/current.rpm
test "$(rpm -q --queryformat '%{VERSION}-%{RELEASE}' metaflux-provider)" = 0.1.0-1
verify_payload
verify_vendor_sentinels
rpm -e metaflux-provider
verify_removed
verify_vendor_sentinels

# Upgrade from a separately built old-version fixture.
rpm -ivh /artifacts/prior.rpm
test "$(rpm -q --queryformat '%{VERSION}-%{RELEASE}' metaflux-provider)" = 0.0.0-1
test -f /usr/share/metaflux/prior-version
rpm -Uvh /artifacts/current.rpm
test "$(rpm -q --queryformat '%{VERSION}-%{RELEASE}' metaflux-provider)" = 0.1.0-1
test ! -e /usr/share/metaflux/prior-version
verify_payload
verify_vendor_sentinels
rpm -e metaflux-provider
verify_removed
verify_vendor_sentinels
"""


TAR_SCRIPT = VERIFY_PAYLOAD + r"""
set -eu
verify_platform
prepare_vendor_sentinels

# Fresh extraction and explicit manifest-owned removal. The host harness uses
# Python's checked tar reader so this also works in the Rocky minimal image.
cp -a /artifacts/tar-root/. /
verify_payload
verify_vendor_sentinels
rm -rf /usr/lib/metaflux /usr/include/metaflux /usr/share/metaflux
rm -f /usr/lib/libmetaflux_cuda_passthrough.a
verify_removed
verify_vendor_sentinels

# Overlay an obsolete private payload, then prove current extraction replaces it.
mkdir -p /usr/lib/metaflux/providers /usr/share/metaflux
printf '%s\n' prior-version > /usr/share/metaflux/prior-version
printf '%s\n' old-provider > /usr/lib/metaflux/providers/libcuda.so.1.0.0
cp -a /artifacts/tar-root/. /
test "$(head -c 4 /usr/lib/metaflux/providers/libcuda.so.1.0.0)" = "$(printf '\177ELF')"
verify_payload
verify_vendor_sentinels
rm -rf /usr/lib/metaflux /usr/include/metaflux /usr/share/metaflux
rm -f /usr/lib/libmetaflux_cuda_passthrough.a
verify_removed
verify_vendor_sentinels
"""


def run_container_case(
    podman: str,
    image: str,
    input_directory: Path,
    name: str,
    package_format: str,
    expected_pretty: str,
    expected_glibc: str,
) -> dict[str, Any]:
    script = {"deb": DEB_SCRIPT, "rpm": RPM_SCRIPT, "tar": TAR_SCRIPT}[package_format]
    vendor_dir = "/usr/lib64" if name.startswith("rocky-") else "/usr/lib/x86_64-linux-gnu"
    arguments = [
        podman,
        "run",
        "--rm",
        "--pull=never",
        "--network=none",
        "--volume",
        f"{input_directory}:/artifacts:ro",
        "--env",
        f"EXPECTED_PRETTY={expected_pretty}",
        "--env",
        f"EXPECTED_GLIBC={expected_glibc}",
        "--env",
        f"VENDOR_DIR={vendor_dir}",
        image,
        "/bin/sh",
        "-c",
        script,
    ]
    record = command_record(arguments)
    return {
        "name": name,
        "format": package_format,
        "expected_pretty_name": expected_pretty,
        "expected_glibc": expected_glibc,
        "status": "passed" if record["returncode"] == 0 else "failed",
        "command": record,
    }


def write_json(path: Path, payload: dict[str, Any]) -> None:
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    os.replace(temporary, path)


def main() -> int:
    arguments = parse_arguments()
    try:
        deb = require_file(arguments.deb, "DEB")
        rpm = require_file(arguments.rpm, "RPM")
        tarball = require_file(arguments.tarball, "tar archive")
        images = [getattr(arguments, field) for _, field, _, _ in UBUNTU_CASES]
        images.append(getattr(arguments, ROCKY_CASE[1]))
        require_digest_pinned_images(images)

        rpmbuild = arguments.rpmbuild or shutil.which("rpmbuild")
        if rpmbuild is None:
            raise ValueError("rpmbuild is required to construct the old-version RPM fixture")
        rpmbuild = str(Path(rpmbuild).resolve(strict=True))

        arguments.output_dir.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="metaflux-package-matrix-") as temporary_text:
            workspace = Path(temporary_text)
            inputs = workspace / "inputs"
            inputs.mkdir()
            shutil.copy2(deb, inputs / "current.deb")
            shutil.copy2(rpm, inputs / "current.rpm")
            shutil.copy2(tarball, inputs / "current.tar.gz")
            tar_members = extract_tar_payload(tarball, inputs / "tar-root")
            prior_rpm, rpmbuild_record = build_prior_rpm(rpmbuild, workspace)
            shutil.copy2(prior_rpm, inputs / "prior.rpm")

            inspections = {image: image_inspect(arguments.podman, image) for image in images}
            cases: list[dict[str, Any]] = []
            for name, field, pretty, glibc in UBUNTU_CASES:
                image = getattr(arguments, field)
                cases.append(
                    run_container_case(
                        arguments.podman, image, inputs, name, "deb", pretty, glibc
                    )
                )
                cases.append(
                    run_container_case(
                        arguments.podman, image, inputs, name, "tar", pretty, glibc
                    )
                )
            rocky_name, rocky_field, rocky_pretty, rocky_glibc = ROCKY_CASE
            rocky_image = getattr(arguments, rocky_field)
            cases.append(
                run_container_case(
                    arguments.podman,
                    rocky_image,
                    inputs,
                    rocky_name,
                    "rpm",
                    rocky_pretty,
                    rocky_glibc,
                )
            )
            cases.append(
                run_container_case(
                    arguments.podman,
                    rocky_image,
                    inputs,
                    rocky_name,
                    "tar",
                    rocky_pretty,
                    rocky_glibc,
                )
            )

            passed = all(case["status"] == "passed" for case in cases)
            report = {
                "schema_version": 1,
                "status": "passed" if passed else "failed",
                "captured_at": dt.datetime.now(dt.timezone.utc).astimezone().isoformat(),
                "network_policy": "container runs use --network=none and --pull=never",
                "harness": harness_fingerprint(Path(__file__)),
                "inputs": {
                    "deb": {"path": str(deb), "sha256": sha256(deb)},
                    "rpm": {"path": str(rpm), "sha256": sha256(rpm)},
                    "tar": {"path": str(tarball), "sha256": sha256(tarball)},
                    "prior_rpm_sha256": sha256(inputs / "prior.rpm"),
                },
                "tar_extraction": {
                    "implementation": "python.tarfile data filter with explicit traversal check",
                    "members": tar_members,
                },
                "images": inspections,
                "prior_rpm_build": rpmbuild_record,
                "cases": cases,
            }
            report_path = arguments.output_dir / "provider-package-matrix.json"
            write_json(report_path, report)
            print(report_path)
            return 0 if passed else 1
    except (OSError, ValueError, RuntimeError, json.JSONDecodeError) as error:
        print(f"package matrix setup failed: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
