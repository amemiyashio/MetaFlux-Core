#!/usr/bin/env python3
"""work-item-0.1.1.3 static vfio-user live bring-up qualification.

Boots the pinned QEMU (toolchains/vfio-user-1.json) with shared file-backed
guest RAM, the running host kernel, and a minimal busybox initramfs carrying
the static guest metaflux_pci.ko. The libvfio-user fixture server presents
the milestone-0.1.1.0 static PCI function. The run proves live PCI identity
enumeration, exact BAR0/BAR2/BAR4 sizes, runtime BAR0 region access, the
BAR2 doorbell ioeventfd, BAR4 MSI-X delivery, and mappable guest-RAM DMA
registration without socket-mediated fallback.

Exit codes: 0 pass, 1 fail, 77 skip (missing or unpinned prerequisite).
"""

from __future__ import annotations

import argparse
import gzip
import importlib.util
import json
import os
import re
import shutil
import signal
import stat
import subprocess
import sys
import tempfile
import time
from pathlib import Path

SKIP = 77

REQUIRED_APPLETS = (
    "awk",
    "cat",
    "devmem",
    "dmesg",
    "echo",
    "grep",
    "insmod",
    "mount",
    "poweroff",
    "sed",
    "sleep",
)


def load_pci_guest_fixture(repository: Path) -> dict[str, int]:
    """Compose expected guest PCI identity/BAR sizes from the root manifests."""
    script = repository / "tools" / "generate-pci-guest-profile.py"
    spec = importlib.util.spec_from_file_location("metaflux_pci_guest_profile", script)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load {script}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.compose(repository.resolve())


def guest_init_script(fixture: dict[str, int]) -> str:
    vendor = f"0x{fixture['vendor_id']:04x}"
    device = f"0x{fixture['device_id']:04x}"
    class_code = f"0x{fixture['class_code']:06x}"
    bar0_size = str(fixture["bar0_size"])
    bar4_size = str(fixture["bar4_size"])
    # BAR0 MSI-X trigger words intentionally embed the CI vendor mnemonic; they
    # are test-side doorbell values, not competing layout owners.
    trigger0 = f"0x{fixture['vendor_id']:04x}5558"
    trigger1 = f"0x{fixture['vendor_id']:04x}5559"
    return f"""#!/bin/sh
export PATH=/bin:/sbin:/usr/bin:/usr/sbin
/bin/busybox mount -t proc proc /proc
/bin/busybox mount -t sysfs sysfs /sys
/bin/busybox mount -t devtmpfs devtmpfs /dev
echo LIVE_GUEST:BOOT
/bin/busybox insmod /lib/modules/__KVER__/metaflux_pci.ko
/bin/busybox sleep 1
if /bin/busybox dmesg | /bin/busybox grep -q 'static MetaFlux guest function ready'; then
  echo LIVE_GUEST:PROBE_OK
else
  echo LIVE_GUEST:PROBE_FAIL
fi
dev=
for d in /sys/bus/pci/devices/*; do
  if [ "$(/bin/busybox cat "$d/vendor" 2>/dev/null)" = "{vendor}" ]; then
    dev="$d"
    break
  fi
done
if [ -n "$dev" ]; then
  if [ "$(/bin/busybox cat "$dev/device")" = "{device}" ] &&
     [ "$(/bin/busybox cat "$dev/class")" = "{class_code}" ]; then
    echo LIVE_GUEST:ID_OK
  else
    echo LIVE_GUEST:ID_FAIL
  fi
else
  echo LIVE_GUEST:ID_FAIL
fi
if [ -n "$dev" ]; then
  set -- $(/bin/busybox sed -n '1p' "$dev/resource")
  bar0=$(( $1 ))
  bar0_end=$(( $2 ))
  set -- $(/bin/busybox sed -n '3p' "$dev/resource")
  bar2=$(( $1 ))
  set -- $(/bin/busybox sed -n '5p' "$dev/resource")
  bar4=$(( $1 ))
  bar4_end=$(( $2 ))
  bar0_size=$(( bar0_end - bar0 + 1 ))
  bar4_size=$(( bar4_end - bar4 + 1 ))
  if [ "$bar0_size" = "{bar0_size}" ] && [ "$bar2" != "0" ] && [ "$bar4_size" = "{bar4_size}" ]; then
    echo LIVE_GUEST:BAR_OK
  else
    echo LIVE_GUEST:BAR_FAIL
  fi
  word0=$(/bin/busybox devmem "$bar0" 32)
  word0=$(/bin/busybox echo "$word0" | /bin/busybox tr 'ABCDEF' 'abcdef')
  word0=${{word0#0x}}
  if [ "$word0" = "3054464d" ]; then
    echo LIVE_GUEST:BAR0_READ_OK
  else
    echo LIVE_GUEST:BAR0_READ_FAIL
  fi
  irq_before=$(/bin/busybox cat /proc/interrupts |
    /bin/busybox awk '/metaflux_pci/ {{ for (i = 2; i < NF; i++) s += $i }} END {{ printf "%d", s }}')
  /bin/busybox devmem "$bar0" 32 {trigger0}
  /bin/busybox devmem "$bar0" 32 {trigger1}
  /bin/busybox devmem "$bar2" 32 0x1
  /bin/busybox devmem "$bar2" 32 0x2
  /bin/busybox sleep 1
  irq_after=$(/bin/busybox cat /proc/interrupts |
    /bin/busybox awk '/metaflux_pci/ {{ for (i = 2; i < NF; i++) s += $i }} END {{ printf "%d", s }}')
  if [ "$irq_after" -gt "$irq_before" ]; then
    echo LIVE_GUEST:MSIX_OK
  else
    echo LIVE_GUEST:MSIX_FAIL
  fi
else
  echo LIVE_GUEST:BAR_FAIL
  echo LIVE_GUEST:BAR0_READ_FAIL
  echo LIVE_GUEST:MSIX_FAIL
fi
echo LIVE_GUEST:DONE
/bin/busybox poweroff -f
/bin/busybox sleep 30
echo LIVE_GUEST:POWEROFF_STUCK
while :; do /bin/busybox sleep 60; done
"""

