#!/usr/bin/env python3
"""Build reproducible MetaFlux provider or complete Linux release packages."""

from __future__ import annotations

import argparse
import gzip
import os
from pathlib import Path
import re
import shutil
import stat
import subprocess
import tarfile
import tempfile
from typing import Iterable


PROJECT_ROOT = Path(__file__).resolve().parent.parent
PROJECT_VERSION_FILE = PROJECT_ROOT / "VERSION"
PROJECT_VERSION_PATTERN = re.compile(
    r"(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\Z"
)
PACKAGE_ARCHITECTURE = "amd64"
TAR_ARCHITECTURE = "x86_64"
NIX_STORE_MARKER = b"/nix/store/"
TARGET_TRIPLE = "x86_64-unknown-linux-gnu"
TARGET_INTERPRETER = "/lib64/ld-linux-x86-64.so.2"
GLIBC_FLOOR = (2, 31)
ALLOWED_DYNAMIC_LIBRARIES = {
    "ld-linux-x86-64.so.2",
    "libc.so.6",
    "libdl.so.2",
    "libm.so.6",
    "libpthread.so.0",
    "librt.so.1",
}
GLIBC_SYMBOL_PATTERN = re.compile(r"\bGLIBC_(\d+)\.(\d+)\b")
MANIFEST_NAMES = {
    "target-sdk": ".metaflux-target-sdk-manifest",
    "generic-toolchain": ".metaflux-generic-llvm-toolchain",
}
BACKEND_VULKAN_DYNAMIC_LIBRARIES = ALLOWED_DYNAMIC_LIBRARIES | {
    "libvulkan.so.1",
}
BACKEND_VULKAN_HEADERS = (
    "vulkan.h",
    "vulkan_arguments.h",
    "vulkan_memory.h",
    "api.h",
)
BACKEND_VULKAN_SHARED_LIBRARY = "usr/lib/metaflux/backends/libmetaflux_vulkan_backend.so"


DEB_PREINST = r"""#!/bin/sh
set -e

capture_upgrade_state() {
  test -d /run/systemd/system || return 0
  command -v systemctl >/dev/null 2>&1 || return 0
  test -e /run/metafluxd-package-upgrade.state && return 0
  active=0
  enabled=0
  if systemctl is-active --quiet metafluxd.socket; then active=1; fi
  if systemctl is-enabled --quiet metafluxd.socket; then enabled=1; fi
  printf 'active=%s\nenabled=%s\n' "$active" "$enabled" \
    > /run/metafluxd-package-upgrade.state
  systemctl stop metafluxd.socket metafluxd.service || true
}

case "${1:-}" in
  upgrade) capture_upgrade_state ;;
esac
exit 0
"""


