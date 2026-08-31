#!/usr/bin/env python3
"""Qualify complete MetaFlux packages in digest-pinned distribution images."""

from __future__ import annotations

import argparse
import datetime as dt
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import textwrap
from typing import Any

import run_provider_package_matrix as provider_matrix
from run_provider_package_matrix import (
    ROCKY_CASE,
    UBUNTU_CASES,
    command_record,
    extract_tar_payload,
    harness_fingerprint,
    image_inspect,
    require_digest_pinned_images,
    require_file,
    sha256,
    write_json,
)


TARGET_INTERPRETER = "/lib64/ld-linux-x86-64.so.2"
GLIBC_FLOOR = (2, 31)
FIXTURE_DYNAMIC_LIBRARIES = {
    "ld-linux-x86-64.so.2",
    "libc.so.6",
    "libcuda.so.1",
    "libdl.so.2",
    "libm.so.6",
    "libpthread.so.0",
    "librt.so.1",
}
GLIBC_SYMBOL_PATTERN = re.compile(r"\bGLIBC_(\d+)\.(\d+)\b")


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--deb", required=True, type=Path)
    parser.add_argument("--rpm", required=True, type=Path)
    parser.add_argument("--tar", required=True, dest="tarball", type=Path)
    parser.add_argument("--cuda-acceptance", required=True, type=Path)
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
    parser.add_argument("--readelf", default="readelf")
    return parser.parse_args()


