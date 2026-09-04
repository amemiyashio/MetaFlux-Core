#!/usr/bin/env python3
"""Batch-0002 debug-kernel qualification supply path.

Builds the out-of-tree ``metaflux_core.ko`` against the pinned 6.12.105 debug
kernel, cross-builds a statically linked
``metaflux_transport_cdev_live_qualification`` guest binary, packs a busybox
initramfs, boots it under QEMU (``-kernel``/``-initrd``, serial console), and
parses the console for the guest markers, the ``mf_cdev_generation`` KUnit TAP
results, the kmemleak scan, and post-module-load kernel warnings.

This runner is the supply path for the batch-0002 host gates. It does NOT
claim any sanitizer gate closed: it reports which qualification CONFIGs the
final kernel ``.config`` actually carries (KASAN and KCSAN may be mutually
exclusive on 6.12) and records the guest evidence as JSON.

Phases (each independently skippable, all default on):
  kernel                bzImage present and its final .config sufficient
  module                metaflux_core.ko (+ mf_cdev_generation_kunit.ko)
  qualification-binary  static metaflux_transport_cdev_live_qualification
  guest                 initramfs build, QEMU boot, console parse

Exit codes: 0 pass, 1 fail, 77 skip (missing prerequisite such as the bzImage,
cmake, qemu, or STATIC_BUSYBOX; CTest SKIP_RETURN_CODE convention). An
explicit --skip-* does not produce 77 by itself.

Tool environments (see kernel/tests/README.md):
  nix develop .#linux-debug --command python3 -B tools/run-debug-kernel-qualification.py ...
  nix develop .#vfio-user  --command env STATIC_BUSYBOX=$(command -v busybox) python3 -B tools/run-debug-kernel-qualification.py ...
"""

from __future__ import annotations

import argparse
import gzip
import json
import os
import re
import shutil
import stat
import struct
import subprocess
import sys
from pathlib import Path

PASS = 0
FAIL = 1
SKIP = 77

REPOSITORY = Path(__file__).resolve().parent.parent
DEFAULT_CACHE_DIR = REPOSITORY.parent / ".metaflux-build" / "MetaFlux-Core" / "debug-kernel"

# The five qualification configs whose presence is reported from the final
# kernel .config (the authoritative result of tools/build-debug-kernel.sh).
QUALIFICATION_CONFIGS = (
    "CONFIG_KUNIT",
    "CONFIG_KASAN",
    "CONFIG_KCSAN",
    "CONFIG_DEBUG_KMEMLEAK",
    "CONFIG_PROVE_LOCKING",
)

# Guest bootability configs: warned about, not reported as the five.
BOOT_CONFIGS = (
    "CONFIG_MODULES",
    "CONFIG_SERIAL_8250_CONSOLE",
    "CONFIG_DEVTMPFS_MOUNT",
)

KERNEL_WARNING_PATTERNS = (
    ("kasan", "BUG: KASAN"),
    ("kcsan", "BUG: KCSAN"),
    ("lockdep", re.compile(r"WARNING:.*lockdep")),
    ("lockdep-cycle", "WARNING: possible circular locking dependency detected"),
    ("rcu-stall", "INFO: rcu"),
    ("kernel-bug", "kernel BUG"),
)

REQUIRED_BUSYBOX_APPLETS = (
    "sh",
    "mount",
    "insmod",
    "dmesg",
    "cat",
    "sleep",
    "poweroff",
    "tail",
    "mkdir",
)

MODULE_BUILD_TIMEOUT = 900
CMAKE_CONFIGURE_TIMEOUT = 300
CMAKE_BUILD_TIMEOUT = 1200
DEFAULT_GUEST_TIMEOUT = 300

MARKER_BOOT = "GUEST:BOOT"
MARKER_MODULE_LOADED = "GUEST:MODULE_LOADED"
MARKER_KUNIT_LOADED = "GUEST:KUNIT_LOADED"
MARKER_QUALIFICATION_RC = "GUEST:QUALIFICATION_RC="
MARKER_KMEMLEAK_BEGIN = "GUEST:KMEMLEAK_BEGIN"
MARKER_KMEMLEAK_END = "GUEST:KMEMLEAK_END"

KERNEL_PRINTK_PREFIX = re.compile(r"^\[\s*\d+\.\d+\]\s?")

# Kernel messages immediately before the module-load marker belong to insmod;
# look back this far so insmod-time reports are not silently skipped.
WARNING_LOOKBACK_BYTES = 2000

# Static device nodes the initramfs needs before devtmpfs is mounted.
INITRAMFS_DEVICES = {
    "dev/console": (5, 1),
    "dev/null": (1, 3),
}


def skip(message: str) -> int:
    print(f"debug-kernel qualification: SKIP: {message}", file=sys.stderr, flush=True)
    return SKIP


def fail(message: str, excerpts: list[str]) -> int:
    print(f"debug-kernel qualification: FAIL: {message}", file=sys.stderr, flush=True)
    for line in excerpts[-40:]:
        print(f"  | {line}", file=sys.stderr, flush=True)
    return FAIL


# ---------------------------------------------------------------------------
# Importable pure helpers (exercised by tools/test-run-debug-kernel-qualification.py)
# ---------------------------------------------------------------------------