DEB_POSTINST = r"""#!/bin/sh
set -e

ensure_account() {
  if command -v systemd-sysusers >/dev/null 2>&1; then
    systemd-sysusers /usr/lib/sysusers.d/metaflux.conf || true
  fi
  if ! getent group metaflux >/dev/null 2>&1; then
    if command -v groupadd >/dev/null 2>&1; then
      groupadd --system metaflux
    elif command -v addgroup >/dev/null 2>&1; then
      addgroup --system metaflux
    else
      echo 'metaflux group creation tool is missing' >&2
      exit 1
    fi
  fi
  if ! getent passwd metaflux >/dev/null 2>&1; then
    shell=/usr/sbin/nologin
    test -x "$shell" || shell=/sbin/nologin
    test -x "$shell" || shell=/bin/false
    if command -v useradd >/dev/null 2>&1; then
      useradd --system --gid metaflux --home-dir /var/lib/metaflux \
        --no-create-home --shell "$shell" \
        --comment 'MetaFlux compute service' metaflux
    elif command -v adduser >/dev/null 2>&1; then
      adduser --system --ingroup metaflux --home /var/lib/metaflux \
        --no-create-home --disabled-login \
        --gecos 'MetaFlux compute service' metaflux
    else
      echo 'metaflux user creation tool is missing' >&2
      exit 1
    fi
  fi
}

ensure_directories() {
  if command -v systemd-tmpfiles >/dev/null 2>&1; then
    systemd-tmpfiles --create /usr/lib/tmpfiles.d/metaflux.conf || true
  fi
  install -d -m 0755 -o root -g metaflux /run/metaflux
  install -d -m 0750 -o metaflux -g metaflux /var/cache/metaflux
  install -d -m 0700 -o metaflux -g metaflux \
    /var/cache/metaflux/compiler/users
  install -d -m 0750 -o metaflux -g metaflux /var/lib/metaflux
  install -d -m 0755 -o root -g root /var/lib/metaflux/aot
}

systemd_running() {
  test -d /run/systemd/system || return 1
  command -v systemctl >/dev/null 2>&1
}

start_fresh_socket() {
  systemctl daemon-reload || true
  if command -v deb-systemd-helper >/dev/null 2>&1; then
    deb-systemd-helper preset metafluxd.socket || true
  else
    systemctl preset metafluxd.socket || true
  fi
  if command -v deb-systemd-invoke >/dev/null 2>&1; then
    deb-systemd-invoke start metafluxd.socket || true
  else
    systemctl start metafluxd.socket || true
  fi
}

restore_upgrade_socket() {
  active=0
  enabled=0
  . /run/metafluxd-package-upgrade.state
  rm -f /run/metafluxd-package-upgrade.state
  systemctl daemon-reload || true
  if test "$enabled" -eq 1; then
    if command -v deb-systemd-helper >/dev/null 2>&1; then
      deb-systemd-helper enable metafluxd.socket || true
    else
      systemctl enable metafluxd.socket || true
    fi
  fi
  if test "$active" -eq 1; then
    if command -v deb-systemd-invoke >/dev/null 2>&1; then
      deb-systemd-invoke start metafluxd.socket || true
    else
      systemctl start metafluxd.socket || true
    fi
  fi
}

ensure_account
ensure_directories
if systemd_running; then
  if test -f /run/metafluxd-package-upgrade.state; then
    restore_upgrade_socket
  else
    start_fresh_socket
  fi
else
  rm -f /run/metafluxd-package-upgrade.state
fi
exit 0
"""


DEB_PRERM = r"""#!/bin/sh
set -e

systemd_running() {
  test -d /run/systemd/system || return 1
  command -v systemctl >/dev/null 2>&1
}

capture_upgrade_state() {
  systemd_running || return 0
  test -e /run/metafluxd-package-upgrade.state && return 0
  active=0
  enabled=0
  if systemctl is-active --quiet metafluxd.socket; then active=1; fi
  if systemctl is-enabled --quiet metafluxd.socket; then enabled=1; fi
  printf 'active=%s\nenabled=%s\n' "$active" "$enabled" \
    > /run/metafluxd-package-upgrade.state
  systemctl stop metafluxd.socket metafluxd.service || true
}

stop_for_removal() {
  systemd_running || return 0
  systemctl stop metafluxd.socket metafluxd.service || true
}

case "${1:-}" in
  upgrade) capture_upgrade_state ;;
  remove|purge) stop_for_removal ;;
esac
exit 0
"""


DEB_POSTRM = r"""#!/bin/sh
set -e
if test -d /run/systemd/system && command -v systemctl >/dev/null 2>&1; then
  systemctl daemon-reload || true
fi
case "${1:-}" in
  purge) rm -f /run/metafluxd-package-upgrade.state ;;
esac
exit 0
"""


RPM_PRE = r"""if test "${1:-0}" -eq 2; then
  if test -d /run/systemd/system && command -v systemctl >/dev/null 2>&1 &&
    test ! -e /run/metafluxd-package-upgrade.state; then
    active=0
    enabled=0
    if systemctl is-active --quiet metafluxd.socket; then active=1; fi
    if systemctl is-enabled --quiet metafluxd.socket; then enabled=1; fi
    printf 'active=%s\nenabled=%s\n' "$active" "$enabled" \
      > /run/metafluxd-package-upgrade.state
    systemctl stop metafluxd.socket metafluxd.service || true
  fi
fi
"""


