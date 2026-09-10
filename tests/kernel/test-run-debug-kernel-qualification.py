#!/usr/bin/env python3
"""Self-tests for tools/run-debug-kernel-qualification.py.

No real kernel, module, or guest is required. Covers:
  - missing bzImage -> the runner exits 77 (skip)
  - read_kernel_config against a .config fixture (y/m/n/absent)
  - kernel-phase fail (CONFIG_MODULES dropped) -> exit 1
  - kernel-phase pass -> exit 0 with the JSON summary shape
  - parse_guest_output markers, warnings window, rc, kmemleak, KUnit TAP
  - guest_init_script content and the newc cpio trailer
  - module vermagic extraction / release matching
"""

from __future__ import annotations

import importlib.util
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

REPOSITORY = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPOSITORY / "tools"))
RUNNER = REPOSITORY / "tools" / "run-debug-kernel-qualification.py"

BUILD_ONLY_FLAGS = [
    "--skip-module",
    "--skip-qualification-binary",
    "--skip-guest",
]


def load_runner():
    spec = importlib.util.spec_from_file_location(
        "run_debug_kernel_qualification", RUNNER
    )
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load {RUNNER}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def run_runner(cache_dir: Path, extra_args: list[str] | None = None) -> tuple[int, str, str]:
    env = dict(os.environ)
    env["METAFLUX_DEBUG_KERNEL_DIR"] = str(cache_dir)
    env.pop("STATIC_BUSYBOX", None)
    result = subprocess.run(
        [sys.executable, "-B", str(RUNNER), *(extra_args or [])],
        capture_output=True,
        text=True,
        env=env,
        timeout=120,
    )
    return result.returncode, result.stdout, result.stderr


def make_fake_cache(cache_dir: Path, config_pairs: dict[str, str]) -> None:
    boot = cache_dir / "build" / "arch" / "x86_64" / "boot"
    boot.mkdir(parents=True, exist_ok=True)
    (boot / "bzImage").write_bytes(b"fake bzImage")
    build = cache_dir / "build"
    build.mkdir(parents=True, exist_ok=True)
    lines = [f"# {key} placeholder" for key in ("dropped", "configs")]
    lines += [f"{key}={value}" for key, value in sorted(config_pairs.items())]
    (build / ".config").write_text("\n".join(lines) + "\n", encoding="utf-8")


FULL_CONFIG = {
    "CONFIG_KUNIT": "y",
    "CONFIG_KASAN": "y",
    "CONFIG_KCSAN": "y",
    "CONFIG_DEBUG_KMEMLEAK": "y",
    "CONFIG_PROVE_LOCKING": "y",
    "CONFIG_MODULES": "y",
    "CONFIG_SERIAL_8250_CONSOLE": "y",
    "CONFIG_DEVTMPFS_MOUNT": "y",
}

HAPPY_CONSOLE = """\
[    0.000000] Linux version 6.12.105 (gcc 13.3.0) #1 SMP preempt
GUEST:BOOT
[    2.345678] metaflux_core: loading out-of-tree module taints kernel.
[    2.456789] metaflux_core: MetaFlux core transport registered
GUEST:MODULE_LOADED
[    2.567890] TAP version 14
[    2.567891] 1..1
[    2.567892]     # Subtest: mf_cdev_generation
[    2.567893]     1..5
[    2.567894]     ok 1 - mf_cdev_generation_test_stale_rejected
[    2.567895]     ok 2 - mf_cdev_generation_test_matching_accepted
[    2.567896]     ok 3 - mf_cdev_generation_test_exclusive_lease
[    2.567897]     ok 4 - mf_cdev_generation_test_exclusive_eventfd
[    2.567898]     ok 5 - mf_cdev_generation_test_tombstone
[    2.567899]     # mf_cdev_generation: pass:5 fail:0 skip:0 total:5
[    2.567900] ok 1 - mf_cdev_generation
GUEST:KUNIT_LOADED
GUEST:QUALIFICATION_RC=0
GUEST:KMEMLEAK_BEGIN
GUEST:KMEMLEAK_END
[   12.000000] reboot: Power down
"""


