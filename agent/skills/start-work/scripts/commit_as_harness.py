#!/usr/bin/env python3
"""Create one Git commit with the active agent harness identity."""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
from dataclasses import dataclass


@dataclass(frozen=True)
class HarnessIdentity:
    name: str
    email: str
    signals: tuple[str, ...]


HARNESSES = {
    "codex": HarnessIdentity(
        name="Codex",
        email="codex@localhost",
        signals=("CODEX_SESSION_ID", "CODEX_THREAD_ID"),
    ),
    "claude-code": HarnessIdentity(
        name="Claude Code",
        email="claude-code@localhost",
        signals=("CLAUDE_PROJECT_DIR",),
    ),
}


def detect_harness(explicit: str | None, environment: dict[str, str]) -> str:
    selected = explicit or environment.get("METAFLUX_AGENT_HARNESS")
    if selected:
        if selected not in HARNESSES:
            choices = ", ".join(sorted(HARNESSES))
            raise ValueError(
                f"unknown agent harness {selected!r}; choose one of: {choices}"
            )
        return selected

    detected = [
        slug
        for slug, identity in HARNESSES.items()
        if any(environment.get(signal) for signal in identity.signals)
    ]
    if len(detected) == 1:
        return detected[0]
    if not detected:
        raise ValueError(
            "agent harness is not detectable; pass --harness or set "
            "METAFLUX_AGENT_HARNESS"
        )
    raise ValueError(
        "multiple agent harnesses are detectable; pass --harness explicitly: "
        + ", ".join(sorted(detected))
    )


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
    result.add_argument(
        "--harness",
        choices=sorted(HARNESSES),
        help="explicit harness when environment detection is unavailable or ambiguous",
    )
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
        harness = detect_harness(arguments.harness, dict(os.environ))
        identity = HARNESSES[harness]
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