RPM_POST = r"""ensure_account() {
  if command -v systemd-sysusers >/dev/null 2>&1; then
    systemd-sysusers /usr/lib/sysusers.d/metaflux.conf || true
  fi
  if ! getent group metaflux >/dev/null 2>&1; then
    if command -v groupadd >/dev/null 2>&1; then
      groupadd --system metaflux
    elif command -v addgroup >/dev/null 2>&1; then
      addgroup --system metaflux
    else
      echo 'metaflux group creation tool is missing' >&2
      exit 1
    fi
  fi
  if ! getent passwd metaflux >/dev/null 2>&1; then
    shell=/usr/sbin/nologin
    test -x "$shell" || shell=/sbin/nologin
    test -x "$shell" || shell=/bin/false
    if command -v useradd >/dev/null 2>&1; then
      useradd --system --gid metaflux --home-dir /var/lib/metaflux \
        --no-create-home --shell "$shell" \
        --comment 'MetaFlux compute service' metaflux
    elif command -v adduser >/dev/null 2>&1; then
      adduser --system --ingroup metaflux --home /var/lib/metaflux \
        --no-create-home --disabled-login \
        --gecos 'MetaFlux compute service' metaflux
    else
      echo 'metaflux user creation tool is missing' >&2
      exit 1
    fi
  fi
}

ensure_directories() {
  if command -v systemd-tmpfiles >/dev/null 2>&1; then
    systemd-tmpfiles --create /usr/lib/tmpfiles.d/metaflux.conf || true
  fi
  install -d -m 0755 -o root -g metaflux /run/metaflux
  install -d -m 0750 -o metaflux -g metaflux /var/cache/metaflux
  install -d -m 0700 -o metaflux -g metaflux \
    /var/cache/metaflux/compiler/users
  install -d -m 0750 -o metaflux -g metaflux /var/lib/metaflux
  install -d -m 0755 -o root -g root /var/lib/metaflux/aot
}

systemd_running() {
  test -d /run/systemd/system || return 1
  command -v systemctl >/dev/null 2>&1
}

ensure_account
ensure_directories
if systemd_running; then
  if test -f /run/metafluxd-package-upgrade.state; then
    active=0
    enabled=0
    . /run/metafluxd-package-upgrade.state
    rm -f /run/metafluxd-package-upgrade.state
    systemctl daemon-reload || true
    if test "$enabled" -eq 1; then
      if command -v deb-systemd-helper >/dev/null 2>&1; then
        deb-systemd-helper enable metafluxd.socket || true
      else
        systemctl enable metafluxd.socket || true
      fi
    fi
    if test "$active" -eq 1; then
      if command -v deb-systemd-invoke >/dev/null 2>&1; then
        deb-systemd-invoke start metafluxd.socket || true
      else
        systemctl start metafluxd.socket || true
      fi
    fi
  else
    systemctl daemon-reload || true
    if command -v deb-systemd-helper >/dev/null 2>&1; then
      deb-systemd-helper preset metafluxd.socket || true
    else
      systemctl preset metafluxd.socket || true
    fi
    if command -v deb-systemd-invoke >/dev/null 2>&1; then
      deb-systemd-invoke start metafluxd.socket || true
    else
      systemctl start metafluxd.socket || true
    fi
  fi
else
  rm -f /run/metafluxd-package-upgrade.state
fi
"""


RPM_PREUN = r"""if test "${1:-0}" -eq 0; then
  if test -d /run/systemd/system && command -v systemctl >/dev/null 2>&1; then
    systemctl stop metafluxd.socket metafluxd.service || true
  fi
fi
"""


RPM_POSTUN = r"""if test -d /run/systemd/system && command -v systemctl >/dev/null 2>&1; then
  systemctl daemon-reload || true
fi
if test "${1:-0}" -eq 0; then
  rm -f /run/metafluxd-package-upgrade.state
fi
"""


def validate_project_version(value: str, source: str) -> str:
    if PROJECT_VERSION_PATTERN.fullmatch(value) is None:
        raise ValueError(
            f"{source} must contain exactly three numeric components "
            f"without leading zeros; got {value!r}"
        )
    return value


def read_project_version() -> str:
    value = PROJECT_VERSION_FILE.read_text(encoding="ascii").strip()
    return validate_project_version(value, str(PROJECT_VERSION_FILE))