def test_missing_bzimage_skips_77() -> None:
    with tempfile.TemporaryDirectory() as temp:
        code, stdout, stderr = run_runner(Path(temp))
        assert code == 77, f"expected 77, got {code}: {stderr}"
        assert "SKIP" in stderr, f"expected SKIP message, got: {stderr}"
        assert "build-debug-kernel.sh" in stderr, stderr
    print("PASS: missing_bzimage_skips_77")


def test_read_kernel_config_parses_fixture() -> None:
    runner = load_runner()
    with tempfile.TemporaryDirectory() as temp:
        config = Path(temp) / ".config"
        config.write_text(
            "\n".join(
                [
                    "# automatically generated config",
                    "CONFIG_KUNIT=y",
                    "CONFIG_KASAN=y",
                    "CONFIG_KCSAN=m",
                    "# CONFIG_DEBUG_KMEMLEAK is not set",
                    'CONFIG_PROVE_LOCKING="y"',
                    "CONFIG_MODULES=y",
                    "CONFIG_UNRELATED=42",
                    "",
                ]
            ),
            encoding="utf-8",
        )
        values = runner.read_kernel_config(config)
        assert values["CONFIG_KUNIT"] == "y"
        assert values["CONFIG_KASAN"] == "y"
        assert values["CONFIG_KCSAN"] == "m"
        assert values["CONFIG_DEBUG_KMEMLEAK"] == "n", values
        assert values["CONFIG_PROVE_LOCKING"] == "y", values
    with tempfile.TemporaryDirectory() as temp:
        values = runner.read_kernel_config(Path(temp) / "missing.config")
        assert values == {key: "absent" for key in runner.TRACKED_CONFIGS}
    print("PASS: read_kernel_config_parses_fixture")


def test_kernel_phase_fail_missing_modules() -> None:
    with tempfile.TemporaryDirectory() as temp:
        config = dict(FULL_CONFIG)
        del config["CONFIG_MODULES"]
        make_fake_cache(Path(temp), config)
        code, stdout, stderr = run_runner(Path(temp), BUILD_ONLY_FLAGS)
        assert code == 1, f"expected 1, got {code}: {stderr}"
        assert "CONFIG_MODULES" in stderr, stderr
        summary = json.loads(stdout)
        assert summary["result"] == "fail"
    print("PASS: kernel_phase_fail_missing_modules")


def test_kernel_phase_fail_no_sanitizer() -> None:
    with tempfile.TemporaryDirectory() as temp:
        config = dict(FULL_CONFIG)
        config["CONFIG_KASAN"] = "n"
        config["CONFIG_KCSAN"] = "n"
        make_fake_cache(Path(temp), config)
        code, stdout, stderr = run_runner(Path(temp), BUILD_ONLY_FLAGS)
        assert code == 1, f"expected 1, got {code}: {stderr}"
        assert "KASAN" in stderr, stderr
    print("PASS: kernel_phase_fail_no_sanitizer")


def test_kernel_phase_pass_json_shape() -> None:
    with tempfile.TemporaryDirectory() as temp:
        make_fake_cache(Path(temp), FULL_CONFIG)
        code, stdout, stderr = run_runner(Path(temp), BUILD_ONLY_FLAGS)
        assert code == 0, f"expected 0, got {code}: {stderr}"
        summary = json.loads(stdout)
        assert summary["result"] == "pass"
        assert summary["kernel_config"] == {
            key: "y" for key in (
                "CONFIG_KUNIT",
                "CONFIG_KASAN",
                "CONFIG_KCSAN",
                "CONFIG_DEBUG_KMEMLEAK",
                "CONFIG_PROVE_LOCKING",
            )
        }
        assert summary["phases"]["kernel"]["status"] == "pass"
        assert summary["phases"]["module"]["status"] == "skipped"
        assert summary["phases"]["qualification_binary"]["status"] == "skipped"
        assert summary["phases"]["guest_run"]["status"] == "skipped"
        assert summary["kmemleak_empty"] is None
        assert summary["kunit"] is None
    print("PASS: kernel_phase_pass_json_shape")