def readelf_output(readelf: str, arguments: list[str], path: Path) -> str:
    result = subprocess.run(
        [readelf, *arguments, str(path)],
        check=False,
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        raise ValueError(
            f"readelf failed for release fixture {path}: "
            f"{result.stderr.strip() or result.stdout.strip()}"
        )
    return result.stdout


def validate_release_fixture(path: Path, readelf: str, *, needs_cuda: bool) -> dict[str, Any]:
    with path.open("rb") as source:
        if source.read(4) != b"\x7fELF":
            raise ValueError(f"release fixture is not an ELF executable: {path}")
        source.seek(0)
        if b"/nix/store/" in source.read():
            raise ValueError(f"release fixture embeds a Nix store path: {path}")

    program_headers = readelf_output(readelf, ["-l", "-W"], path)
    interpreters = re.findall(r"Requesting program interpreter: ([^]]+)", program_headers)
    if interpreters != [TARGET_INTERPRETER]:
        raise ValueError(
            f"release fixture must use {TARGET_INTERPRETER}: {path} ({interpreters})"
        )

    dynamic = readelf_output(readelf, ["-d", "-W"], path)
    if "(RPATH)" in dynamic or "(RUNPATH)" in dynamic:
        raise ValueError(f"release fixture has RPATH/RUNPATH: {path}")
    dependencies = sorted(set(re.findall(r"\(NEEDED\).*?\[([^]]+)\]", dynamic)))
    unexpected = sorted(set(dependencies) - FIXTURE_DYNAMIC_LIBRARIES)
    if unexpected:
        raise ValueError(
            f"release fixture has non-system dynamic dependencies {unexpected}: {path}"
        )
    if needs_cuda and "libcuda.so.1" not in dependencies:
        raise ValueError(f"CUDA acceptance fixture does not link libcuda.so.1: {path}")
    if not needs_cuda and "libcuda.so.1" in dependencies:
        raise ValueError(f"activation launcher unexpectedly links libcuda.so.1: {path}")

    versions = [
        (int(major), int(minor))
        for major, minor in GLIBC_SYMBOL_PATTERN.findall(
            readelf_output(readelf, ["--version-info", "-W"], path)
        )
    ]
    highest = max(versions) if versions else None
    if highest is not None and highest > GLIBC_FLOOR:
        raise ValueError(
            f"release fixture requires GLIBC_{highest[0]}.{highest[1]} above 2.31: {path}"
        )
    return {
        "interpreter": interpreters[0],
        "needed": dependencies,
        "highest_glibc": None if highest is None else f"{highest[0]}.{highest[1]}",
    }


def build_prior_rpm(rpmbuild: str, workspace: Path) -> tuple[Path, dict[str, Any]]:
    top = workspace / "rpmbuild"
    for name in ("BUILD", "BUILDROOT", "RPMS", "SOURCES", "SPECS", "SRPMS", "rpmdb", "tmp"):
        (top / name).mkdir(parents=True, exist_ok=True)
    spec = top / "SPECS" / "metaflux-prior.spec"
    spec.write_text(
        textwrap.dedent(
            """\
            Name: metaflux
            Version: 0.0.0
            Release: 1
            Summary: MetaFlux complete-package upgrade fixture
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
    candidates = sorted((top / "RPMS").glob("**/metaflux-0.0.0-1.*.rpm"))
    if len(candidates) != 1:
        raise RuntimeError(f"expected one prior-version RPM, found {len(candidates)}")
    return candidates[0], record


SHUTDOWN_STATISTICS_HELPERS = r"""
verify_shutdown_statistics() (
  set -efu
  daemon_log=$1
  expected_mode=$2
  expected_compiler_requests=$3
  expected_cache_hits=$4
  expected_cache_misses=$5
  expected_loaded_modules=$6

  if statistics_lines=$(grep -E '^metafluxd: cpu-execution ' "$daemon_log"); then
    :
  else
    status=$?
    if test "$status" -eq 1; then
      echo "metafluxd shutdown statistics are missing" >&2
    else
      echo "failed to read metafluxd shutdown statistics" >&2
    fi
    return 1
  fi
  line_count=$(printf '%s\n' "$statistics_lines" | wc -l)
  if test "$line_count" -ne 1; then
    echo "expected exactly one metafluxd shutdown statistics line, found $line_count" >&2
    return 1
  fi

  set -f
  set -- $statistics_lines
  if test "$#" -ne 16 ||
    test "$1" != 'metafluxd:' ||
    test "$2" != 'cpu-execution' ||
    test "$3" != "mode=$expected_mode" ||
    test "$4" != "compiler-requests=$expected_compiler_requests" ||
    test "$5" != "cache-hits=$expected_cache_hits" ||
    test "$6" != "cache-misses=$expected_cache_misses" ||
    test "$7" != "loaded-modules=$expected_loaded_modules"; then
    echo "malformed or unexpected metafluxd shutdown statistics: $statistics_lines" >&2
    return 1
  fi

  read_unsigned_counter() {
    counter_name=$1
    counter_token=$2
    case "$counter_token" in
      "$counter_name"=*) counter_value=${counter_token#*=} ;;
      *)
        echo "expected $counter_name in shutdown statistics, got $counter_token" >&2
        return 1
        ;;
    esac
    case "$counter_value" in
      '' | *[!0-9]*)
        echo "non-numeric $counter_name in shutdown statistics: $counter_value" >&2
        return 1
        ;;
    esac
    printf '%s\n' "$counter_value"
  }

  registrations=$(read_unsigned_counter host-address-space-registrations "$8")
  direct_source_operations=$(read_unsigned_counter direct-host-source-operations "$9")
  direct_source_bytes=$(read_unsigned_counter direct-host-source-bytes "${10}")
  direct_destination_operations=$(
    read_unsigned_counter direct-host-destination-operations "${11}"
  )
  direct_destination_bytes=$(read_unsigned_counter direct-host-destination-bytes "${12}")
  staged_source_operations=$(read_unsigned_counter staged-host-source-operations "${13}")
  staged_source_bytes=$(read_unsigned_counter staged-host-source-bytes "${14}")
  staged_destination_operations=$(
    read_unsigned_counter staged-host-destination-operations "${15}"
  )
  staged_destination_bytes=$(read_unsigned_counter staged-host-destination-bytes "${16}")

  if test "$registrations" -ne 1 ||
    test "$direct_source_operations" -le 0 ||
    test "$direct_source_bytes" -le 0 ||
    test "$direct_destination_operations" -le 0 ||
    test "$direct_destination_bytes" -le 0 ||
    test "$staged_source_operations" -ne 0 ||
    test "$staged_source_bytes" -ne 0 ||
    test "$staged_destination_operations" -ne 0 ||
    test "$staged_destination_bytes" -ne 0; then
    echo "shutdown statistics do not prove direct-only host copies: $statistics_lines" >&2
    return 1
  fi

  printf '%s\n' "$statistics_lines"
)
"""


VERIFY_PAYLOAD = (
    provider_matrix.NEGATIVE_ASSERTION_HELPERS
    + SHUTDOWN_STATISTICS_HELPERS
    + r"""
run_cuda_acceptance_mode() (
  set -eu
  mode=$1
  record_evidence=$2
  root=/tmp/metaflux-cuda-acceptance-$mode
  socket=/run/metaflux/metafluxd.sock
  daemon_log=$root/metafluxd.log
  application_log=$root/application.log
  daemon_pid=
  stop_daemon() {
    test -n "$daemon_pid" || return 0
    if ! kill -TERM "$daemon_pid" 2>/dev/null; then
      wait "$daemon_pid" 2>/dev/null || true
      daemon_pid=
      echo "packaged metafluxd exited before shutdown" >&2
      return 1
    fi
    (
      sleep 5
      kill -KILL "$daemon_pid" 2>/dev/null || true
    ) &
    watchdog_pid=$!
    daemon_status=0
    wait "$daemon_pid" || daemon_status=$?
    kill -TERM "$watchdog_pid" 2>/dev/null || true
    wait "$watchdog_pid" 2>/dev/null || true
    daemon_pid=
    if test "$daemon_status" -ne 0; then
      echo "packaged metafluxd shutdown failed with status $daemon_status" >&2
      return 1
    fi
  }
  cleanup() {
    stop_daemon >/dev/null 2>&1 || true
    rm -f "$socket"
    rm -rf "$root"
  }
  trap cleanup EXIT
  trap 'exit 129' HUP
  trap 'exit 130' INT
  trap 'exit 143' TERM
  rm -rf "$root"
  mkdir -p "$root"
  rm -f "$socket"
  service_uid=$(id -u metaflux)
  service_gid=$(getent group metaflux | cut -d: -f3)

  daemon_is_ready() {
    test -S "$socket" || return 1
    kill -0 "$daemon_pid" 2>/dev/null || return 1
    test "$(runuser -u metaflux -- readlink "/proc/$daemon_pid/exe" 2>/dev/null)" = \
      /usr/bin/metafluxd || return 1
    runuser -u metaflux -- readlink "/proc/$daemon_pid/fd/3" 2>/dev/null \
      | grep -Eq '^socket:\[[0-9]+\]$' || return 1
    runuser -u metaflux -- awk -v expected="$service_uid" '
      $1 == "Uid:" {
        found = 1
        for (field = 2; field <= 5; ++field) {
          if ($field != expected) exit 1
        }
      }
      END { if (!found) exit 1 }
    ' "/proc/$daemon_pid/status" || return 1
    runuser -u metaflux -- awk -v expected="$service_gid" '
      $1 == "Gid:" {
        found = 1
        for (field = 2; field <= 5; ++field) {
          if ($field != expected) exit 1
        }
      }
      END { if (!found) exit 1 }
    ' "/proc/$daemon_pid/status" || return 1
  }

  METAFLUX_SOCKET="$socket" \
  METAFLUX_MODE=managed \
  METAFLUX_CPU_EXECUTION_MODE="$mode" \
    /artifacts/activation-launcher "$socket" /usr/bin/metafluxd \
      >"$daemon_log" 2>&1 &
  daemon_pid=$!

  attempt=0
  while test "$attempt" -lt 1000 && ! daemon_is_ready; do
    if ! kill -0 "$daemon_pid" 2>/dev/null; then
      cat "$daemon_log" >&2
      return 1
    fi
    attempt=$((attempt + 1))
    sleep 0.01
  done
  if ! daemon_is_ready; then
    echo "packaged metafluxd did not complete fd-3 activation and identity drop" >&2
    cat "$daemon_log" >&2
    return 1
  fi

  test "$(stat -c %a "$socket")" = 660
  test "$(stat -c %u "$socket")" = "$service_uid"
  test "$(stat -c %g "$socket")" = "$service_gid"
  test "$(runuser -u metaflux -- readlink "/proc/$daemon_pid/exe")" = \
    /usr/bin/metafluxd
  runuser -u metaflux -- readlink "/proc/$daemon_pid/fd/3" \
    | grep -E '^socket:\[[0-9]+\]$'

  if ! runuser -u metaflux -- env \
    METAFLUX_SOCKET="$socket" \
    METAFLUX_MODE=managed \
    LD_LIBRARY_PATH=/usr/lib/metaflux/providers \
    /artifacts/cuda-add-copy >"$application_log" 2>&1; then
    cat "$application_log" >&2
    cat "$daemon_log" >&2
    return 1
  fi
  grep -F 'cuda-add-copy: PASS' "$application_log"

  if ! stop_daemon; then
    cat "$daemon_log" >&2
    return 1
  fi

  case "$mode" in
    interpreter)
      expected_compiler_requests=0
      expected_cache_hits=0
      expected_cache_misses=0
      ;;
    cold-jit)
      expected_compiler_requests=1
      expected_cache_hits=0
      expected_cache_misses=1
      ;;
    warm-jit)
      expected_compiler_requests=0
      expected_cache_hits=1
      expected_cache_misses=0
      ;;
    aot)
      expected_compiler_requests=0
      expected_cache_hits=1
      expected_cache_misses=0
      ;;
    *)
      echo "unknown compiler mode $mode" >&2
      return 1
      ;;
  esac
  statistics_line=$(
    verify_shutdown_statistics \
      "$daemon_log" \
      "$mode" \
      "$expected_compiler_requests" \
      "$expected_cache_hits" \
      "$expected_cache_misses" \
      1
  )
  if test "$record_evidence" -eq 1; then
    printf 'METAFLUX_COMPILER_MODE_EVIDENCE mode=%s statistics=%s\n' \
      "$mode" "${statistics_line#metafluxd: cpu-execution }"
  fi
)