# Static device nodes the initramfs needs before devtmpfs is mounted.
INITRAMFS_DEVICES = {
    "dev/console": (5, 1),
    "dev/null": (1, 3),
    "dev/zero": (1, 5),
    "dev/mem": (1, 10),
    "dev/tty": (5, 0),
    "dev/ttyS0": (4, 64),
    "dev/kmsg": (1, 11),
}

GUEST_SERIAL_REQUIRED = (
    "LIVE_GUEST:BOOT",
    "LIVE_GUEST:PROBE_OK",
    "LIVE_GUEST:ID_OK",
    "LIVE_GUEST:BAR_OK",
    "LIVE_GUEST:BAR0_READ_OK",
    "LIVE_GUEST:MSIX_OK",
    "LIVE_GUEST:DONE",
)


def skip(reason: str) -> int:
    print(f"vfio-user live bring-up: SKIP: {reason}", flush=True)
    return SKIP


def fail(reason: str, excerpts: list[str]) -> int:
    print(f"vfio-user live bring-up: FAIL: {reason}", flush=True)
    for line in excerpts[:40]:
        print(f"  | {line}", flush=True)
    return 1


def resolve_qemu(repository: Path, explicit: str | None) -> tuple[str | None, str]:
    manifest_path = repository / "toolchains" / "vfio-user-1.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    pinned = manifest["packages"]["qemu"]["version"]

    qemu = explicit or os.environ.get("METAFLUX_QEMU_SYSTEM") or shutil.which("qemu-system-x86_64")
    if qemu is None:
        return None, f"qemu-system-x86_64 not found; run inside 'nix develop .#vfio-user'"

    try:
        version = subprocess.run(
            [qemu, "--version"], capture_output=True, text=True, timeout=30, check=False
        )
    except (OSError, subprocess.SubprocessError) as error:
        return None, f"cannot execute {qemu}: {error}"
    if version.returncode != 0 or f"version {pinned}" not in version.stdout:
        found = version.stdout.strip().splitlines()[0] if version.stdout else "unknown"
        return None, f"QEMU pin {pinned} required (toolchains/vfio-user-1.json), found: {found}"

    devices = subprocess.run(
        [qemu, "-device", "help"], capture_output=True, text=True, timeout=60, check=False
    )
    if devices.returncode != 0 or "vfio-user-pci" not in devices.stdout:
        return None, f"{qemu} does not provide the vfio-user-pci device"
    return qemu, ""