def test_parse_guest_output_pass() -> None:
    runner = load_runner()
    evidence = runner.parse_guest_output(HAPPY_CONSOLE)
    assert evidence["boot"] is True
    assert evidence["module_loaded"] is True
    assert evidence["kunit_loaded"] is True
    assert evidence["qualification_rc"] == 0
    assert evidence["warnings"] == []
    assert evidence["kmemleak"]["scanned"] is True
    assert evidence["kmemleak"]["empty"] is True
    assert evidence["kunit"] is not None
    assert evidence["kunit"]["ok"] is True
    assert evidence["kunit"]["suite_found"] is True
    assert evidence["kunit"]["passed"] == 5
    assert evidence["kunit"]["failed"] == 0

    without_rc = HAPPY_CONSOLE.replace("GUEST:QUALIFICATION_RC=0\n", "")
    assert runner.parse_guest_output(without_rc)["qualification_rc"] is None

    failing_rc = HAPPY_CONSOLE.replace("GUEST:QUALIFICATION_RC=0", "GUEST:QUALIFICATION_RC=19")
    assert runner.parse_guest_output(failing_rc)["qualification_rc"] == 19
    print("PASS: parse_guest_output_pass")


def test_parse_guest_output_warning_window() -> None:
    runner = load_runner()
    # KASAN report right after the module-load marker is caught.
    with_bug = HAPPY_CONSOLE.replace(
        "GUEST:KUNIT_LOADED",
        "BUG: KASAN: slab-out-of-bounds in metaflux_queue_push\nGUEST:KUNIT_LOADED",
    )
    evidence = runner.parse_guest_output(with_bug)
    assert any("BUG: KASAN" in line for line in evidence["warnings"]), evidence["warnings"]

    # Lockdep-style WARNING and rcu stall are caught too.
    with_lockdep = HAPPY_CONSOLE.replace(
        "GUEST:KUNIT_LOADED",
        "WARNING: CPU: 0 PID: 22 at kernel/locking/lockdep.c:1234\n"
        "INFO: rcu_preempt detected stalls\n"
        "GUEST:KUNIT_LOADED",
    )
    evidence = runner.parse_guest_output(with_lockdep)
    assert any("lockdep.c" in line for line in evidence["warnings"])
    assert any("INFO: rcu" in line for line in evidence["warnings"])

    # Boot-time noise well before the module load is outside the window.
    boot_noise = HAPPY_CONSOLE.replace(
        "Linux version",
        "BUG: KASAN: bogus early report\n" + ("[    0.1] early boot line\n" * 260),
    )
    evidence = runner.parse_guest_output(boot_noise)
    assert evidence["warnings"] == [], evidence["warnings"]
    print("PASS: parse_guest_output_warning_window")


def test_parse_kunit_tap_variants() -> None:
    runner = load_runner()
    ok = runner.parse_kunit_tap(HAPPY_CONSOLE)
    assert ok is not None and ok["ok"] is True and ok["passed"] == 5

    failing = (
        HAPPY_CONSOLE.replace(
            "    ok 5 - mf_cdev_generation_test_tombstone",
            "    not ok 5 - mf_cdev_generation_test_tombstone",
        ).replace(
            "ok 1 - mf_cdev_generation\n",
            "not ok 1 - mf_cdev_generation\n",
            1,
        )
    )
    parsed = runner.parse_kunit_tap(failing)
    assert parsed is not None and parsed["failed"] == 1 and parsed["ok"] is False
    assert parsed["suite_ok"] is False

    # Prefix-free TAP (raw printk disabled) still parses.
    plain = "TAP version 14\n1..1\nok 1 - mf_cdev_generation\n"
    parsed = runner.parse_kunit_tap(plain)
    assert parsed is not None and parsed["suite_ok"] is True

    assert runner.parse_kunit_tap("no tap output here at all\n") is None
    print("PASS: parse_kunit_tap_variants")


