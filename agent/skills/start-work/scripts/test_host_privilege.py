#!/usr/bin/env python3
"""Behavioral and static tests for the D0032 host-privilege boundary."""

from __future__ import annotations

import importlib.util
import sys
import tempfile
from pathlib import Path
from types import ModuleType
from typing import Callable


SCRIPT = Path(__file__).with_name("host_privilege.py").resolve()
PACMAN_ROOT = SCRIPT.with_name("metaflux-pacman-install")
DRIVER_ROOT = SCRIPT.with_name("metaflux-driver-debug")
SUDOERS = SCRIPT.with_name("metaflux-host-privilege.sudoers.in")


def load_module() -> ModuleType:
    spec = importlib.util.spec_from_file_location("metaflux_host_privilege", SCRIPT)
    if spec is None or spec.loader is None:
        raise AssertionError(f"cannot load {SCRIPT}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


HOST = load_module()


def expect_value_error(action: Callable[[], object], text: str) -> None:
    try:
        action()
    except ValueError as error:
        if text not in str(error):
            raise AssertionError(f"expected {text!r} in {error!r}") from error
        return
    raise AssertionError(f"expected ValueError containing {text!r}")


def test_package_command_is_bounded(root: Path) -> None:
    del root
    command = HOST.package_command(("linux-headers", "cmake"))
    assert command == (
        "/usr/bin/sudo",
        "--non-interactive",
        "/usr/local/libexec/metaflux-pacman-install",
        "linux-headers",
        "cmake",
    )
    for invalid in ("-S", "pkg name", "https://example.invalid/pkg", "../pkg", ""):
        expect_value_error(lambda invalid=invalid: HOST.package_command((invalid,)), "invalid")


def test_check_uses_noninteractive_exact_helpers(root: Path) -> None:
    del root
    assert HOST.check_commands() == (
        (
            "/usr/bin/sudo",
            "--non-interactive",
            "/usr/local/libexec/metaflux-pacman-install",
            "--check",
        ),
        (
            "/usr/bin/sudo",
            "--non-interactive",
            "/usr/local/libexec/metaflux-driver-debug",
            "check",
        ),
    )


def test_driver_module_path_is_project_scoped(root: Path) -> None:
    module = root / "build" / "metaflux_core.ko"
    module.parent.mkdir()
    module.write_bytes(b"fixture")
    command = HOST.driver_command("load", (str(module),), root=root)
    assert command[-2:] == ("load", str(module.resolve()))

    outside = root.parent / "metaflux_core.ko"
    outside.write_bytes(b"outside")
    expect_value_error(
        lambda: HOST.driver_command("load", (str(outside),), root=root),
        "inside the repository",
    )


def test_live_path_requires_exact_executable(root: Path) -> None:
    live = root / "build" / "metaflux_transport_cdev_live_qualification"
    live.parent.mkdir(exist_ok=True)
    live.write_text("fixture\n", encoding="utf-8")
    expect_value_error(
        lambda: HOST.driver_command("live", (str(live),), root=root),
        "not executable",
    )
    live.chmod(0o755)
    command = HOST.driver_command("live", (str(live),), root=root)
    assert command[-2:] == ("live", str(live.resolve()))


def test_driver_actions_reject_extra_or_unknown_arguments(root: Path) -> None:
    expect_value_error(
        lambda: HOST.driver_command("logs", ("extra",), root=root),
        "requires 0",
    )
    expect_value_error(
        lambda: HOST.driver_command("shell", (), root=root),
        "unsupported",
    )


def test_root_helpers_have_closed_command_sets(root: Path) -> None:
    del root
    pacman = PACMAN_ROOT.read_text(encoding="utf-8")
    driver = DRIVER_ROOT.read_text(encoding="utf-8")
    assert "exec \"$PACMAN\" --sync --needed --noconfirm -- \"$@\"" in pacman
    assert "*[!a-z0-9@._+-]*" in pacman
    for forbidden in ("eval ", "sh -c", "bash -c", "sudo "):
        assert forbidden not in pacman
        assert forbidden not in driver
    for action in (
        "load|reload)",
        "unload)",
        "logs)",
        "kmemleak-clear|kmemleak-scan)",
        "kmemleak-read)",
        "live)",
    ):
        assert action in driver
    assert "0:600|0:644" in driver
    assert "configuration ownership or mode is invalid" in driver


def test_no_credential_transport_or_generic_sudo_rule(root: Path) -> None:
    del root
    combined = "\n".join(
        path.read_text(encoding="utf-8")
        for path in (SCRIPT, PACMAN_ROOT, DRIVER_ROOT, SUDOERS)
    )
    for forbidden in ("getpass", "SUDO_ASKPASS", "sudo -S", "ALL=(ALL) ALL"):
        assert forbidden not in combined
    sudoers = SUDOERS.read_text(encoding="utf-8")
    assert sudoers.count("NOPASSWD:") == 2
    assert "/usr/local/libexec/metaflux-pacman-install *" in sudoers
    assert "/usr/local/libexec/metaflux-driver-debug *" in sudoers


def main() -> int:
    tests = (
        test_package_command_is_bounded,
        test_check_uses_noninteractive_exact_helpers,
        test_driver_module_path_is_project_scoped,
        test_live_path_requires_exact_executable,
        test_driver_actions_reject_extra_or_unknown_arguments,
        test_root_helpers_have_closed_command_sets,
        test_no_credential_transport_or_generic_sudo_rule,
    )
    with tempfile.TemporaryDirectory(prefix="metaflux-host-privilege-") as temporary:
        root = Path(temporary)
        for test in tests:
            test(root)
    print(f"host-privilege self-test: {len(tests)}/{len(tests)} passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
