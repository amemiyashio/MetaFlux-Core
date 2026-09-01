#!/usr/bin/env python3
"""Invoke helpers owned by the MetaFlux host-privilege skill."""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path
from typing import Sequence

sys.path.insert(0, str(Path(__file__).resolve().parents[4] / "tools"))

from agent_diagnostics import (  # noqa: E402
    DiagnosticArgumentParser,
    DiagnosticError,
    add_diagnostic_format_argument,
    emit_diagnostics,
    task_stop_error,
)


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


def privilege_error(
    *,
    code: str,
    summary: str,
    evidence: Sequence[object],
    required_action: str,
    resume_when: str,
    responsibility: str = "current-agent",
    disposition: str = "fix-and-retry",
) -> DiagnosticError:
    return task_stop_error(
        code=code,
        source="manage-host-privilege",
        summary=summary,
        evidence=evidence,
        responsibility=responsibility,
        disposition=disposition,
        required_action=required_action,
        resume_when=resume_when,
    )


def validate_package_name(package: str) -> str:
    if PACKAGE_PATTERN.fullmatch(package) is None:
        raise privilege_error(
            code="host-privilege.invalid-package-name",
            summary="The pacman package name is outside the bounded interface.",
            evidence=(f"package argument: {package!r}",),
            required_action=(
                "Supply exact repository package names only after manage-toolchain "
                "has proved a Nix provision or materialization gap."
            ),
            resume_when="Every package argument matches the package-name allowlist.",
        )
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
        raise privilege_error(
            code="host-privilege.artifact-path-relative",
            summary="The driver artifact path is not absolute.",
            evidence=(f"artifact argument: {value}",),
            required_action=(
                "Pass the canonical artifact's absolute path from the configured "
                "repository root."
            ),
            resume_when="The artifact argument is an absolute path.",
        )
    try:
        resolved = candidate.resolve(strict=True)
        repository_root = root.resolve(strict=True)
        resolved.relative_to(repository_root)
    except (FileNotFoundError, RuntimeError, ValueError) as error:
        raise privilege_error(
            code="host-privilege.artifact-outside-repository",
            summary="The driver artifact does not resolve inside the configured repository.",
            evidence=(f"artifact argument: {value}", f"configured root: {root}"),
            responsibility="user-or-application",
            disposition="preserve-and-report",
            required_action=(
                "Preserve the candidate and supply the canonical integrated artifact "
                "from the configured repository root; do not copy it into a sibling "
                "source tree or reconfigure the privilege boundary from this task."
            ),
            resume_when=(
                "The supplied canonical artifact resolves under the configured "
                "repository root."
            ),
        ) from error
    if resolved.name != expected_name or not resolved.is_file():
        raise privilege_error(
            code="host-privilege.artifact-name-invalid",
            summary="The driver artifact does not have the canonical file name.",
            evidence=(f"resolved artifact: {resolved}", f"required name: {expected_name}"),
            required_action="Build and supply the canonical artifact named by the owning Skill.",
            resume_when="The resolved artifact is the required canonical file.",
        )
    if executable and resolved.stat().st_mode & 0o111 == 0:
        raise privilege_error(
            code="host-privilege.artifact-not-executable",
            summary="The live qualification artifact is not executable.",
            evidence=(f"resolved artifact: {resolved}",),
            required_action="Build or restore the canonical executable qualification artifact.",
            resume_when="The canonical qualification artifact has an executable mode.",
        )
    return resolved


def package_command(packages: Sequence[str]) -> tuple[str, ...]:
    if not packages:
        raise privilege_error(
            code="host-privilege.package-required",
            summary="At least one exact pacman package is required.",
            evidence=("package list is empty",),
            required_action=(
                "Supply exact repository package names backed by confirmed Nix-gap evidence."
            ),
            resume_when="The bounded package command contains at least one package.",
        )
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
        raise privilege_error(
            code="host-privilege.driver-action-unsupported",
            summary="The requested driver action is outside the bounded allowlist.",
            evidence=(f"action: {action!r}",),
            required_action="Choose one action declared by manage-host-privilege.",
            resume_when="The driver action is present in the exact allowlist.",
        )
    if len(arguments) != expected:
        raise privilege_error(
            code="host-privilege.driver-arity-invalid",
            summary="The driver action received the wrong number of arguments.",
            evidence=(
                f"action: {action}",
                f"required arguments: {expected}",
                f"supplied arguments: {len(arguments)}",
            ),
            required_action="Invoke the exact action signature documented by the Skill.",
            resume_when="The driver action receives its exact argument count.",
        )

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
    parser = DiagnosticArgumentParser(
        description="Use the decision-0032 bounded host-privilege helpers.",
        diagnostic_source="manage-host-privilege",
    )
    add_diagnostic_format_argument(parser)
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
    except DiagnosticError as error:
        emit_diagnostics(
            (error.diagnostic,), diagnostic_format=parsed.diagnostic_format
        )
        return 2

    for command in commands:
        result = subprocess.run(command, check=False)
        if result.returncode != 0:
            error = privilege_error(
                code="host-privilege.helper-rejected",
                summary="A bounded root-owned helper rejected the operation.",
                evidence=(
                    f"helper: {command[2]}",
                    f"operation: {parsed.command}",
                    f"return code: {result.returncode}",
                ),
                responsibility="host-operator",
                disposition="stop-and-report",
                required_action=(
                    "Preserve the preceding helper output and run the canonical "
                    "authorization check. The host operator must repair the exact "
                    "helper, grant, or kernel prerequisite; do not widen the command set."
                ),
                resume_when=(
                    "The canonical helper check passes and the same bounded operation "
                    "succeeds."
                ),
            )
            emit_diagnostics(
                (error.diagnostic,), diagnostic_format=parsed.diagnostic_format
            )
            return result.returncode
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