def parse_project_version(value: str) -> str:
    try:
        return validate_project_version(value, "--version")
    except ValueError as error:
        raise argparse.ArgumentTypeError(str(error)) from error


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument(
        "--kind", choices=("provider", "complete", "backend-vulkan"), required=True
    )
    parser.add_argument(
        "--format",
        dest="formats",
        action="append",
        choices=("deb", "rpm", "tar", "all"),
        help="format to emit; repeat for more than one (default: all)",
    )
    parser.add_argument("--target-sdk", type=Path)
    parser.add_argument("--generic-toolchain", type=Path)
    parser.add_argument(
        "--version",
        type=parse_project_version,
        default=read_project_version(),
    )
    parser.add_argument("--release", default="1")
    parser.add_argument(
        "--source-date-epoch",
        type=int,
        default=int(os.environ.get("SOURCE_DATE_EPOCH", "0")),
    )
    parser.add_argument("--cmake", default="cmake")
    parser.add_argument("--dpkg-deb", default="dpkg-deb")
    parser.add_argument("--rpmbuild", default="rpmbuild")
    parser.add_argument("--readelf", default="readelf")
    return parser.parse_args()


def run(command: list[str], *, environment: dict[str, str] | None = None) -> None:
    print("+", " ".join(command), flush=True)
    subprocess.run(command, check=True, env=environment)


def resolve_input_file(source: Path, manifest_name: str, label: str) -> Path:
    source = source.resolve(strict=True)
    candidate = source / manifest_name if source.is_dir() else source
    if not candidate.is_file():
        raise ValueError(f"{label} does not contain {manifest_name}: {source}")
    return candidate


def write_text(path: Path, content: str, mode: int = 0o644) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="ascii")
    path.chmod(mode)


def normalize_timestamps(root: Path, epoch: int) -> None:
    for path in sorted(root.rglob("*"), key=lambda item: len(item.parts), reverse=True):
        try:
            os.utime(path, (epoch, epoch), follow_symlinks=False)
        except FileNotFoundError:
            continue


def manifest_fields(source_file: Path) -> dict[str, str]:
    fields: dict[str, str] = {}
    try:
        lines = source_file.read_text(encoding="ascii").splitlines()
    except UnicodeDecodeError as error:
        raise ValueError(f"{source_file} is not an ASCII provenance manifest") from error
    for line_number, line in enumerate(lines, start=1):
        line = line.strip()
        if not line:
            continue
        key, separator, value = line.partition("=")
        if not separator or not key or key in fields:
            raise ValueError(f"malformed provenance manifest line {source_file}:{line_number}")
        fields[key] = value
    return fields


def validate_manifest(source_file: Path, key: str) -> dict[str, str]:
    fields = manifest_fields(source_file)
    if fields.get("target-triple") != TARGET_TRIPLE:
        raise ValueError(f"{key} manifest does not target {TARGET_TRIPLE}: {source_file}")
    if key == "target-sdk":
        if fields.get("distribution") != "ubuntu-20.04" or not re.fullmatch(
            r"2\.31(?:-.*)?", fields.get("glibc", "")
        ):
            raise ValueError(
                f"target SDK manifest is not the Ubuntu 20.04/glibc 2.31 floor: {source_file}"
            )
    elif key == "generic-toolchain":
        if fields.get("target-sdk-distribution") != "ubuntu-20.04":
            raise ValueError(
                f"generic toolchain manifest does not use the Ubuntu 20.04 SDK: {source_file}"
            )
    else:
        raise ValueError(f"unknown provenance manifest kind: {key}")
    return fields


def copy_manifest(stage: Path, source: Path, key: str, destination_name: str) -> dict[str, str]:
    source_file = resolve_input_file(source, MANIFEST_NAMES[key], key)
    content = source_file.read_bytes()
    if NIX_STORE_MARKER in content:
        raise ValueError(f"{key} manifest embeds a Nix store path: {source_file}")
    fields = validate_manifest(source_file, key)
    destination = stage / "usr/share/metaflux/toolchains" / destination_name
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source_file, destination)
    return fields


def copy_target_lld(stage: Path, generic_toolchain: Path) -> None:
    generic_toolchain = generic_toolchain.resolve(strict=True)
    candidates = (generic_toolchain / "bin/ld.lld", generic_toolchain / "bin/lld")
    source = next((candidate for candidate in candidates if candidate.is_file()), None)
    if source is None:
        raise ValueError(f"generic toolchain has no ld.lld or lld: {generic_toolchain}")
    destination = stage / "usr/libexec/metaflux/ld.lld"
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, destination)
    destination.chmod(destination.stat().st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)