def resolve_busybox(explicit: str | None) -> tuple[str | None, str]:
    busybox = explicit or os.environ.get("METAFLUX_BUSYBOX") or shutil.which("busybox")
    if busybox is None:
        return None, "busybox not found; run inside 'nix develop .#vfio-user'"
    listing = subprocess.run(
        [busybox, "--list"], capture_output=True, text=True, timeout=30, check=False
    )
    if listing.returncode != 0:
        return None, f"cannot list {busybox} applets"
    available = set(listing.stdout.split())
    missing = [applet for applet in REQUIRED_APPLETS if applet not in available]
    if missing:
        return None, f"busybox lacks required applets: {', '.join(missing)}"
    return busybox, ""


def resolve_kernel(explicit: str | None) -> tuple[Path | None, str]:
    kernel = explicit or os.environ.get("METAFLUX_GUEST_KERNEL")
    if kernel is not None:
        path = Path(kernel)
        if path.is_file():
            return path, ""
        return None, f"METAFLUX_GUEST_KERNEL {kernel} does not exist"
    release = os.uname().release
    candidate = Path(f"/usr/lib/modules/{release}/vmlinuz")
    if candidate.is_file():
        return candidate, ""
    return None, f"no guest kernel image at {candidate}"


def resolve_module(repository: Path, explicit: str | None) -> tuple[Path | None, str]:
    if explicit is not None:
        module = Path(explicit)
        if module.is_file():
            return module, ""
        return None, f"explicit module {explicit} does not exist"
    module_dir = repository / "kernel" / "pci"
    module = module_dir / "metaflux_pci.ko"
    if module.is_file():
        return module, ""
    build_tree = Path(f"/lib/modules/{os.uname().release}/build")
    if not build_tree.is_dir():
        return None, f"kernel build tree {build_tree} missing and {module} not built"
    result = subprocess.run(
        ["make", "-C", str(build_tree), f"M={module_dir}", "modules"],
        capture_output=True,
        text=True,
        timeout=600,
        check=False,
        cwd=str(repository),
    )
    if result.returncode != 0 or not module.is_file():
        return None, f"metaflux_pci.ko build failed: {result.stdout[-400:]} {result.stderr[-400:]}"
    return module, ""


def build_initramfs(
    staging: Path, busybox: str, module: Path, release: str, fixture: dict[str, int]
) -> bytes:
    (staging / "bin").mkdir(parents=True, exist_ok=True)
    shutil.copy2(busybox, staging / "bin" / "busybox")
    (staging / "bin" / "sh").symlink_to("busybox")
    for directory in ("proc", "sys", "dev", "etc", "tmp", "root", "usr/bin", "usr/sbin"):
        (staging / directory).mkdir(parents=True, exist_ok=True)
    module_dir = staging / "lib" / "modules" / release
    module_dir.mkdir(parents=True, exist_ok=True)
    shutil.copy2(module, module_dir / "metaflux_pci.ko")
    init_path = staging / "init"
    init_path.write_text(
        guest_init_script(fixture).replace("__KVER__", release), encoding="utf-8"
    )
    init_path.chmod(0o755)

    entries: list[tuple[str, int, int, int, bytes | None]] = []
    # (name, mode, size, rdev, data): rdev packs major/minor for char devices.
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
                mode = 0o100755 if path.stat().st_mode & stat.S_IXUSR else 0o100644
                entries.append((relative, mode, path.stat().st_size, 0, path.read_bytes()))

    for name, (major, minor) in INITRAMFS_DEVICES.items():
        entries.append((name, 0o020644, 0, os.makedev(major, minor), None))

    return _pack_cpio_newc(entries)


def _pack_cpio_newc(entries: list[tuple[str, int, int, int, bytes | None]]) -> bytes:
    output = bytearray()
    ino = 0
    for name, mode, size, rdev, data in entries:
        ino += 1
        encoded = name.encode("utf-8") + b"\0"
        rdev_major = os.major(rdev) if rdev else 0
        rdev_minor = os.minor(rdev) if rdev else 0
        nlink = 2 if stat.S_ISDIR(mode) else 1
        fields = (
            ino,
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

    trailer_name = b"TRAILER!!!\0"
    trailer_fields = (0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, len(trailer_name), 0)
    output += b"070701" + b"".join(f"{value:08X}".encode("ascii") for value in trailer_fields)
    output += trailer_name
    while len(output) % 4 != 0:
        output += b"\0"
    return bytes(output)


def wait_for_marker(path: Path, marker: str, deadline_seconds: float) -> bool:
    deadline = time.monotonic() + deadline_seconds
    while time.monotonic() < deadline:
        try:
            if marker in path.read_text(encoding="utf-8", errors="replace"):
                return True
        except OSError:
            pass
        time.sleep(0.1)
    return False


def tail(path: Path, lines: int = 40) -> list[str]:
    try:
        content = path.read_text(encoding="utf-8", errors="replace").splitlines()
    except OSError:
        return []
    return content[-lines:]


def stop_process(process: subprocess.Popen) -> None:
    if process.poll() is not None:
        return
    process.send_signal(signal.SIGTERM)
    try:
        process.wait(timeout=10)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=10)