TRACKED_CONFIGS = QUALIFICATION_CONFIGS + BOOT_CONFIGS
NOT_SET_PREFIX = "# "
NOT_SET_SUFFIX = " is not set"


def read_kernel_config(path: Path) -> dict[str, str]:
    """Parse the final kernel .config into y/m/n/absent per tracked CONFIG.

    Tracks the five qualification CONFIGs plus the guest bootability set;
    ``# CONFIG_X is not set`` comments map to "n".
    """
    values = {config: "absent" for config in TRACKED_CONFIGS}
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return values
    for raw in text.splitlines():
        line = raw.strip()
        if line.startswith(NOT_SET_PREFIX) and line.endswith(NOT_SET_SUFFIX):
            key = line[len(NOT_SET_PREFIX) : -len(NOT_SET_SUFFIX)].strip()
            if key in values:
                values[key] = "n"
            continue
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, _, value = line.partition("=")
        key = key.strip()
        value = value.strip().strip('"')
        if key in values:
            values[key] = value if value in ("y", "m", "n") else "absent"
    return values


def clean_console_line(line: str) -> str:
    """Strip a leading printk timestamp prefix such as '[    2.345678] '."""
    return KERNEL_PRINTK_PREFIX.sub("", line)


def parse_kunit_tap(text: str) -> dict | None:
    """Parse the KUnit TAP output for the mf_cdev_generation suite.

    Console lines may carry printk timestamp prefixes and the dmesg dump at the
    end repeats the live block, so subtests are counted as distinct names.
    Returns None when the console carries no TAP evidence at all.
    """
    tap_found = False
    suite_ok = False
    suite_not_ok = False
    passed: set[str] = set()
    failed: set[str] = set()
    for raw in text.splitlines():
        line = clean_console_line(raw)
        stripped = line.strip()
        if re.match(r"^(?:KTAP|TAP) version \d+", stripped):
            tap_found = True
        if re.match(r"^ok \d+\s+-?\s*mf_cdev_generation(?:\s|$)", stripped):
            suite_ok = True
        if re.match(r"^not ok \d+\s+-?\s*mf_cdev_generation(?:\s|$)", stripped):
            suite_not_ok = True
        match = re.match(r"^\s*(not ok|ok) \d+\s+-?\s*(mf_cdev_generation_test_\S+)", stripped)
        if match is not None:
            (failed if match.group(1) == "not ok" else passed).add(match.group(2))
    if not (tap_found or suite_ok or suite_not_ok or passed or failed):
        return None
    return {
        "tap_found": tap_found,
        "suite_found": suite_ok or suite_not_ok,
        "suite_ok": suite_ok and not suite_not_ok,
        "passed": len(passed),
        "failed": len(failed),
        "ok": suite_ok and not suite_not_ok and len(passed) > 0 and not failed,
    }


def extract_kmemleak(text: str) -> dict:
    """Extract the GUEST:KMEMLEAK_BEGIN..END block (empty or comment-only = clean)."""
    begin = text.find(MARKER_KMEMLEAK_BEGIN)
    end = text.find(MARKER_KMEMLEAK_END)
    if begin < 0 or end < 0 or end < begin:
        return {"scanned": False, "empty": False, "report_lines": []}
    block = text[begin + len(MARKER_KMEMLEAK_BEGIN) : end]
    report_lines = [
        line
        for line in block.splitlines()
        if line.strip() and not line.lstrip().startswith("#")
    ]
    return {"scanned": True, "empty": not report_lines, "report_lines": report_lines}


def find_kernel_warnings(text: str) -> list[str]:
    """Return console lines after (just before) the module-load markers that
    match KASAN/KCSAN/lockdep/RCU/kernel-BUG patterns."""
    offsets = [
        offset
        for offset in (text.find(MARKER_MODULE_LOADED), text.find(MARKER_KUNIT_LOADED))
        if offset >= 0
    ]
    start = max(0, min(offsets) - WARNING_LOOKBACK_BYTES) if offsets else 0
    found: list[str] = []
    for raw in text[start:].splitlines():
        line = clean_console_line(raw)
        if not any(
            (pattern in line) if isinstance(pattern, str) else (pattern.search(line) is not None)
            for _, pattern in KERNEL_WARNING_PATTERNS
        ):
            continue
        if line not in found:
            found.append(line)
        if len(found) >= 50:
            break
    return found


def parse_guest_output(text: str) -> dict:
    """Parse the full guest console into verification evidence."""
    rc_match = re.search(re.escape(MARKER_QUALIFICATION_RC) + r"(-?\d+)", text)
    return {
        "boot": MARKER_BOOT in text,
        "module_loaded": MARKER_MODULE_LOADED in text,
        "kunit_loaded": MARKER_KUNIT_LOADED in text,
        "qualification_rc": int(rc_match.group(1)) if rc_match else None,
        "kunit": parse_kunit_tap(text),
        "kmemleak": extract_kmemleak(text),
        "warnings": find_kernel_warnings(text),
        "powered_off": "reboot: Power down" in text,
    }