def readelf_output(readelf: str, arguments: list[str], path: Path) -> str:
    result = subprocess.run(
        [readelf, *arguments, str(path)],
        check=False,
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        raise ValueError(
            f"readelf failed for {path}: {result.stderr.strip() or result.stdout.strip()}"
        )
    return result.stdout


def validate_elf(
    path: Path,
    relative_path: str,
    readelf: str,
    *,
    allowed_libraries: frozenset[str] = ALLOWED_DYNAMIC_LIBRARIES,
) -> None:
    program_headers = readelf_output(readelf, ["-l", "-W"], path)
    interpreters = re.findall(r"Requesting program interpreter: ([^]]+)", program_headers)
    if interpreters and any(interpreter != TARGET_INTERPRETER for interpreter in interpreters):
        raise ValueError(f"ELF has a non-system interpreter: {path}")
    if relative_path in {"usr/bin/metafluxd", "usr/libexec/metaflux/ld.lld"} and not interpreters:
        raise ValueError(f"required executable has no system interpreter: {path}")

    dynamic = readelf_output(readelf, ["-d", "-W"], path)
    if "(RPATH)" in dynamic or "(RUNPATH)" in dynamic:
        raise ValueError(f"ELF has RPATH/RUNPATH: {path}")
    dependencies = re.findall(r"\(NEEDED\).*?\[([^]]+)\]", dynamic)
    unexpected = sorted(set(dependencies) - allowed_libraries)
    if unexpected:
        raise ValueError(f"ELF has non-system dynamic dependencies {unexpected}: {path}")

    versions = [
        (int(major), int(minor))
        for major, minor in GLIBC_SYMBOL_PATTERN.findall(
            readelf_output(readelf, ["--version-info", "-W"], path)
        )
    ]
    if versions and max(versions) > GLIBC_FLOOR:
        highest = max(versions)
        raise ValueError(f"ELF requires GLIBC_{highest[0]}.{highest[1]} above 2.31: {path}")


def validate_payload(stage: Path, kind: str, readelf: str) -> None:
    allowed_libraries = ALLOWED_DYNAMIC_LIBRARIES
    if kind == "backend-vulkan":
        required = [
            BACKEND_VULKAN_SHARED_LIBRARY,
            *(f"usr/include/metaflux/backend/{name}" for name in BACKEND_VULKAN_HEADERS),
            "usr/share/doc/metaflux-backend-vulkan/README.md",
        ]
    else:
        lib_candidates = ["usr/lib/metaflux", "usr/lib64/metaflux"]
        lib_dir = None
        for candidate in lib_candidates:
            if (stage / candidate / "providers").is_dir():
                lib_dir = candidate
                break
        if lib_dir is None:
            raise ValueError("package payload is missing metaflux providers directory")
        required = [
            f"{lib_dir}/providers/libcuda.so.1.0.0",
            f"{lib_dir}/providers/libnvidia-ml.so.1.0.0",
            f"{lib_dir}/providers/libcuda.so.1",
            f"{lib_dir}/providers/libnvidia-ml.so.1",
            "usr/include/metaflux",
            "usr/share/metaflux",
        ]
        if kind == "complete":
            required.extend(
                (
                    "usr/bin/metafluxd",
                    "usr/libexec/metaflux/ld.lld",
                    "usr/lib/udev/rules.d/70-metaflux.rules",
                    "usr/share/metaflux/toolchains/ubuntu-20.04-target-sdk.manifest",
                    "usr/share/metaflux/toolchains/generic-llvm-toolchain.manifest",
                )
            )
    for relative in required:
        if not (stage / relative).exists():
            raise ValueError(f"package payload is missing {relative}")
    shared_library = stage / BACKEND_VULKAN_SHARED_LIBRARY
    if kind == "backend-vulkan":
        elf_header = readelf_output(readelf, ["-h", "-W"], shared_library)
        if "DYN" not in elf_header:
            raise ValueError(
                f"packaged Vulkan backend is not a shared library: {shared_library}"
            )
        allowed_libraries = BACKEND_VULKAN_DYNAMIC_LIBRARIES
    for path in stage.rglob("*"):
        if path.is_symlink():
            if os.readlink(path).startswith("/"):
                raise ValueError(f"package payload has an absolute symlink: {path}")
            continue
        if not path.is_file():
            continue
        with path.open("rb") as source:
            overlap = b""
            while chunk := source.read(1024 * 1024):
                if NIX_STORE_MARKER in overlap + chunk:
                    raise ValueError(f"package payload embeds a Nix store path: {path}")
                overlap = (overlap + chunk)[-(len(NIX_STORE_MARKER) - 1) :]
        with path.open("rb") as source:
            if source.read(4) == b"\x7fELF":
                validate_elf(
                    path,
                    path.relative_to(stage).as_posix(),
                    readelf,
                    allowed_libraries=allowed_libraries,
                )


def stage_backend_vulkan(build_dir: Path, stage: Path) -> None:
    """Stage the packaged shared Vulkan backend from a generic build tree."""
    candidates = sorted(build_dir.rglob("libmetaflux_vulkan_backend.so"))
    if not candidates:
        raise ValueError(
            "generic build tree has no libmetaflux_vulkan_backend.so; "
            "configure it with METAFLUX_BUILD_VULKAN_BACKEND=ON and "
            "METAFLUX_VULKAN_BACKEND_SHARED=ON"
        )
    (stage / "usr/lib/metaflux/backends").mkdir(parents=True, exist_ok=True)
    shutil.copyfile(candidates[0], stage / BACKEND_VULKAN_SHARED_LIBRARY)
    shutil.copymode(candidates[0], stage / BACKEND_VULKAN_SHARED_LIBRARY)

    header_source = PROJECT_ROOT / "contracts/plugin/backend/v1/include/metaflux/backend"
    header_stage = stage / "usr/include/metaflux/backend"
    header_stage.mkdir(parents=True, exist_ok=True)
    for name in BACKEND_VULKAN_HEADERS:
        source = header_source / name
        if not source.is_file():
            raise ValueError(f"backend contract header is missing: {source}")
        shutil.copyfile(source, header_stage / name)

    notes = PROJECT_ROOT / "packaging/backend/metaflux-backend-vulkan/README.md"
    if not notes.is_file():
        raise ValueError(f"backend packaging notes are missing: {notes}")
    notes_stage = stage / "usr/share/doc/metaflux-backend-vulkan/README.md"
    notes_stage.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(notes, notes_stage)


def install_stage(
    build_dir: Path,
    kind: str,
    cmake: str,
    workspace: Path,
    target_sdk: Path | None,
    generic_toolchain: Path | None,
    epoch: int,
    readelf: str,
) -> Path:
    build_dir = build_dir.resolve(strict=True)
    stage = workspace / "stage"
    stage.mkdir()
    if kind == "backend-vulkan":
        stage_backend_vulkan(build_dir, stage)
    else:
        command = [cmake, "--install", str(build_dir), "--prefix", str(stage / "usr")]
        if kind == "provider":
            command.extend(("--component", "Provider"))
        run(command)
    if kind == "complete":
        if target_sdk is None or generic_toolchain is None:
            raise ValueError("complete packages require --target-sdk and --generic-toolchain")
        copy_target_lld(stage, generic_toolchain)
        target_fields = copy_manifest(
            stage,
            target_sdk,
            "target-sdk",
            "ubuntu-20.04-target-sdk.manifest",
        )
        generic_fields = copy_manifest(
            stage,
            generic_toolchain,
            "generic-toolchain",
            "generic-llvm-toolchain.manifest",
        )
        if generic_fields.get("target-sdk-build-identity") != target_fields.get("build-identity"):
            raise ValueError("generic toolchain and target SDK build identities do not match")
        if generic_fields.get("target-sdk-package-set-sha256") != target_fields.get(
            "package-set-sha256"
        ):
            raise ValueError("generic toolchain and target SDK package sets do not match")
    normalize_timestamps(stage, epoch)
    validate_payload(stage, kind, readelf)
    return stage


def package_stem(kind: str) -> str:
    if kind == "provider":
        return "metaflux-provider"
    if kind == "backend-vulkan":
        return "metaflux-backend-vulkan"
    return "metaflux"


def package_description(kind: str) -> str:
    if kind == "complete":
        return "Complete CPU-backed MetaFlux runtime"
    if kind == "backend-vulkan":
        return "MetaFlux Vulkan execution backend"
    return "MetaFlux CUDA and NVML providers"


def output_path(output_dir: Path, kind: str, version: str, release: str, fmt: str) -> Path:
    stem = package_stem(kind)
    if fmt == "deb":
        return output_dir / f"{stem}_{version}_{PACKAGE_ARCHITECTURE}.deb"
    if fmt == "rpm":
        return output_dir / f"{stem}-{version}-{release}.x86_64.rpm"
    return output_dir / f"{stem}-{version}-{TAR_ARCHITECTURE}.tar.gz"


def deb_control(kind: str, version: str) -> str:
    stem = package_stem(kind)
    depends = "libc6 (>= 2.31)"
    if kind == "backend-vulkan":
        depends = "libc6 (>= 2.31), libvulkan1"
    fields = [
        f"Package: {stem}",
        f"Version: {version}",
        f"Architecture: {PACKAGE_ARCHITECTURE}",
        "Maintainer: MetaFlux Project <noreply@metaflux.invalid>",
        "Section: libs",
        "Priority: optional",
        f"Depends: {depends}",
    ]
    if kind == "complete":
        fields.extend(
            (
                "Provides: metaflux-provider",
                "Conflicts: metaflux-provider",
                f"Replaces: metaflux-provider (<< {version})",
            )
        )
    fields.extend(
        (
            f"Description: {package_description(kind)}",
            " Compatibility providers and runtime files owned below the MetaFlux prefix.",
            "",
        )
    )
    return "\n".join(fields)


def prepare_deb_root(stage: Path, kind: str, version: str, workspace: Path) -> Path:
    root = workspace / "deb-root"
    shutil.copytree(stage, root, symlinks=True)
    control_dir = root / "DEBIAN"
    control_dir.mkdir()
    write_text(control_dir / "control", deb_control(kind, version))
    if kind == "complete":
        write_text(control_dir / "preinst", DEB_PREINST, 0o755)
        write_text(control_dir / "postinst", DEB_POSTINST, 0o755)
        write_text(control_dir / "prerm", DEB_PRERM, 0o755)
        write_text(control_dir / "postrm", DEB_POSTRM, 0o755)
    return root


def build_deb(
    stage: Path,
    kind: str,
    version: str,
    output: Path,
    dpkg_deb: str,
    workspace: Path,
    epoch: int,
) -> None:
    root = prepare_deb_root(stage, kind, version, workspace)
    normalize_timestamps(root, epoch)
    environment = dict(os.environ)
    environment["SOURCE_DATE_EPOCH"] = str(epoch)
    if output.exists():
        output.unlink()
    run(
        [dpkg_deb, "--build", "--root-owner-group", str(root), str(output)],
        environment=environment,
    )


def rpm_files(stage: Path) -> list[str]:
    private_directory_roots = (
        "usr/include/metaflux",
        "usr/lib/cmake/MetaFlux",
        "usr/lib/metaflux",
        "usr/libexec/metaflux",
        "usr/share/doc/metaflux",
        "usr/share/doc/metaflux-backend-vulkan",
        "usr/share/metaflux",
    )
    paths: list[str] = []
    for path in sorted(stage.rglob("*"), key=lambda item: item.relative_to(stage).as_posix()):
        relative_path = path.relative_to(stage).as_posix()
        relative = "/" + relative_path
        if path.is_dir() and not path.is_symlink():
            if any(
                relative_path == root or relative_path.startswith(root + "/")
                for root in private_directory_roots
            ):
                paths.append(f"%dir {relative}")
        else:
            paths.append(relative)
    return paths


def rpm_spec(stage: Path, kind: str, version: str, release: str) -> str:
    stem = package_stem(kind)
    description = package_description(kind)
    if kind == "backend-vulkan":
        requires = "Requires: glibc >= 2.31\nRequires: vulkan-loader\n"
        relationships = ""
    else:
        requires = "Requires: glibc >= 2.31\n"
        relationships = (
            "Provides: metaflux-provider\nObsoletes: metaflux-provider < %{version}-%{release}"
            if kind == "complete"
            else ""
        )
        if relationships:
            relationships += "\n"
    scriptlets = ""
    if kind == "complete":
        scriptlets = (
            "\n%pre\n"
            + RPM_PRE
            + "\n%post\n"
            + RPM_POST
            + "\n%preun\n"
            + RPM_PREUN
            + "\n%postun\n"
            + RPM_POSTUN
        )
    stage_shell = "'" + str(stage).replace("'", "'\\''") + "'"
    files = "\n".join(rpm_files(stage))
    return (
        f"Name: {stem}\n"
        f"Version: {version}\n"
        f"Release: {release}\n"
        f"Summary: {description}\n"
        "License: NOASSERTION\n"
        "BuildArch: x86_64\n"
        f"{requires}"
        f"{relationships}"
        "%description\n"
        f"{description}.\n"
        "\n"
        "%prep\n"
        "\n"
        "%build\n"
        "\n"
        "%install\n"
        "rm -rf %{buildroot}\n"
        f"mkdir -p %{{buildroot}}\ncp -a {stage_shell}/. %{{buildroot}}/\n"
        "\n"
        "%files\n"
        f"{files}\n"
        f"{scriptlets}\n"
        "%changelog\n"
        "* Sat Aug 29 2026 MetaFlux Project <noreply@metaflux.invalid> - "
        f"{version}-{release}\n"
        "- Reproducible MetaFlux release artifact\n"
    )


def build_rpm(
    stage: Path,
    kind: str,
    version: str,
    release: str,
    output: Path,
    rpmbuild: str,
    workspace: Path,
    epoch: int,
) -> None:
    top = workspace / "rpmbuild"
    for name in ("BUILD", "BUILDROOT", "RPMS", "SOURCES", "SPECS", "SRPMS", "tmp", "rpmdb"):
        (top / name).mkdir(parents=True, exist_ok=True)
    spec = top / "SPECS" / f"{package_stem(kind)}.spec"
    write_text(spec, rpm_spec(stage, kind, version, release))
    environment = dict(os.environ)
    environment["SOURCE_DATE_EPOCH"] = str(epoch)
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
    run(command, environment=environment)
    candidates = sorted((top / "RPMS").glob(f"**/{package_stem(kind)}-{version}-{release}.*.rpm"))
    if len(candidates) != 1:
        raise RuntimeError(f"expected one RPM, found {len(candidates)}: {candidates}")
    if output.exists():
        output.unlink()
    shutil.copyfile(candidates[0], output)


def iter_stage_entries(stage: Path) -> Iterable[Path]:
    return sorted(stage.rglob("*"), key=lambda item: item.relative_to(stage).as_posix())


def build_tar(stage: Path, output: Path, epoch: int) -> None:
    if output.exists():
        output.unlink()
    with output.open("wb") as raw:
        with gzip.GzipFile(fileobj=raw, mode="wb", filename="", mtime=epoch) as compressed:
            with tarfile.open(fileobj=compressed, mode="w", format=tarfile.GNU_FORMAT) as archive:
                for path in iter_stage_entries(stage):
                    arcname = path.relative_to(stage).as_posix()
                    info = archive.gettarinfo(str(path), arcname=arcname)
                    info.uid = 0
                    info.gid = 0
                    info.uname = ""
                    info.gname = ""
                    info.mtime = epoch
                    info.pax_headers = {}
                    if info.isreg():
                        with path.open("rb") as source:
                            archive.addfile(info, source)
                    else:
                        archive.addfile(info)


def main() -> int:
    arguments = parse_arguments()
    if arguments.source_date_epoch < 0:
        raise ValueError("--source-date-epoch must be non-negative")
    formats = arguments.formats or ["all"]
    if "all" in formats:
        formats = ["deb", "rpm", "tar"]
    formats = list(dict.fromkeys(formats))
    arguments.output_dir.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".metaflux-package-", dir=arguments.output_dir) as temporary:
        workspace = Path(temporary)
        stage = install_stage(
            arguments.build_dir,
            arguments.kind,
            arguments.cmake,
            workspace,
            arguments.target_sdk,
            arguments.generic_toolchain,
            arguments.source_date_epoch,
            arguments.readelf,
        )
        for package_format in formats:
            output = output_path(
                arguments.output_dir,
                arguments.kind,
                arguments.version,
                arguments.release,
                package_format,
            )
            if package_format == "deb":
                build_deb(
                    stage,
                    arguments.kind,
                    arguments.version,
                    output,
                    arguments.dpkg_deb,
                    workspace,
                    arguments.source_date_epoch,
                )
            elif package_format == "rpm":
                build_rpm(
                    stage,
                    arguments.kind,
                    arguments.version,
                    arguments.release,
                    output,
                    arguments.rpmbuild,
                    workspace,
                    arguments.source_date_epoch,
                )
            else:
                build_tar(stage, output, arguments.source_date_epoch)
            print(output)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, ValueError, subprocess.CalledProcessError) as error:
        print(f"packaging/build.py: error: {error}", file=os.sys.stderr)
        raise SystemExit(1)
