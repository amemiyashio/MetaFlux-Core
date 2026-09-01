#!/usr/bin/env python3
"""Invoke the bounded MetaFlux host-privilege helpers."""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path
from typing import Sequence


SUDO = "/usr/bin/sudo"
PACMAN_HELPER = "/usr/local/libexec/metaflux-pacman-install"
DRIVER_HELPER = "/usr/local/libexec/metaflux-driver-debug"
REPOSITORY_ROOT = Path(__file__).resolve().parents[4]
PACKAGE_PATTERN = re.compile(r"^[a-z0-9][a-z0-9@._+-]{0,127}$")
DRIVER_ACTION_ARITY = {
    "check": 0,
    "load": 1,
    "reload": 1,
    "unload": 0,
    "logs": 0,
    "kmemleak-clear": 0,
    "kmemleak-scan": 0,
    "kmemleak-read": 0,
    "live": 1,
}


def validate_package_name(package: str) -> str:
    if PACKAGE_PATTERN.fullmatch(package) is None:
        raise ValueError(f"invalid pacman package name: {package!r}")
    return package


def _project_artifact(
    value: str,
    *,
    root: Path,
    expected_name: str,
    executable: bool,
) -> Path:
    candidate = Path(value)
    if not candidate.is_absolute():
        raise ValueError("driver artifact path must be absolute")
    try:
        resolved = candidate.resolve(strict=True)
        repository_root = root.resolve(strict=True)
        resolved.relative_to(repository_root)
    except (FileNotFoundError, RuntimeError, ValueError) as error:
        raise ValueError("driver artifact must resolve inside the repository") from error
    if resolved.name != expected_name or not resolved.is_file():
        raise ValueError(f"driver artifact must be the file {expected_name}")
    if executable and resolved.stat().st_mode & 0o111 == 0:
        raise ValueError(f"driver artifact {expected_name} is not executable")
    return resolved


def package_command(packages: Sequence[str]) -> tuple[str, ...]:
    if not packages:
        raise ValueError("at least one pacman package is required")
    validated = tuple(validate_package_name(package) for package in packages)
    return (SUDO, "--non-interactive", PACMAN_HELPER, *validated)


def driver_command(
    action: str,
    arguments: Sequence[str],
    *,
    root: Path = REPOSITORY_ROOT,
) -> tuple[str, ...]:
    expected = DRIVER_ACTION_ARITY.get(action)
    if expected is None:
        raise ValueError(f"unsupported driver action: {action!r}")
    if len(arguments) != expected:
        raise ValueError(f"driver action {action!r} requires {expected} argument(s)")

    validated: tuple[str, ...] = ()
    if action in {"load", "reload"}:
        module = _project_artifact(
            arguments[0],
            root=root,
            expected_name="metaflux_core.ko",
            executable=False,
        )
        validated = (str(module),)
    elif action == "live":
        executable = _project_artifact(
            arguments[0],
            root=root,
            expected_name="metaflux_transport_cdev_live_qualification",
            executable=True,
        )
        validated = (str(executable),)

    return (SUDO, "--non-interactive", DRIVER_HELPER, action, *validated)


def check_commands() -> tuple[tuple[str, ...], ...]:
    return (
        (SUDO, "--non-interactive", PACMAN_HELPER, "--check"),
        driver_command("check", ()),
    )


def parse_args(arguments: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Use the D0032 bounded host-privilege helpers."
    )
    subparsers = parser.add_subparsers(dest="command", required=True)
    subparsers.add_parser("check", help="verify both persistent helper grants")

    package = subparsers.add_parser("package", help="install exact pacman packages")
    package.add_argument("packages", nargs="+")

    driver = subparsers.add_parser("driver", help="run one bounded driver action")
    driver.add_argument("action", choices=tuple(DRIVER_ACTION_ARITY))
    driver.add_argument("arguments", nargs="*")
    return parser.parse_args(arguments)


def main(arguments: Sequence[str] | None = None) -> int:
    parsed = parse_args(sys.argv[1:] if arguments is None else arguments)
    try:
        if parsed.command == "check":
            commands = check_commands()
        elif parsed.command == "package":
            commands = (package_command(parsed.packages),)
        else:
            commands = (driver_command(parsed.action, parsed.arguments),)
    except ValueError as error:
        print(f"host privilege: {error}", file=sys.stderr)
        return 2

    for command in commands:
        result = subprocess.run(command, check=False)
        if result.returncode != 0:
            return result.returncode
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