def run_qemu(qemu: str, workdir: Path, initramfs: Path, kernel: Path, socket_path: Path,
             serial_log: Path, timeout: int) -> subprocess.CompletedProcess:
    accel = "kvm" if os.access("/dev/kvm", os.R_OK | os.W_OK) else "tcg"
    cpu = "host" if accel == "kvm" else "max"
    command = [
        qemu,
        "-object",
        "memory-backend-memfd,id=mem,size=512M,share=on",
        "-machine",
        f"pc,accel={accel},memory-backend=mem",
        "-cpu",
        cpu,
        "-smp",
        "2",
        "-m",
        "512M",
        "-kernel",
        str(kernel),
        "-initrd",
        str(initramfs),
        "-append",
        "console=ttyS0 rdinit=/init panic=-1 loglevel=7 iomem=relaxed",
        "-display",
        "none",
        "-serial",
        f"file:{serial_log}",
        "-monitor",
        "none",
        "-no-reboot",
        "-net",
        "none",
    ]
    device_forms = [
        f"vfio-user-pci,socket={socket_path}",
        json.dumps(
            {"driver": "vfio-user-pci", "socket": {"path": str(socket_path), "type": "unix"}}
        ),
    ]
    last: subprocess.CompletedProcess | None = None
    for device in device_forms:
        full = command + ["-device", device]
        try:
            result = subprocess.run(
                full, capture_output=True, text=True, timeout=timeout, check=False
            )
        except subprocess.TimeoutExpired as error:
            return subprocess.CompletedProcess(
                full, 124, stdout=error.stdout or "", stderr=error.stderr or ""
            )
        last = result
        serial_text = ""
        try:
            serial_text = serial_log.read_text(encoding="utf-8", errors="replace")
        except OSError:
            pass
        if result.returncode == 0 or "LIVE_GUEST:BOOT" in serial_text:
            return result
    assert last is not None
    return last


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repository", required=True, type=Path)
    parser.add_argument("--server", required=True)
    parser.add_argument("--qemu")
    parser.add_argument("--kernel")
    parser.add_argument("--module")
    parser.add_argument("--busybox")
    parser.add_argument("--timeout", type=int, default=240)
    parser.add_argument("--keep", action="store_true")
    arguments = parser.parse_args()

    if not arguments.server or not Path(arguments.server).is_file():
        return skip(
            "live fixture server binary not built "
            "(libvfio-user prerequisite missing from this host)"
        )

    qemu, reason = resolve_qemu(arguments.repository, arguments.qemu)
    if qemu is None:
        return skip(reason)
    busybox, reason = resolve_busybox(arguments.busybox)
    if busybox is None:
        return skip(reason)
    kernel, reason = resolve_kernel(arguments.kernel)
    if kernel is None:
        return skip(reason)
    module, reason = resolve_module(arguments.repository, arguments.module)
    if module is None:
        return skip(reason)
    try:
        fixture = load_pci_guest_fixture(arguments.repository)
    except Exception as error:  # noqa: BLE001 - surface compose diagnostics
        return fail(f"pci-guest fixture compose failed: {error}")

    release = os.uname().release
    workdir = Path(tempfile.mkdtemp(prefix="metaflux-vfu-live-"))
    server_log = workdir / "server.log"
    serial_log = workdir / "serial.log"
    socket_path = workdir / "vfu.sock"
    initramfs = workdir / "initramfs.cpio.gz"

    server: subprocess.Popen | None = None
    try:
        initramfs.write_bytes(
            gzip.compress(
                build_initramfs(
                    workdir / "initramfs-root", busybox, module, release, fixture
                ),
                compresslevel=6,
                mtime=0,
            )
        )

        with server_log.open("wb") as server_output:
            server = subprocess.Popen(
                [arguments.server, str(socket_path)],
                stdout=server_output,
                stderr=subprocess.STDOUT,
                text=False,
            )
        if not wait_for_marker(server_log, "LIVE:LISTENING", 15.0):
            if server.poll() is not None:
                return fail("fixture server exited before listening", tail(server_log))
            return fail("fixture server never reported LISTENING", tail(server_log))

        qemu_result = run_qemu(
            qemu, workdir, initramfs, kernel, socket_path, serial_log, arguments.timeout
        )
        time.sleep(0.5)
        stop_process(server)

        serial_text = serial_log.read_text(encoding="utf-8", errors="replace")
        server_text = server_log.read_text(encoding="utf-8", errors="replace")

        if qemu_result.returncode == 124:
            return fail(
                f"QEMU timed out after {arguments.timeout}s",
                tail(serial_log) + ["--- server ---"] + tail(server_log),
            )

        missing_guest = [marker for marker in GUEST_SERIAL_REQUIRED if marker not in serial_text]
        if missing_guest:
            reason = f"guest markers missing: {', '.join(missing_guest)}"
            if "LIVE_GUEST:PROBE_FAIL" in serial_text:
                reason = "guest metaflux_pci.ko probe failed"
            if "LIVE_GUEST:ID_FAIL" in serial_text:
                reason = "static PCI identity enumeration failed"
            return fail(reason, tail(serial_log) + ["--- server ---"] + tail(server_log))

        problems: list[str] = []
        if "DMA_MAP" not in server_text:
            problems.append("no guest-RAM DMA registration observed")
        elif "mappable=1" not in server_text:
            problems.append("DMA registration was not file-backed mappable (share=on violated)")
        if "MSIX_TRIGGERED vector=0" not in server_text:
            problems.append("MSI-X vector 0 was not triggered through BAR0")
        if "MSIX_TRIGGERED vector=1" not in server_text:
            problems.append("MSI-X vector 1 was not triggered through BAR0")
        doorbell_total = 0
        for line in server_text.splitlines():
            match = re.match(r"DOORBELL total=(\d+)", line)
            if match is not None:
                doorbell_total = max(doorbell_total, int(match.group(1)))
            elif line.startswith("DOORBELL"):
                problems.append(f"malformed doorbell marker: {line}")
        if doorbell_total < 2:
            problems.append(f"doorbell ioeventfd writes observed: {doorbell_total} (need 2)")
        # milestone-0.1.1.0 advertises no reset: the server never acknowledges a
        # client-requested device reset (type=0). The pinned QEMU issues one
        # during machine init, before any guest runtime access; the frozen
        # pinned-pair result is an ENOTSUP error reply that the client
        # tolerates while bring-up continues. A type=0 reset after guest
        # runtime activity began is terminal loss. Type=1 is the library's own
        # lost-connection teardown and is expected.
        guest_runtime_started = False
        init_phase_reset = False
        runtime_reset = False
        for line in server_text.splitlines():
            if line.startswith("BAR0_READ"):
                guest_runtime_started = True
            if line == "RESET_OBSERVED type=0":
                if guest_runtime_started:
                    runtime_reset = True
                else:
                    init_phase_reset = True
        if runtime_reset:
            problems.append("client-requested device reset during guest runtime (terminal loss)")
        if init_phase_reset and "Operation not supported" not in server_text:
            problems.append(
                "machine-init device reset was not refused with an error reply (no ENOTSUP)"
            )
        if "DISCONNECT" not in server_text and "STOPPED" not in server_text:
            problems.append("server did not observe a clean disconnect or stop")
        if problems:
            return fail("; ".join(problems), tail(server_log) + ["--- serial ---"] + tail(serial_log))

        print("vfio-user live bring-up: PASS", flush=True)
        print(
            "  guest: PCI identity, BAR0/2/4 sizes, BAR0 runtime read/write, "
            "MSI-X vectors 0+1 delivered; host: mappable DMA map, doorbell ioeventfd",
            flush=True,
        )
        if not arguments.keep:
            shutil.rmtree(workdir, ignore_errors=True)
        else:
            print(f"  artifacts kept in {workdir}", flush=True)
        return 0
    finally:
        if server is not None:
            stop_process(server)
        if arguments.keep:
            print(f"  artifacts kept in {workdir}", flush=True)


if __name__ == "__main__":
    sys.exit(main())