def guest_init_script(library_path: str = "") -> str:
    """Minimal busybox ash /init. Applets are invoked via /bin/busybox because
    standalone-shell fallback is not guaranteed in the packaged busybox."""
    library_export = (
        f'export LD_LIBRARY_PATH="{library_path}"\n' if library_path else ""
    )
    return f"""#!/bin/sh
export PATH=/bin:/sbin:/usr/bin:/usr/sbin
{library_export}/bin/busybox mkdir -p /proc /sys /dev /sys/kernel/debug
/bin/busybox mount -t proc proc /proc
/bin/busybox mount -t sysfs sysfs /sys
/bin/busybox mount -t devtmpfs devtmpfs /dev || /bin/busybox mount -t tmpfs tmpfs /dev
[ -e /dev/metaflux0 ] || /bin/busybox mdev -s 2>/dev/null
/bin/busybox mount -t debugfs debugfs /sys/kernel/debug
/bin/busybox dmesg -n 8
echo GUEST:BOOT
/bin/busybox insmod /ko/metaflux_core.ko && echo GUEST:MODULE_LOADED
if [ -f /ko/mf_cdev_generation_kunit.ko ]; then
  /bin/busybox insmod /ko/mf_cdev_generation_kunit.ko && echo GUEST:KUNIT_LOADED
fi
/bin/metaflux_transport_cdev_live_qualification
echo GUEST:QUALIFICATION_RC=$?
if [ -w /sys/kernel/debug/kmemleak ]; then
  echo scan > /sys/kernel/debug/kmemleak
  /bin/busybox sleep 2
  echo GUEST:KMEMLEAK_BEGIN
  /bin/busybox cat /sys/kernel/debug/kmemleak
  echo GUEST:KMEMLEAK_END
fi
/bin/busybox dmesg | /bin/busybox tail -n 200
/bin/busybox poweroff -f
/bin/busybox sleep 30
echo GUEST:POWEROFF_STUCK
while :; do /bin/busybox sleep 60; done
"""


def pack_cpio_newc(entries: list[tuple[str, int, int, int, bytes | None]]) -> bytes:
    """Pack (name, mode, size, rdev, data) entries into a newc cpio archive.

    rdev packs major/minor via os.makedev for character devices; data is None
    for directories and device nodes.
    """
    output = bytearray()
    inode = 0
    for name, mode, size, rdev, data in entries:
        inode += 1
        encoded = name.encode("utf-8") + b"\0"
        rdev_major = os.major(rdev) if rdev else 0
        rdev_minor = os.minor(rdev) if rdev else 0
        nlink = 2 if stat.S_ISDIR(mode) else 1
        fields = (
            inode,
            mode,
            0,
            0,
            nlink,
            0,
            size,
            0,
            0,
            rdev_major,
            rdev_minor,
            len(encoded),
            0,
        )
        header = b"070701" + b"".join(f"{value:08X}".encode("ascii") for value in fields)
        output += header + encoded
        while len(output) % 4 != 0:
            output += b"\0"
        if data is not None:
            output += data
            while len(output) % 4 != 0:
                output += b"\0"
    trailer = b"TRAILER!!!\0"
    fields = (0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, len(trailer), 0)
    output += b"070701" + b"".join(f"{value:08X}".encode("ascii") for value in fields)
    output += trailer
    while len(output) % 4 != 0:
        output += b"\0"
    return bytes(output)


def module_vermagic(path: Path) -> str | None:
    """Best-effort vermagic extraction from a .ko (bytes scan of .modinfo)."""
    try:
        data = path.read_bytes()
    except OSError:
        return None
    index = data.find(b"vermagic=")
    if index < 0:
        return None
    chunk = data[index + len(b"vermagic=") : index + len(b"vermagic=") + 128]
    return chunk.split(b"\0", 1)[0].decode("latin-1").strip()


def module_release_matches(path: Path, kernel_release: str) -> bool | None:
    """True/False when the module's vermagic release token is comparable."""
    vermagic = module_vermagic(path)
    if not vermagic or not kernel_release:
        return None
    return vermagic.split()[0] == kernel_release


def kernel_release(build_dir: Path) -> str:
    try:
        return (
            (build_dir / "include" / "config" / "kernel.release")
            .read_text(encoding="utf-8")
            .strip()
        )
    except OSError:
        return ""


def elf_has_interpreter(path: Path) -> bool | None:
    """True when the ELF carries a PT_INTERP segment (i.e. is dynamic)."""
    try:
        with path.open("rb") as handle:
            header = handle.read(64)
        if len(header) < 64 or header[:4] != b"\x7fELF":
            return None
        endian = "<" if header[5] == 1 else ">"
        if header[4] == 2:  # ELFCLASS64
            phoff = struct.unpack_from(endian + "Q", header, 0x20)[0]
            phentsize, phnum = struct.unpack_from(endian + "HH", header, 0x36)
        elif header[4] == 1:  # ELFCLASS32
            phoff = struct.unpack_from(endian + "I", header, 0x1C)[0]
            phentsize, phnum = struct.unpack_from(endian + "HH", header, 0x2A)
        else:
            return None
        with path.open("rb") as handle:
            handle.seek(phoff)
            program = handle.read(phentsize * phnum)
        for index in range(phnum):
            entry = program[index * phentsize : (index + 1) * phentsize]
            if struct.unpack_from(endian + "I", entry, 0)[0] == 3:  # PT_INTERP
                return True
        return False
    except OSError:
        return None