def test_extract_kmemleak_blocks() -> None:
    runner = load_runner()
    assert runner.extract_kmemleak(HAPPY_CONSOLE)["empty"] is True
    leaky = HAPPY_CONSOLE.replace(
        "GUEST:KMEMLEAK_END",
        "unreferenced object 0xffff88800bad0000 (size 128):\n"
        "  comm \"init\", pid 1, jiffies 4294\n"
        "GUEST:KMEMLEAK_END",
    )
    block = runner.extract_kmemleak(leaky)
    assert block["scanned"] is True
    assert block["empty"] is False
    assert len(block["report_lines"]) == 2
    comment_only = HAPPY_CONSOLE.replace(
        "GUEST:KMEMLEAK_END", "# kmemleak: no leaks\nGUEST:KMEMLEAK_END"
    )
    assert runner.extract_kmemleak(comment_only)["empty"] is True
    missing = runner.extract_kmemleak("no markers here")
    assert missing == {"scanned": False, "empty": False, "report_lines": []}
    print("PASS: extract_kmemleak_blocks")


def test_guest_init_script_and_cpio() -> None:
    runner = load_runner()
    init_text = runner.guest_init_script()
    for marker in (
        runner.MARKER_BOOT,
        runner.MARKER_MODULE_LOADED,
        runner.MARKER_KUNIT_LOADED,
        runner.MARKER_QUALIFICATION_RC,
        runner.MARKER_KMEMLEAK_BEGIN,
        runner.MARKER_KMEMLEAK_END,
    ):
        assert marker in init_text, marker
    assert "/ko/metaflux_core.ko" in init_text
    assert "/ko/mf_cdev_generation_kunit.ko" in init_text
    assert "poweroff" in init_text

    entries = [("init", 0o100755, 5, 0, b"#!x\n\n\n\n\n")]
    packed = runner.pack_cpio_newc(entries)
    assert packed.startswith(b"070701")
    assert b"TRAILER!!!\0" in packed[-64:]
    assert len(packed) % 4 == 0
    print("PASS: guest_init_script_and_cpio")


def test_module_vermagic_helpers() -> None:
    runner = load_runner()
    with tempfile.TemporaryDirectory() as temp:
        module = Path(temp) / "metaflux_core.ko"
        module.write_bytes(
            b"\x7fELFfake"
            + b"\0" * 32
            + b"vermagic=6.12.105 SMP preempt mod_unload \0srcversion=ABC\0"
        )
        assert runner.module_vermagic(module).startswith("6.12.105")
        assert runner.module_release_matches(module, "6.12.105") is True
        assert runner.module_release_matches(module, "6.18.0") is False
        assert runner.module_release_matches(module, "") is None
        stale = Path(temp) / "stale.ko"
        stale.write_bytes(b"\x7fELFfake" + b"\0" * 32 + b"vermagic=6.18.0-custom SMP \0")
        assert runner.module_release_matches(stale, "6.12.105") is False
        empty = Path(temp) / "empty.ko"
        empty.write_bytes(b"not an elf with modinfo")
        assert runner.module_release_matches(empty, "6.12.105") is None
    print("PASS: module_vermagic_helpers")


def main() -> int:
    tests = [
        test_missing_bzimage_skips_77,
        test_read_kernel_config_parses_fixture,
        test_kernel_phase_fail_missing_modules,
        test_kernel_phase_fail_no_sanitizer,
        test_kernel_phase_pass_json_shape,
        test_parse_guest_output_pass,
        test_parse_guest_output_warning_window,
        test_parse_kunit_tap_variants,
        test_extract_kmemleak_blocks,
        test_guest_init_script_and_cpio,
        test_module_vermagic_helpers,
    ]
    failed = 0
    for test in tests:
        try:
            test()
        except Exception as exc:  # noqa: BLE001 - report and continue
            print(f"FAIL: {test.__name__}: {exc}", file=sys.stderr)
            failed += 1
    if failed:
        print(f"{failed} self-test(s) failed", file=sys.stderr)
        return 1
    print("All self-tests passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