run_compiler_mode_suite() {
  rm -rf \
    /var/cache/metaflux/compiler/.state \
    /var/cache/metaflux/compiler/users \
    /var/lib/metaflux/aot/epoch-1
  install -d -m 0750 -o metaflux -g metaflux /var/cache/metaflux/compiler
  install -d -m 0755 -o root -g root /var/lib/metaflux/aot

  run_cuda_acceptance_mode interpreter 1
  run_cuda_acceptance_mode cold-jit 1
  test "$(find /var/cache/metaflux/compiler/users -type f -name kernel.so | wc -l)" -eq 1
  run_cuda_acceptance_mode warm-jit 1

  METAFLUX_CPU_EXECUTION_MODE=aot \
    /usr/bin/metafluxd --prewarm-aot /artifacts/add-u32.ptx \
      > /tmp/metaflux-aot-prewarm.log 2>&1
  grep -E '^metafluxd: AOT prewarm compiled cache-key=mf-cache-v1-[0-9a-f]{64}$' \
    /tmp/metaflux-aot-prewarm.log
  METAFLUX_CPU_EXECUTION_MODE=aot \
    /usr/bin/metafluxd --prewarm-aot /artifacts/add-u32.ptx \
      > /tmp/metaflux-aot-prewarm-hit.log 2>&1
  grep -E '^metafluxd: AOT prewarm hit cache-key=mf-cache-v1-[0-9a-f]{64}$' \
    /tmp/metaflux-aot-prewarm-hit.log
  test "$(find /var/lib/metaflux/aot/epoch-1 -type f -name kernel.so | wc -l)" -eq 1
  test "$(find /var/lib/metaflux/aot/epoch-1 -type f -name kernel.so -perm /0222 | wc -l)" -eq 0
  run_cuda_acceptance_mode aot 1
  touch /tmp/metaflux-compiler-mode-suite-complete
}

