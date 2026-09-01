#!/usr/bin/env python3
"""Create one Git commit with the active agent harness identity."""

from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
from dataclasses import dataclass


@dataclass(frozen=True)
class HarnessIdentity:
    subject: str
    name: str
    email: str


HARNESS_DECLARATION = "METAFLUX_AGENT_HARNESS"
HARNESS_SUBJECT = re.compile(r"[a-z0-9]+(?:-[a-z0-9]+)*")
MAX_SUBJECT_LENGTH = 24
NON_HARNESS_SEGMENTS = {
    "backend",
    "build",
    "cli",
    "gpt",
    "model",
    "prompt",
    "session",
    "template",
    "thread",
}


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
    if NON_HARNESS_SEGMENTS.intersection(value.split("-")):
        raise ValueError(
            "harness subject must identify the stable harness product, not a "
            "model, template, backend, build, CLI, session, thread, or prompt"
        )
    return value


def identity_for_subject(subject: str) -> HarnessIdentity:
    validated = validate_subject(subject)
    return HarnessIdentity(
        subject=validated,
        name=f"Agent Harness ({validated})",
        email=f"{validated}@localhost",
    )


def declared_harness(environment: dict[str, str]) -> str:
    declared_value = environment.get(HARNESS_DECLARATION)
    if not declared_value:
        raise ValueError(
            "agent harness declaration is missing; the agent must read the "
            "stable harness product slug from active runtime instruction "
            f"context and supply {HARNESS_DECLARATION}"
        )
    return validate_subject(declared_value)


def resolve_identity(
    environment: dict[str, str],
) -> HarnessIdentity:
    return identity_for_subject(declared_harness(environment))


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
                "--harness was removed; the agent must declare the stable "
                "harness product slug from active runtime instruction context "
                f"through {HARNESS_DECLARATION}"
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
