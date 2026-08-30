#!/usr/bin/env python3
"""Create one Git commit with the active agent harness identity."""

from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Callable


@dataclass(frozen=True)
class HarnessIdentity:
    subject: str
    name: str
    email: str


HARNESS_DECLARATION = "METAFLUX_AGENT_HARNESS"
HARNESS_SIGNAL = re.compile(
    r"^(?P<namespace>[A-Z][A-Z0-9_]*)_"
    r"(?:SESSION_ID|THREAD_ID|PROJECT_DIR)$"
)
HARNESS_SUBJECT = re.compile(r"[a-z0-9]+(?:-[a-z0-9]+)*")
MAX_SUBJECT_LENGTH = 48


def validate_subject(value: str) -> str:
    if not value or len(value) > MAX_SUBJECT_LENGTH:
        raise ValueError(
            f"harness subject must contain 1-{MAX_SUBJECT_LENGTH} characters"
        )
    if not HARNESS_SUBJECT.fullmatch(value):
        raise ValueError(
            "harness subject must use lowercase ASCII letters, digits, and "
            "single hyphen separators"
        )
    return value


def identity_for_subject(subject: str) -> HarnessIdentity:
    validated = validate_subject(subject)
    return HarnessIdentity(
        subject=validated,
        name=f"Agent Harness ({validated})",
        email=f"{validated}@localhost",
    )


def environment_subjects(environment: dict[str, str]) -> tuple[str, ...]:
    subjects: set[str] = set()
    for name, value in environment.items():
        if not value:
            continue
        match = HARNESS_SIGNAL.fullmatch(name)
        if not match:
            continue
        candidate = match.group("namespace").lower().replace("_", "-")
        try:
            subjects.add(validate_subject(candidate))
        except ValueError:
            continue
    return tuple(sorted(subjects))


def command_mentions_subject(command: str, subject: str) -> bool:
    boundary = re.compile(
        rf"(?<![a-z0-9]){re.escape(subject)}(?![a-z0-9])",
        re.IGNORECASE,
    )
    return boundary.search(command) is not None


def detect_harness(
    environment: dict[str, str],
    ancestry: tuple[str, ...],
) -> str:
    declared_value = environment.get(HARNESS_DECLARATION)
    if declared_value:
        return validate_subject(declared_value)

    subjects = environment_subjects(environment)

    detected: str | None = None
    for command in ancestry:
        matches = [
            subject
            for subject in subjects
            if command_mentions_subject(command, subject)
        ]
        if len(matches) > 1:
            raise ValueError(
                "multiple harness subjects match the same ancestor process: "
                + ", ".join(matches)
            )
        if matches:
            detected = matches[0]
            break

    if detected:
        return detected
    if not subjects:
        raise ValueError(
            "agent harness is not detectable; the harness must expose runtime "
            "session, thread, or project signals"
        )
    raise ValueError(
        "harness environment signals are not corroborated by process ancestry: "
        + ", ".join(subjects)
    )


def read_process_ancestry(start_pid: int | None = None) -> tuple[str, ...]:
    process_id = os.getppid() if start_pid is None else start_pid
    commands: list[str] = []
    visited: set[int] = set()

    while process_id > 1 and process_id not in visited and len(visited) < 64:
        visited.add(process_id)
        process_root = Path("/proc") / str(process_id)
        try:
            command_bytes = (process_root / "cmdline").read_bytes()
            status = (process_root / "status").read_text(encoding="utf-8")
        except FileNotFoundError:
            break
        except OSError as error:
            raise ValueError(
                f"cannot read harness process ancestry at pid {process_id}: {error}"
            ) from error

        command = command_bytes.replace(b"\0", b" ").decode(
            "utf-8", errors="replace"
        ).strip()
        if not command:
            try:
                command = (process_root / "comm").read_text(
                    encoding="utf-8"
                ).strip()
            except OSError:
                command = ""
        if command:
            commands.append(command)

        parent_lines = [
            line for line in status.splitlines() if line.startswith("PPid:")
        ]
        if len(parent_lines) != 1:
            raise ValueError(
                f"cannot resolve parent process for harness ancestry pid {process_id}"
            )
        process_id = int(parent_lines[0].split()[1])

    return tuple(commands)


def resolve_identity(
    environment: dict[str, str],
    ancestry_reader: Callable[[], tuple[str, ...]] = read_process_ancestry,
) -> HarnessIdentity:
    ancestry = () if environment.get(HARNESS_DECLARATION) else ancestry_reader()
    return identity_for_subject(detect_harness(environment, ancestry))


def commit_arguments(raw_arguments: list[str]) -> list[str]:
    arguments = list(raw_arguments)
    if arguments and arguments[0] == "--":
        arguments.pop(0)
    if not arguments:
        raise ValueError("git commit arguments are required after --")

    def matches_long_option(argument: str, protected: str) -> bool:
        candidate = argument.partition("=")[0]
        return (
            candidate.startswith("--")
            and len(candidate) > 2
            and protected.startswith(candidate)
        )

    if any(matches_long_option(argument, "--author") for argument in arguments):
        raise ValueError("--author conflicts with the required harness identity")
    if any(matches_long_option(argument, "--amend") for argument in arguments):
        raise ValueError(
            "--amend is excluded because it can preserve or rewrite another "
            "author's identity"
        )
    short_reuse = any(
        argument.startswith("-")
        and not argument.startswith("--")
        and ("C" in argument[1:] or "c" in argument[1:])
        for argument in arguments
    )
    long_reuse = any(
        matches_long_option(argument, option)
        for argument in arguments
        for option in ("--reuse-message", "--reedit-message")
    )
    if short_reuse or long_reuse:
        raise ValueError(
            "commit options that reuse another commit's authorship are excluded"
        )
    return arguments


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(
        description="Run git commit with the active agent harness as Author and Committer."
    )
    result.add_argument("--harness", dest="legacy_harness", help=argparse.SUPPRESS)
    result.add_argument(
        "--print-identity",
        action="store_true",
        help="print the resolved identity without creating a commit",
    )
    result.add_argument("git_arguments", nargs=argparse.REMAINDER)
    return result


def main() -> int:
    arguments = parser().parse_args()
    try:
        if arguments.legacy_harness is not None:
            raise ValueError(
                "--harness was removed; identity is read automatically from "
                "the runtime harness subject"
            )
        identity = resolve_identity(dict(os.environ))
        if arguments.print_identity:
            if arguments.git_arguments:
                raise ValueError("--print-identity does not accept git commit arguments")
            print(f"{identity.name} <{identity.email}>")
            return 0
        git_arguments = commit_arguments(arguments.git_arguments)
    except ValueError as error:
        print(f"error: {error}", file=sys.stderr)
        return 2

    environment = os.environ.copy()
    environment.update(
        {
            "GIT_AUTHOR_NAME": identity.name,
            "GIT_AUTHOR_EMAIL": identity.email,
            "GIT_COMMITTER_NAME": identity.name,
            "GIT_COMMITTER_EMAIL": identity.email,
        }
    )
    print(
        f"agent harness identity: {identity.name} <{identity.email}>",
        file=sys.stderr,
    )
    return subprocess.run(
        ["git", "commit", *git_arguments],
        check=False,
        env=environment,
    ).returncode


if __name__ == "__main__":
    raise SystemExit(main())