run_cuda_acceptance() {
  run_cuda_acceptance_mode interpreter 0
}

verify_payload() {
  test -x /artifacts/cuda-add-copy
  test -x /artifacts/activation-launcher
  test -f /artifacts/add-u32.ptx
  test -x /usr/bin/metafluxd
  test -x /usr/libexec/metaflux/ld.lld
  test -f /usr/lib/udev/rules.d/70-metaflux.rules
  grep -Fx 'SUBSYSTEM=="misc", KERNEL=="metafluxctl", GROUP="metaflux", MODE="0660"' \
    /usr/lib/udev/rules.d/70-metaflux.rules
  grep -Fx 'SUBSYSTEM=="misc", KERNEL=="metaflux[0-9]*", GROUP="metaflux", MODE="0660"' \
    /usr/lib/udev/rules.d/70-metaflux.rules
  test ! -e /usr/lib/metaflux/runtime
  test -f /usr/share/metaflux/toolchains/ubuntu-20.04-target-sdk.manifest
  test -f /usr/share/metaflux/toolchains/generic-llvm-toolchain.manifest
  test -f /usr/lib/metaflux/providers/libcuda.so.1.0.0
  test -f /usr/lib/metaflux/providers/libnvidia-ml.so.1.0.0
  test -L /usr/lib/metaflux/providers/libcuda.so.1
  test -L /usr/lib/metaflux/providers/libnvidia-ml.so.1
  test "$(readlink /usr/lib/metaflux/providers/libcuda.so.1)" = libcuda.so.1.0.0
  test "$(readlink /usr/lib/metaflux/providers/libnvidia-ml.so.1)" = libnvidia-ml.so.1.0.0
  /usr/bin/metafluxd --version | grep -Fx 'metafluxd 0.1.0'
  /usr/libexec/metaflux/ld.lld --version | grep -F 'LLD 22.1.8'
  LD_LIBRARY_PATH=/usr/lib/metaflux/providers ldd \
    /usr/lib/metaflux/providers/libcuda.so.1 > /tmp/metaflux-ldd-cuda
  LD_LIBRARY_PATH=/usr/lib/metaflux/providers ldd \
    /usr/lib/metaflux/providers/libnvidia-ml.so.1 > /tmp/metaflux-ldd-nvml
  LD_LIBRARY_PATH=/usr/lib/metaflux/providers ldd \
    /artifacts/cuda-add-copy > /tmp/metaflux-ldd-acceptance
  grep -E '^[[:space:]]*libcuda\.so\.1 => /usr/lib/metaflux/providers/libcuda\.so\.1 ' \
    /tmp/metaflux-ldd-acceptance
  ldd /usr/bin/metafluxd > /tmp/metaflux-ldd-daemon
  ldd /usr/libexec/metaflux/ld.lld > /tmp/metaflux-ldd-lld
  assert_no_fixed_match 'not found' \
    /tmp/metaflux-ldd-cuda /tmp/metaflux-ldd-nvml \
    /tmp/metaflux-ldd-acceptance /tmp/metaflux-ldd-daemon /tmp/metaflux-ldd-lld
  assert_no_fixed_match '/nix/store/' \
    /tmp/metaflux-ldd-cuda /tmp/metaflux-ldd-nvml \
    /tmp/metaflux-ldd-acceptance /tmp/metaflux-ldd-daemon /tmp/metaflux-ldd-lld
  assert_no_extended_match 'version .* not found|error while loading shared libraries' \
    /tmp/metaflux-ldd-cuda /tmp/metaflux-ldd-nvml \
    /tmp/metaflux-ldd-acceptance /tmp/metaflux-ldd-daemon /tmp/metaflux-ldd-lld
  assert_no_recursive_match /nix/store/ \
    /usr/bin/metafluxd \
    /usr/include/metaflux \
    /usr/lib/metaflux \
    /usr/libexec/metaflux \
    /usr/share/metaflux
  verify_lifecycle_state
  if test "$RUN_COMPILER_MODES" -eq 1 && \
    test ! -e /tmp/metaflux-compiler-mode-suite-complete; then
    run_compiler_mode_suite
  else
    run_cuda_acceptance
  fi
}