def command_tail(result: subprocess.CompletedProcess | subprocess.TimeoutExpired, lines: int = 30) -> list[str]:
    stdout = getattr(result, "stdout", "") or ""
    stderr = getattr(result, "stderr", "") or ""
    combined = (stdout + "\n" + stderr).splitlines()
    return combined[-lines:]


def run_command(
    command: list[str], timeout: int, env: dict[str, str] | None = None
) -> subprocess.CompletedProcess:
    return subprocess.run(
        command,
        capture_output=True,
        text=True,
        errors="replace",
        timeout=timeout,
        check=False,
        env=env,
    )


# ---------------------------------------------------------------------------
# Initramfs assembly
# ---------------------------------------------------------------------------


def elf_interp(path: Path) -> str | None:
    """Return the PT_INTERP string of an ELF, or None for static/unreadable."""
    try:
        with path.open("rb") as handle:
            header = handle.read(64)
        if len(header) < 64 or header[:4] != b"\x7fELF":
            return None
        endian = "<" if header[5] == 1 else ">"
        if header[4] == 2:  # ELFCLASS64
            phoff = struct.unpack_from(endian + "Q", header, 0x20)[0]
            phentsize, phnum = struct.unpack_from(endian + "HH", header, 0x36)
        else:
            return None
        with path.open("rb") as handle:
            handle.seek(phoff)
            program = handle.read(phentsize * phnum)
            for index in range(phnum):
                entry = program[index * phentsize : (index + 1) * phentsize]
                p_type = struct.unpack_from(endian + "I", entry, 0)[0]
                if p_type != 3:  # PT_INTERP
                    continue
                offset = struct.unpack_from(endian + "Q", entry, 0x08)[0]
                filesz = struct.unpack_from(endian + "Q", entry, 0x20)[0]
                handle.seek(offset)
                return handle.read(filesz).split(b"\0")[0].decode("utf-8", "replace")
    except (OSError, struct.error):
        return None
    return None


def collect_glibc_runtime(binary: Path) -> tuple[list[tuple[Path, Path]], list[str]]:
    """glibc-only guest runtime collection (no musl).

    Returns ((source, destination-inside-staging) pairs, library directories).
    The interpreter is staged at its exact absolute PT_INTERP path; every
    resolved NEEDED library is staged at its host absolute path too, and its
    directory feeds LD_LIBRARY_PATH inside the guest.
    """
    interp_value = elf_interp(binary)
    if not interp_value:
        return [], []
    files: list[tuple[Path, Path]] = []
    directories: list[str] = []
    interp = Path(interp_value)
    if interp.is_file():
        files.append((interp, Path(interp.as_posix().lstrip("/"))))
        directories.append(str(interp.parent))
    trace = run_command(
        [str(binary)],
        60,
        env={**os.environ, "LD_TRACE_LOADED_OBJECTS": "1"},
    )
    for line in trace.stdout.splitlines():
        match = re.search(r"=>\s*(/\S+?)\s*\(", line)
        if match is None:
            match = re.match(r"^\s*(/\S+?)\s*\(", line)
        if match is None:
            continue
        library = Path(match.group(1))
        if not library.is_file():
            continue
        files.append((library, Path(library.as_posix().lstrip("/"))))
        if str(library.parent) not in directories:
            directories.append(str(library.parent))
    return files, directories


def build_initramfs(
    staging: Path,
    busybox: Path,
    core_module: Path,
    qualification_binary: Path,
    kunit_module: Path | None,
    init_text: str,
    runtime_files: list[tuple[Path, Path]] | None = None,
) -> bytes:
    if staging.exists():
        shutil.rmtree(staging)
    (staging / "bin").mkdir(parents=True)
    (staging / "ko").mkdir(parents=True)
    for directory in ("proc", "sys", "dev", "tmp", "root"):
        (staging / directory).mkdir(parents=True)

    shutil.copyfile(busybox, staging / "bin" / "busybox")
    (staging / "bin" / "busybox").chmod(0o755)
    (staging / "bin" / "sh").symlink_to("busybox")
    shutil.copyfile(core_module, staging / "ko" / "metaflux_core.ko")
    (staging / "ko" / "metaflux_core.ko").chmod(0o644)
    shutil.copyfile(qualification_binary, staging / "bin" / "metaflux_transport_cdev_live_qualification")
    (staging / "bin" / "metaflux_transport_cdev_live_qualification").chmod(0o755)
    if kunit_module is not None:
        shutil.copyfile(kunit_module, staging / "ko" / "mf_cdev_generation_kunit.ko")
        (staging / "ko" / "mf_cdev_generation_kunit.ko").chmod(0o644)
    for source, destination in runtime_files or []:
        target = staging / destination
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
        target.chmod(0o755)

    init_path = staging / "init"
    init_path.write_text(init_text, encoding="utf-8")
    init_path.chmod(0o755)

    entries: list[tuple[str, int, int, int, bytes | None]] = []
    for current, directories, files in os.walk(staging):
        directories.sort()
        files.sort()
        base = Path(current)
        for name in directories:
            relative = (base / name).relative_to(staging).as_posix()
            entries.append((relative, 0o040755, 0, 0, None))
        for name in files:
            path = base / name
            relative = path.relative_to(staging).as_posix()
            if path.is_symlink():
                target = os.readlink(path).encode("utf-8")
                entries.append((relative, 0o120777, len(target), 0, target))
            else:
                data = path.read_bytes()
                mode = 0o100755 if path.stat().st_mode & stat.S_IXUSR else 0o100644
                entries.append((relative, mode, len(data), 0, data))

    for name, (major, minor) in INITRAMFS_DEVICES.items():
        entries.append((name, 0o020644, 0, os.makedev(major, minor), None))

    return pack_cpio_newc(entries)


