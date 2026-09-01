#!/usr/bin/env python3
"""Create one Git commit with the fixed Codex identity and declared Epoch."""

from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
from dataclasses import dataclass


HARNESS_DECLARATION = "METAFLUX_AGENT_HARNESS"
EPOCH_DECLARATION = "METAFLUX_AGENT_EPOCH"
EXPECTED_HARNESS = "codex"
EPOCH_RE = re.compile(r"epoch-[0-9]{4}")


@dataclass(frozen=True)
class AgentIdentity:
    harness: str
    epoch: str
    name: str = "codex"
    email: str = "codex@localhost"


def declared_identity(environment: dict[str, str]) -> AgentIdentity:
    harness = environment.get(HARNESS_DECLARATION)
    if harness != EXPECTED_HARNESS:
        raise ValueError(
            f"{HARNESS_DECLARATION} must be exactly {EXPECTED_HARNESS!r}"
        )
    epoch = environment.get(EPOCH_DECLARATION)
    if epoch is None or EPOCH_RE.fullmatch(epoch) is None:
        raise ValueError(f"{EPOCH_DECLARATION} must match epoch-NNNN")
    return AgentIdentity(harness=harness, epoch=epoch)


def commit_arguments(raw_arguments: list[str]) -> list[str]:
    arguments = list(raw_arguments)
    if arguments and arguments[0] == "--":
        arguments.pop(0)
    if not arguments:
        raise ValueError("git commit arguments are required after --")

    protected = (
        "--author",
        "--amend",
        "--reuse-message",
        "--reedit-message",
    )
    for argument in arguments:
        long_name = argument.partition("=")[0]
        if long_name.startswith("--") and any(
            option.startswith(long_name) for option in protected
        ):
            raise ValueError(f"{long_name} conflicts with fixed agent identity")
        if argument.startswith("-") and not argument.startswith("--"):
            if "C" in argument[1:] or "c" in argument[1:]:
                raise ValueError("message reuse conflicts with fixed agent identity")
    return arguments


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(
        description="Run git commit as codex under the declared MetaFlux Epoch."
    )
    result.add_argument("--print-identity", action="store_true")
    result.add_argument("git_arguments", nargs=argparse.REMAINDER)
    return result


def main() -> int:
    arguments = parser().parse_args()
    try:
        identity = declared_identity(dict(os.environ))
        if arguments.print_identity:
            if arguments.git_arguments:
                raise ValueError("--print-identity does not accept commit arguments")
            print(f"{identity.name} <{identity.email}> @ {identity.epoch}")
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
        f"agent identity: {identity.name} <{identity.email}> @ {identity.epoch}",
        file=sys.stderr,
    )
    return subprocess.run(
        ["git", "commit", *git_arguments], check=False, env=environment
    ).returncode


if __name__ == "__main__":
    raise SystemExit(main())