create_metaflux_account_fallback() {
  if ! getent group metaflux >/dev/null; then
    if command -v groupadd >/dev/null 2>&1; then
      groupadd --system metaflux
    elif command -v addgroup >/dev/null 2>&1; then
      addgroup --system metaflux
    else
      echo "groupadd or addgroup is required" >&2
      return 1
    fi
  fi
  if ! getent passwd metaflux >/dev/null; then
    shell=/usr/sbin/nologin
    test -x "$shell" || shell=/sbin/nologin
    test -x "$shell" || shell=/bin/false
    if command -v useradd >/dev/null 2>&1; then
      useradd --system --gid metaflux --home-dir /var/lib/metaflux \
        --no-create-home --shell "$shell" \
        --comment "MetaFlux compute service" metaflux
    elif command -v adduser >/dev/null 2>&1; then
      adduser --system --ingroup metaflux --home /var/lib/metaflux \
        --no-create-home --disabled-login \
        --gecos "MetaFlux compute service" metaflux
    else
      echo "useradd or adduser is required" >&2
      return 1
    fi
  fi
}

apply_tar_lifecycle_metadata() {
  test -f /usr/lib/sysusers.d/metaflux.conf
  test -f /usr/lib/tmpfiles.d/metaflux.conf
  test -f /usr/lib/udev/rules.d/70-metaflux.rules
  grep -F 'u metaflux ' /usr/lib/sysusers.d/metaflux.conf
  grep -F 'd /var/cache/metaflux 0750 metaflux metaflux' \
    /usr/lib/tmpfiles.d/metaflux.conf
  if command -v systemd-sysusers >/dev/null 2>&1; then
    systemd-sysusers /usr/lib/sysusers.d/metaflux.conf
  else
    create_metaflux_account_fallback
  fi
  if command -v systemd-tmpfiles >/dev/null 2>&1; then
    systemd-tmpfiles --create /usr/lib/tmpfiles.d/metaflux.conf
  fi
  install -d -m 0755 -o root -g metaflux /run/metaflux
  install -d -m 0750 -o metaflux -g metaflux /var/cache/metaflux
  install -d -m 0700 -o metaflux -g metaflux \
    /var/cache/metaflux/compiler/users
  install -d -m 0750 -o metaflux -g metaflux /var/lib/metaflux
  install -d -m 0755 -o root -g root /var/lib/metaflux/aot
}

verify_lifecycle_state() {
  getent passwd metaflux >/tmp/metaflux-passwd
  getent group metaflux >/tmp/metaflux-group
  test "$(cut -d: -f6 /tmp/metaflux-passwd)" = /var/lib/metaflux
  service_uid=$(cut -d: -f3 /tmp/metaflux-passwd)
  service_gid=$(cut -d: -f3 /tmp/metaflux-group)
  test "$(stat -c %a:%u:%g /run/metaflux)" = "755:0:$service_gid"
  test "$(stat -c %a:%u:%g /var/cache/metaflux)" = \
    "750:$service_uid:$service_gid"
  test "$(stat -c %a:%u:%g /var/cache/metaflux/compiler/users)" = \
    "700:$service_uid:$service_gid"
  test "$(stat -c %a:%u:%g /var/lib/metaflux)" = \
    "750:$service_uid:$service_gid"
  test "$(stat -c %a:%u:%g /var/lib/metaflux/aot)" = 755:0:0
  test ! -e /run/metafluxd-package-upgrade.state
}

mark_persistent_state() {
  runuser -u metaflux -- sh -c \
    'printf "%s\n" preserved > /var/lib/metaflux/package-state-marker'
  runuser -u metaflux -- sh -c \
    'printf "%s\n" preserved > /var/cache/metaflux/package-cache-marker'
}

verify_persistent_state() {
  verify_lifecycle_state
  grep -Fx preserved /var/lib/metaflux/package-state-marker
  grep -Fx preserved /var/cache/metaflux/package-cache-marker
}

install_fake_systemd_runtime() {
  fake_bin=/tmp/metaflux-fake-systemd
  FAKE_SYSTEMD_LOG=/tmp/metaflux-fake-systemd.log
  mkdir -p "$fake_bin" /run/systemd/system
  cat > "$fake_bin/systemctl" <<'EOF'
#!/bin/sh
printf 'systemctl %s\n' "$*" >> "$FAKE_SYSTEMD_LOG"
case "$1" in
  is-active)
    test "$FAKE_SYSTEMD_ACTIVE" = 1
    ;;
  is-enabled)
    test "$FAKE_SYSTEMD_ENABLED" = 1
    ;;
  *)
    exit 0
    ;;
esac
EOF
  cat > "$fake_bin/deb-systemd-invoke" <<'EOF'
#!/bin/sh
printf 'deb-systemd-invoke %s\n' "$*" >> "$FAKE_SYSTEMD_LOG"
exit 0
EOF
  cat > "$fake_bin/deb-systemd-helper" <<'EOF'
#!/bin/sh
printf 'deb-systemd-helper %s\n' "$*" >> "$FAKE_SYSTEMD_LOG"
exit 0
EOF
  chmod 0755 \
    "$fake_bin/systemctl" \
    "$fake_bin/deb-systemd-invoke" \
    "$fake_bin/deb-systemd-helper"
  PATH="$fake_bin:$PATH"
  export PATH FAKE_SYSTEMD_LOG
}