# ---------------------------------------------------------------------------
# Runner
# ---------------------------------------------------------------------------


def resolve_cache_dir(arguments: argparse.Namespace) -> Path:
    explicit = (
        arguments.cache_dir
        or os.environ.get("METAFLUX_DEBUG_KERNEL_DIR")
        or str(DEFAULT_CACHE_DIR)
    )
    return Path(explicit).resolve()


def resolve_qualification_dir(arguments: argparse.Namespace, cache_dir: Path) -> Path:
    explicit = (
        arguments.qualification_dir
        or os.environ.get("METAFLUX_QUALIFICATION_STATIC_DIR")
        or str(cache_dir / "qualification-static")
    )
    return Path(explicit).resolve()


def find_qualification_binary(qualification_dir: Path) -> Path | None:
    candidates = sorted(
        qualification_dir.glob("**/metaflux_transport_cdev_live_qualification")
    )
    return candidates[0] if candidates else None


def make_environment() -> dict[str, str]:
    """Environment for kernel-module make: drop CC/CXX overrides so Kbuild
    uses its default host compiler even inside clang-based shells, and drop
    musl toolchain PATH entries whose wrappers break glibc-linked host
    build tools (e.g. the Nix libz against the musl loader)."""
    env = dict(os.environ)
    env.pop("CC", None)
    env.pop("CXX", None)
    path_entries = env.get("PATH", "").split(":")
    env["PATH"] = ":".join(
        entry for entry in path_entries if entry and "musl" not in entry.lower()
    )
    return env