set_fake_systemd_state() {
  FAKE_SYSTEMD_ACTIVE=$1
  FAKE_SYSTEMD_ENABLED=$2
  export FAKE_SYSTEMD_ACTIVE FAKE_SYSTEMD_ENABLED
  : > "$FAKE_SYSTEMD_LOG"
}

verify_inactive_upgrade_preserved() {
  grep -F 'systemctl is-active --quiet metafluxd.socket' "$FAKE_SYSTEMD_LOG"
  grep -F 'systemctl is-enabled --quiet metafluxd.socket' "$FAKE_SYSTEMD_LOG"
  grep -E '^(systemctl|deb-systemd-invoke) stop metafluxd.socket metafluxd.service$' \
    "$FAKE_SYSTEMD_LOG"
  grep -F 'systemctl daemon-reload' "$FAKE_SYSTEMD_LOG"
  assert_no_extended_match \
    '^(systemctl|deb-systemd-invoke|deb-systemd-helper) (preset|start|enable) ' \
    "$FAKE_SYSTEMD_LOG"
  test ! -e /run/metafluxd-package-upgrade.state
}

verify_active_enabled_upgrade_restored() {
  grep -E '^(systemctl|deb-systemd-helper) enable metafluxd.socket$' \
    "$FAKE_SYSTEMD_LOG"
  grep -E '^(systemctl|deb-systemd-invoke) start metafluxd.socket$' \
    "$FAKE_SYSTEMD_LOG"
  assert_no_extended_match \
    '^(systemctl|deb-systemd-invoke|deb-systemd-helper) preset ' \
    "$FAKE_SYSTEMD_LOG"
  test ! -e /run/metafluxd-package-upgrade.state
}

verify_fresh_install_started() {
  grep -F 'systemctl daemon-reload' "$FAKE_SYSTEMD_LOG"
  grep -E '^(systemctl|deb-systemd-helper) preset metafluxd.socket$' \
    "$FAKE_SYSTEMD_LOG"
  grep -E '^(systemctl|deb-systemd-invoke) start metafluxd.socket$' \
    "$FAKE_SYSTEMD_LOG"
  assert_no_fixed_match 'systemctl is-active --quiet metafluxd.socket' \
    "$FAKE_SYSTEMD_LOG"
  assert_no_fixed_match 'systemctl is-enabled --quiet metafluxd.socket' \
    "$FAKE_SYSTEMD_LOG"
  test ! -e /run/metafluxd-package-upgrade.state
}

verify_remove_stopped() {
  grep -E '^(systemctl|deb-systemd-invoke) stop metafluxd.socket metafluxd.service$' \
    "$FAKE_SYSTEMD_LOG"
  grep -F 'systemctl daemon-reload' "$FAKE_SYSTEMD_LOG"
  assert_no_extended_match \
    '^(systemctl|deb-systemd-invoke|deb-systemd-helper) (preset|start|enable) ' \
    "$FAKE_SYSTEMD_LOG"
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
    /usr/bin/metafluxd \
    /usr/include/metaflux \
    /usr/lib/libmetaflux_cuda_passthrough.a \
    /usr/lib/metaflux \
    /usr/lib/systemd/system/metafluxd.service \
    /usr/lib/systemd/system/metafluxd.socket \
    /usr/lib/udev/rules.d/70-metaflux.rules \
    /usr/lib/sysusers.d/metaflux.conf \
    /usr/lib/tmpfiles.d/metaflux.conf \
    /usr/libexec/metaflux \
    /usr/share/doc/metaflux \
    /usr/share/metaflux; do
    test ! -e "$path"
  done
}
"""
)


DEB_SCRIPT = VERIFY_PAYLOAD + r"""
set -eu
VENDOR_DIR=/usr/lib/x86_64-linux-gnu
verify_platform
prepare_vendor_sentinels

dpkg -i /artifacts/current.deb
test "$(dpkg-query -W -f='${Version}' metaflux)" = 0.1.0
dpkg -i /artifacts/current.deb
verify_payload
mark_persistent_state
verify_vendor_sentinels
dpkg -r metaflux
verify_removed
verify_persistent_state
verify_vendor_sentinels
dpkg --purge metaflux
verify_removed
verify_persistent_state

install_fake_systemd_runtime
set_fake_systemd_state 0 0
dpkg -i /artifacts/current.deb
verify_fresh_install_started
set_fake_systemd_state 0 0
dpkg --purge metaflux
verify_remove_stopped
verify_removed
verify_persistent_state

mkdir -p /tmp/prior/DEBIAN /tmp/prior/usr/share/metaflux
cat > /tmp/prior/DEBIAN/control <<'EOF'
Package: metaflux
Version: 0.0.0
Architecture: amd64
Maintainer: MetaFlux Project <noreply@metaflux.invalid>
Section: devel
Priority: optional
Description: MetaFlux complete-package upgrade fixture
EOF
printf '%s\n' prior-version > /tmp/prior/usr/share/metaflux/prior-version
dpkg-deb --build --root-owner-group /tmp/prior /tmp/metaflux-prior.deb
dpkg -i /tmp/metaflux-prior.deb
test "$(dpkg-query -W -f='${Version}' metaflux)" = 0.0.0
test -f /usr/share/metaflux/prior-version
set_fake_systemd_state 0 0
dpkg -i /artifacts/current.deb
verify_inactive_upgrade_preserved
test "$(dpkg-query -W -f='${Version}' metaflux)" = 0.1.0
test ! -e /usr/share/metaflux/prior-version
set_fake_systemd_state 1 1
dpkg -i /artifacts/current.deb
verify_active_enabled_upgrade_restored
verify_payload
verify_vendor_sentinels
dpkg --purge metaflux
verify_removed
verify_persistent_state
verify_vendor_sentinels
"""


RPM_SCRIPT = VERIFY_PAYLOAD + r"""
set -eu
VENDOR_DIR=/usr/lib64
verify_platform
prepare_vendor_sentinels

rpm -ivh /artifacts/current.rpm
test "$(rpm -q --queryformat '%{VERSION}-%{RELEASE}' metaflux)" = 0.1.0-1
rpm -Uvh --replacepkgs /artifacts/current.rpm
verify_payload
mark_persistent_state
verify_vendor_sentinels
rpm -e metaflux
verify_removed
verify_persistent_state
verify_vendor_sentinels

install_fake_systemd_runtime
ln -s /tmp/metaflux-fake-systemd/systemctl /usr/bin/systemctl
set_fake_systemd_state 0 0
rpm -ivh /artifacts/current.rpm
verify_fresh_install_started
set_fake_systemd_state 0 0
rpm -e metaflux
verify_remove_stopped
verify_removed
verify_persistent_state

rpm -ivh /artifacts/prior.rpm
test "$(rpm -q --queryformat '%{VERSION}-%{RELEASE}' metaflux)" = 0.0.0-1
test -f /usr/share/metaflux/prior-version
set_fake_systemd_state 0 0
rpm -Uvh /artifacts/current.rpm
verify_inactive_upgrade_preserved
test "$(rpm -q --queryformat '%{VERSION}-%{RELEASE}' metaflux)" = 0.1.0-1
test ! -e /usr/share/metaflux/prior-version
set_fake_systemd_state 1 1
rpm -Uvh --replacepkgs /artifacts/current.rpm
verify_active_enabled_upgrade_restored
verify_payload
verify_vendor_sentinels
rpm -e metaflux
verify_removed
verify_persistent_state
verify_vendor_sentinels
"""


TAR_SCRIPT = VERIFY_PAYLOAD + r"""
set -eu
verify_platform
prepare_vendor_sentinels

remove_tar_payload() {
  rm -rf /usr/lib/metaflux /usr/libexec/metaflux /usr/include/metaflux /usr/share/metaflux
  rm -rf /usr/share/doc/metaflux
  rm -f /usr/bin/metafluxd /usr/lib/libmetaflux_cuda_passthrough.a
  rm -f /usr/lib/systemd/system/metafluxd.service /usr/lib/systemd/system/metafluxd.socket
  rm -f /usr/lib/udev/rules.d/70-metaflux.rules
  rm -f /usr/lib/sysusers.d/metaflux.conf /usr/lib/tmpfiles.d/metaflux.conf
}

cp -a /artifacts/tar-root/. /
apply_tar_lifecycle_metadata
verify_payload
mark_persistent_state
verify_vendor_sentinels
remove_tar_payload
verify_removed
verify_persistent_state
verify_vendor_sentinels

mkdir -p /usr/lib/metaflux/providers /usr/share/metaflux
printf '%s\n' prior-version > /usr/share/metaflux/prior-version
printf '%s\n' old-provider > /usr/lib/metaflux/providers/libcuda.so.1.0.0
cp -a /artifacts/tar-root/. /
apply_tar_lifecycle_metadata
test "$(head -c 4 /usr/lib/metaflux/providers/libcuda.so.1.0.0)" = "$(printf '\177ELF')"
verify_payload
verify_vendor_sentinels
remove_tar_payload
verify_removed
verify_persistent_state
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
    expected_compiler_modes = (
        ["interpreter", "cold-jit", "warm-jit", "aot"]
        if name.startswith("ubuntu-20.04")
        else []
    )
    command = [
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
        "--env",
        f"RUN_COMPILER_MODES={1 if expected_compiler_modes else 0}",
        image,
        "/bin/sh",
        "-c",
        script,
    ]
    record = command_record(command)
    evidence_prefix = "METAFLUX_COMPILER_MODE_EVIDENCE mode="
    compiler_mode_evidence = [
        line for line in record["stdout"] if line.startswith(evidence_prefix)
    ]
    observed_compiler_modes = [
        line[len(evidence_prefix) :].split(" ", maxsplit=1)[0]
        for line in compiler_mode_evidence
    ]
    compiler_modes_complete = observed_compiler_modes == expected_compiler_modes
    return {
        "name": name,
        "format": package_format,
        "expected_pretty_name": expected_pretty,
        "expected_glibc": expected_glibc,
        "status": (
            "passed"
            if record["returncode"] == 0 and compiler_modes_complete
            else "failed"
        ),
        "compiler_mode_evidence": {
            "expected": expected_compiler_modes,
            "observed": observed_compiler_modes,
            "records": compiler_mode_evidence,
        },
        "command": record,
    }