def build_modules(
    cache_dir: Path, repository: Path, kunit_enabled: bool
) -> tuple[Path | None, Path | None, list[str]]:
    """Build metaflux_core.ko and, when enabled, the KUnit module.

    Returns (core_module, kunit_module_or_None, warnings). Raises RuntimeError
    when the core module build fails.
    """
    warnings: list[str] = []
    overlay = cache_dir / "src"
    build_dir = cache_dir / "build"
    command = [
        "make",
        "-C",
        str(overlay),
        f"O={build_dir}",
        f"M={repository / 'kernel' / 'core'}",
        "ARCH=x86_64",
        "modules",
    ]
    result = run_command(command, MODULE_BUILD_TIMEOUT, env=make_environment())
    core_module = repository / "kernel" / "core" / "metaflux_core.ko"
    if result.returncode != 0 or not core_module.is_file():
        raise RuntimeError(f"metaflux_core.ko module build failed:\n{result.stdout[-2000:]}\n{result.stderr[-2000:]}")

    kunit_module = None
    if kunit_enabled:
        kunit_command = [
            "make",
            "-C",
            str(overlay),
            f"O={build_dir}",
            f"M={repository / 'kernel' / 'tests' / 'kunit'}",
            "ARCH=x86_64",
            "modules",
        ]
        kunit_result = run_command(kunit_command, MODULE_BUILD_TIMEOUT, env=make_environment())
        candidate = repository / "kernel" / "tests" / "kunit" / "mf_cdev_generation_kunit.ko"
        if kunit_result.returncode == 0 and candidate.is_file():
            kunit_module = candidate
        else:
            warnings.append(
                "mf_cdev_generation_kunit.ko build failed; KUnit phase recorded as unavailable "
                f"(tail: {' / '.join(command_tail(kunit_result, lines=5))})"
            )
    return core_module, kunit_module, warnings


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cache-dir", default=None, help="Debug kernel cache dir (default: METAFLUX_DEBUG_KERNEL_DIR or %(default)s)")
    parser.add_argument("--qualification-dir", default=None, help="Static qualification CMake build dir (default: METAFLUX_QUALIFICATION_STATIC_DIR or <cache-dir>/qualification-static)")
    parser.add_argument("--repository", type=Path, default=REPOSITORY)
    parser.add_argument("--static-busybox", default=None, help="Static busybox for the guest (default: STATIC_BUSYBOX env)")
    parser.add_argument("--qemu", default=None, help="qemu-system-x86_64 path (default: PATH lookup)")
    parser.add_argument("--timeout", type=int, default=DEFAULT_GUEST_TIMEOUT, help="Guest boot timeout seconds")
    parser.add_argument("--output", default=None, help="Write the JSON summary to this path too")
    parser.add_argument("--skip-kernel", action="store_true", help="Skip the bzImage/.config phase")
    parser.add_argument("--skip-module", action="store_true", help="Skip the kernel module build phase")
    parser.add_argument("--skip-qualification-binary", action="store_true", help="Skip the static qualification binary build phase")
    parser.add_argument("--skip-guest", action="store_true", help="Skip the guest boot phase")
    parser.add_argument(
        "--force-tcg",
        action="store_true",
        help="Boot under TCG with a baseline CPU model even when /dev/kvm is writable",
    )
    arguments = parser.parse_args(argv)

    repository = arguments.repository.resolve()
    cache_dir = resolve_cache_dir(arguments)
    qualification_dir = resolve_qualification_dir(arguments, cache_dir)

    summary: dict = {
        "kernel_config": {},
        "phases": {},
        "kmemleak_empty": None,
        "kunit": None,
        "warnings": [],
        "result": None,
    }

    def emit_summary() -> None:
        json_text = json.dumps(summary, indent=2, sort_keys=True)
        print(json_text, flush=True)
        if arguments.output:
            Path(arguments.output).write_text(json_text + "\n", encoding="utf-8")

    # --- Phase: KERNEL -----------------------------------------------------
    config_values: dict[str, str] = {}
    if arguments.skip_kernel:
        summary["phases"]["kernel"] = {"status": "skipped"}
    else:
        bz_image = cache_dir / "build" / "arch" / "x86_64" / "boot" / "bzImage"
        if not bz_image.is_file():
            return skip(
                f"no bzImage at {bz_image}; run tools/build-debug-kernel.sh "
                "inside 'nix develop .#linux-debug' first"
            )
        config_path = cache_dir / "build" / ".config"
        if not config_path.is_file():
            summary["result"] = "fail"
            emit_summary()
            return fail(f"kernel .config missing at {config_path}", [])
        config_values = read_kernel_config(config_path)
        summary["kernel_config"] = {
            key: value for key, value in config_values.items() if key in QUALIFICATION_CONFIGS
        }
        phase_warnings: list[str] = []
        if config_values.get("CONFIG_MODULES") != "y":
            summary["result"] = "fail"
            emit_summary()
            return fail("kernel lacks CONFIG_MODULES=y; metaflux_core.ko cannot load", [])
        sanitizers = [c for c in ("CONFIG_KASAN", "CONFIG_KCSAN") if config_values.get(c) == "y"]
        if not sanitizers:
            summary["result"] = "fail"
            emit_summary()
            return fail("kernel lacks both CONFIG_KASAN and CONFIG_KCSAN=y; nothing to qualify", [])
        if config_values.get("CONFIG_KUNIT") != "y":
            phase_warnings.append("CONFIG_KUNIT is not =y in the final .config; KUnit phase unavailable")
        if config_values.get("CONFIG_DEBUG_KMEMLEAK") != "y":
            phase_warnings.append("CONFIG_DEBUG_KMEMLEAK is not =y in the final .config; kmemleak scan unavailable")
        if config_values.get("CONFIG_PROVE_LOCKING") != "y":
            phase_warnings.append("CONFIG_PROVE_LOCKING is not =y; lockdep coverage absent")
        for boot_config in BOOT_CONFIGS:
            if config_values.get(boot_config) != "y":
                phase_warnings.append(f"{boot_config} is not =y; guest boot may fail")
        summary["warnings"].extend(phase_warnings)
        summary["phases"]["kernel"] = {
            "status": "pass",
            "bz_image": str(bz_image),
            "config": str(config_path),
            "warnings": phase_warnings,
        }
    kunit_enabled = config_values.get("CONFIG_KUNIT") == "y"
    kmemleak_enabled = config_values.get("CONFIG_DEBUG_KMEMLEAK") == "y"

    # --- Phase: MODULE -----------------------------------------------------
    kunit_module_path: Path | None = None
    if arguments.skip_module:
        summary["phases"]["module"] = {"status": "skipped"}
    else:
        try:
            core_module, kunit_module_path, module_warnings = build_modules(
                cache_dir, repository, kunit_enabled
            )
        except RuntimeError as error:
            summary["result"] = "fail"
            emit_summary()
            return fail(str(error).splitlines()[0], str(error).splitlines()[1:])
        summary["warnings"].extend(module_warnings)
        summary["phases"]["module"] = {
            "status": "pass",
            "module": str(core_module),
            "kunit_module": str(kunit_module_path) if kunit_module_path else None,
            "warnings": module_warnings,
        }

    # --- Phase: QUALIFICATION BINARY ---------------------------------------
    qualification_binary = find_qualification_binary(qualification_dir)
    if arguments.skip_qualification_binary:
        summary["phases"]["qualification_binary"] = {
            "status": "skipped",
            "binary": str(qualification_binary) if qualification_binary else None,
        }
    else:
        cmake = shutil.which("cmake")
        ninja = shutil.which("ninja")
        if cmake is None or ninja is None:
            return skip(
                "cmake/ninja not found on PATH; run the qualification-binary phase "
                "from a shell that provides them (e.g. 'nix develop .')"
            )
        # glibc-only (constraint): build the qualification binary normally
        # (dynamically linked) and ship the glibc runtime it needs inside the
        # guest image. A static glibc pull would drag its bootstrap through
        # IFUNC-selected EVEX paths that fault under TCG.
        configure = run_command(
            [
                cmake,
                "-S",
                str(repository),
                "-B",
                str(qualification_dir),
                "-G",
                "Ninja",
                "-DCMAKE_BUILD_TYPE=Release",
                # Guest qualification has no daemon; with the daemon target
                # off the binary skips its CDEV_BIND section.
                "-DMETAFLUX_BUILD_DAEMON=OFF",
                "-DMETAFLUX_BUILD_TESTS=ON",
            ],
            CMAKE_CONFIGURE_TIMEOUT,
        )
        if configure.returncode != 0:
            summary["result"] = "fail"
            emit_summary()
            return fail(
                "static qualification cmake configure failed",
                command_tail(configure),
            )
        build = run_command(
            [cmake, "--build", str(qualification_dir), "--target", "metaflux_transport_cdev_live_qualification"],
            CMAKE_BUILD_TIMEOUT,
        )
        qualification_binary = find_qualification_binary(qualification_dir)
        if build.returncode != 0 or qualification_binary is None:
            summary["result"] = "fail"
            emit_summary()
            return fail(
                "static qualification binary build failed"
                if build.returncode != 0
                else "metaflux_transport_cdev_live_qualification not found after build",
                command_tail(build),
            )
        summary["phases"]["qualification_binary"] = {
            "status": "pass",
            "binary": str(qualification_binary),
            "build_dir": str(qualification_dir),
        }

    # --- Phase: GUEST RUN --------------------------------------------------
    if arguments.skip_guest:
        summary["phases"]["guest_run"] = {"status": "skipped"}
        executed_any = any(
            phase.get("status") == "pass" for phase in summary["phases"].values()
        )
        summary["result"] = "pass" if executed_any else "skip"
        emit_summary()
        return PASS if executed_any else SKIP

    busybox = arguments.static_busybox or os.environ.get("STATIC_BUSYBOX")
    if not busybox:
        return skip(
            "STATIC_BUSYBOX is unset; provide a static busybox path "
            "(the 'nix develop .#vfio-user' shell packages one via 'command -v busybox')"
        )
    busybox_path = Path(busybox)
    if not busybox_path.is_file():
        return skip(f"STATIC_BUSYBOX {busybox_path} does not exist")
    applets = run_command([str(busybox_path), "--list"], 60)
    available = set(applets.stdout.split())
    missing_applets = [a for a in REQUIRED_BUSYBOX_APPLETS if a not in available]
    if applets.returncode != 0 or missing_applets:
        return skip(
            f"{busybox_path} is not a usable busybox (missing applets: {', '.join(missing_applets) or 'unknown'})"
        )
    dynamic = elf_has_interpreter(busybox_path)
    if dynamic is True:
        return skip(f"{busybox_path} is dynamically linked; the guest needs a static busybox")
    if dynamic is None:
        return skip(f"{busybox_path} is not a readable ELF binary")

    qemu = arguments.qemu or shutil.which("qemu-system-x86_64")
    if qemu is None:
        return skip("qemu-system-x86_64 not found; run inside 'nix develop .#vfio-user'")

    core_module = repository / "kernel" / "core" / "metaflux_core.ko"
    if not core_module.is_file():
        summary["result"] = "fail"
        emit_summary()
        return fail(
            f"{core_module} missing; run the module phase (drop --skip-module)", []
        )
    release = kernel_release(cache_dir / "build")
    release_match = module_release_matches(core_module, release)
    if release_match is False:
        vermagic = module_vermagic(core_module)
        summary["result"] = "fail"
        emit_summary()
        return fail(
            f"metaflux_core.ko vermagic '{vermagic}' does not match kernel release "
            f"'{release}'; rebuild via the module phase",
            [],
        )

    kunit_shipped = False
    if kunit_enabled:
        candidate = repository / "kernel" / "tests" / "kunit" / "mf_cdev_generation_kunit.ko"
        if candidate.is_file():
            kunit_match = module_release_matches(candidate, release)
            if kunit_match is False:
                summary["warnings"].append(
                    f"mf_cdev_generation_kunit.ko vermagic "
                    f"'{module_vermagic(candidate)}' does not match kernel release "
                    f"'{release}'; not shipped to the guest"
                )
            else:
                kunit_shipped = True
        else:
            summary["warnings"].append(
                "mf_cdev_generation_kunit.ko unavailable; KUnit phase recorded as not attempted"
            )

    if qualification_binary is None:
        summary["result"] = "fail"
        emit_summary()
        return fail(
            "metaflux_transport_cdev_live_qualification not found; run the "
            "qualification-binary phase (drop --skip-qualification-binary)",
            [],
        )

    staging = cache_dir / "qualification-initramfs"
    initramfs_path = cache_dir / "qualification-initramfs.cpio.gz"
    console_log = cache_dir / "qualification-console.log"
    try:
        runtime_files, library_dirs = collect_glibc_runtime(qualification_binary)
        if not runtime_files:
            summary["warnings"].append(
                "qualification binary carries no PT_INTERP; expected a dynamic "
                "glibc build for the glibc-only guest runtime"
            )
        initramfs_bytes = build_initramfs(
            staging,
            busybox_path,
            core_module,
            qualification_binary,
            repository / "kernel" / "tests" / "kunit" / "mf_cdev_generation_kunit.ko"
            if kunit_shipped
            else None,
            guest_init_script(":".join(library_dirs)),
            runtime_files=runtime_files,
        )
        summary["phases"].setdefault("guest_run", {})["runtime_files"] = len(runtime_files)
    except OSError as error:
        summary["result"] = "fail"
        emit_summary()
        return fail(f"initramfs staging failed: {error}", [])
    initramfs_path.write_bytes(gzip.compress(initramfs_bytes, compresslevel=6, mtime=0))

    # KVM exposes the host CPU's CET/AVX512 CPUID bits; the guest glibc's
    # IFUNCs then select paths that QEMU's trap handling may not model,
    # surfacing as invalid-opcode traps. TCG with a conservative CPU model
    # keeps the guest on baseline instructions, which is plenty for the
    # second-scale qualification run.
    use_kvm = os.access("/dev/kvm", os.R_OK | os.W_OK) and not arguments.force_tcg
    accel = ["-enable-kvm"] if use_kvm else ["-accel", "tcg", "-cpu", "qemu64"]
    qemu_command = [
        qemu,
        "-m",
        "4G",
        "-kernel",
        str(cache_dir / "build" / "arch" / "x86_64" / "boot" / "bzImage"),
        "-initrd",
        str(initramfs_path),
        "-append",
        "console=ttyS0 oops=panic panic=-1 kasan_multi_shot",
        "-nographic",
        "-no-reboot",
        *accel,
    ]
    try:
        qemu_result = subprocess.run(
            qemu_command,
            stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            errors="replace",
            timeout=arguments.timeout,
            check=False,
        )
        console_text = qemu_result.stdout or ""
    except subprocess.TimeoutExpired as error:
        console_text = error.stdout or "" if isinstance(error.stdout, str) else ""
        console_log.write_text(console_text, encoding="utf-8")
        summary["phases"]["guest_run"] = {
            "status": "fail",
            "console_log": str(console_log),
            "accel": "kvm" if use_kvm else "tcg",
            "reason": f"qemu timed out after {arguments.timeout}s",
        }
        summary["result"] = "fail"
        emit_summary()
        return fail(
            f"qemu timed out after {arguments.timeout}s",
            console_text.splitlines()[-40:],
        )
    console_log.write_text(console_text, encoding="utf-8")

    evidence = parse_guest_output(console_text)
    problems: list[str] = []
    if not evidence["boot"]:
        problems.append("guest boot marker missing (kernel did not reach /init)")
    elif not evidence["module_loaded"]:
        problems.append("metaflux_core.ko did not load (GUEST:MODULE_LOADED missing)")
    if evidence["qualification_rc"] is None:
        problems.append("qualification binary produced no GUEST:QUALIFICATION_RC marker")
    elif evidence["qualification_rc"] != 0:
        problems.append(f"qualification binary exited {evidence['qualification_rc']}")
    for warning_line in evidence["warnings"]:
        problems.append(f"kernel warning: {warning_line}")

    kmemleak = evidence["kmemleak"]
    if kmemleak_enabled:
        if not kmemleak["scanned"]:
            problems.append("kmemleak scan did not run (GUEST:KMEMLEAK markers missing)")
        else:
            summary["kmemleak_empty"] = kmemleak["empty"]
            if not kmemleak["empty"]:
                problems.append("kmemleak reported leaks")
    else:
        summary["warnings"].append(
            "kernel lacks CONFIG_DEBUG_KMEMLEAK=y; kmemleak scan not evaluated"
        )

    if kunit_shipped:
        if not evidence["kunit_loaded"]:
            problems.append("mf_cdev_generation_kunit.ko did not load (GUEST:KUNIT_LOADED missing)")
            summary["kunit"] = {"attempted": True, "loaded": False}
        else:
            tap = evidence["kunit"] or {}
            summary["kunit"] = {
                "attempted": True,
                "loaded": True,
                "tap_found": tap.get("tap_found", False),
                "suite_found": tap.get("suite_found", False),
                "suite_ok": tap.get("suite_ok", False),
                "passed": tap.get("passed", 0),
                "failed": tap.get("failed", 0),
                "ok": tap.get("ok", False),
            }
            if not tap.get("ok"):
                problems.append(
                    "KUnit TAP for mf_cdev_generation missing or failing "
                    f"(tap_found={tap.get('tap_found')}, suite_found={tap.get('suite_found')}, "
                    f"passed={tap.get('passed')}, failed={tap.get('failed')})"
                )
    else:
        summary["kunit"] = None

    summary["warnings"].extend(evidence["warnings"])
    summary["phases"]["guest_run"] = {
        "status": "fail" if problems else "pass",
        "console_log": str(console_log),
        "accel": "kvm" if use_kvm else "tcg",
        "boot": evidence["boot"],
        "module_loaded": evidence["module_loaded"],
        "kunit_loaded": evidence["kunit_loaded"],
        "qualification_rc": evidence["qualification_rc"],
        "kunit_shipped": kunit_shipped,
        "problems": problems,
    }
    summary["result"] = "fail" if problems else "pass"
    emit_summary()
    if problems:
        return fail(
            f"{len(problems)} guest qualification problem(s); first: {problems[0]}",
            console_text.splitlines()[-40:],
        )
    print(
        "debug-kernel qualification: PASS "
        f"(accel={'kvm' if use_kvm else 'tcg'}, qualification_rc="
        f"{evidence['qualification_rc']}, kmemleak_empty={summary['kmemleak_empty']}, "
        f"kunit={'ok' if (summary['kunit'] or {}).get('ok') else 'not-attempted'})",
        flush=True,
    )
    return PASS


if __name__ == "__main__":
    sys.exit(main())