def main() -> int:
    arguments = parse_arguments()
    try:
        deb = require_file(arguments.deb, "DEB")
        rpm = require_file(arguments.rpm, "RPM")
        tarball = require_file(arguments.tarball, "tar archive")
        cuda_acceptance = require_file(arguments.cuda_acceptance, "CUDA acceptance binary")
        activation_launcher = require_file(
            cuda_acceptance.with_name("metaflux-activation-launcher"),
            "socket activation launcher next to the CUDA acceptance binary",
        )
        add_u32_ptx = require_file(
            cuda_acceptance.with_name("metaflux-add-u32.ptx"),
            "Add PTX fixture next to the CUDA acceptance binary",
        )
        fixture_abi = {
            "cuda_acceptance": validate_release_fixture(
                cuda_acceptance, arguments.readelf, needs_cuda=True
            ),
            "activation_launcher": validate_release_fixture(
                activation_launcher, arguments.readelf, needs_cuda=False
            ),
        }
        images = [getattr(arguments, field) for _, field, _, _ in UBUNTU_CASES]
        images.append(getattr(arguments, ROCKY_CASE[1]))
        require_digest_pinned_images(images)

        rpmbuild = arguments.rpmbuild or shutil.which("rpmbuild")
        if rpmbuild is None:
            raise ValueError("rpmbuild is required to construct the old-version RPM fixture")
        rpmbuild = str(Path(rpmbuild).resolve(strict=True))

        arguments.output_dir.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="metaflux-release-matrix-") as temporary_text:
            workspace = Path(temporary_text)
            inputs = workspace / "inputs"
            inputs.mkdir()
            shutil.copy2(deb, inputs / "current.deb")
            shutil.copy2(rpm, inputs / "current.rpm")
            shutil.copy2(tarball, inputs / "current.tar.gz")
            shutil.copy2(cuda_acceptance, inputs / "cuda-add-copy")
            (inputs / "cuda-add-copy").chmod(0o755)
            shutil.copy2(activation_launcher, inputs / "activation-launcher")
            (inputs / "activation-launcher").chmod(0o755)
            shutil.copy2(add_u32_ptx, inputs / "add-u32.ptx")
            tar_members = extract_tar_payload(tarball, inputs / "tar-root")
            prior_rpm, rpmbuild_record = build_prior_rpm(rpmbuild, workspace)
            shutil.copy2(prior_rpm, inputs / "prior.rpm")

            inspections = {image: image_inspect(arguments.podman, image) for image in images}
            cases: list[dict[str, Any]] = []
            for name, field, pretty, glibc in UBUNTU_CASES:
                image = getattr(arguments, field)
                for package_format in ("deb", "tar"):
                    cases.append(
                        run_container_case(
                            arguments.podman,
                            image,
                            inputs,
                            name,
                            package_format,
                            pretty,
                            glibc,
                        )
                    )
            rocky_name, rocky_field, rocky_pretty, rocky_glibc = ROCKY_CASE
            rocky_image = getattr(arguments, rocky_field)
            for package_format in ("rpm", "tar"):
                cases.append(
                    run_container_case(
                        arguments.podman,
                        rocky_image,
                        inputs,
                        rocky_name,
                        package_format,
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
                "harness": harness_fingerprint(
                    Path(__file__), Path(provider_matrix.__file__)
                ),
                "inputs": {
                    "deb": {"path": str(deb), "sha256": sha256(deb)},
                    "rpm": {"path": str(rpm), "sha256": sha256(rpm)},
                    "tar": {"path": str(tarball), "sha256": sha256(tarball)},
                    "cuda_acceptance": {
                        "path": str(cuda_acceptance),
                        "sha256": sha256(cuda_acceptance),
                    },
                    "activation_launcher": {
                        "path": str(activation_launcher),
                        "sha256": sha256(activation_launcher),
                    },
                    "fixture_abi": fixture_abi,
                    "add_u32_ptx": {
                        "path": str(add_u32_ptx),
                        "sha256": sha256(add_u32_ptx),
                    },
                    "prior_rpm_sha256": sha256(inputs / "prior.rpm"),
                },
                "tar_extraction": {
                    "implementation": "python.tarfile data filter with traversal check",
                    "members": tar_members,
                },
                "images": inspections,
                "prior_rpm_build": rpmbuild_record,
                "cases": cases,
            }
            report_path = arguments.output_dir / "release-package-matrix.json"
            write_json(report_path, report)
            print(report_path)
            return 0 if passed else 1
    except (OSError, ValueError, RuntimeError, json.JSONDecodeError) as error:
        print(f"release matrix setup failed: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
