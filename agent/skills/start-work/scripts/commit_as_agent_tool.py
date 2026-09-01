#!/usr/bin/env python3
"""Create one Git commit with detected agent-tool identity and declared Epoch."""

from __future__ import annotations

import argparse
import importlib.util
import os
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


TOOL_EXECUTABLE_DECLARATION = "METAFLUX_AGENT_TOOL_EXECUTABLE"
EPOCH_DECLARATION = "METAFLUX_AGENT_EPOCH"
EPOCH_RE = re.compile(r"epoch-[0-9]{4}")
SCRIPT = Path(__file__).resolve()
DETECTOR_SCRIPT = (
    SCRIPT.parents[2]
    / "detect-agent-tool"
    / "scripts"
    / "detect_agent_tool.py"
)


def load_detector():
    spec = importlib.util.spec_from_file_location(
        "metaflux_agent_tool_for_commit", DETECTOR_SCRIPT
    )
    if spec is None or spec.loader is None:
        raise RuntimeError("agent-tool detector cannot be loaded")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


DETECTOR = load_detector()


@dataclass(frozen=True)
class AgentIdentity:
    subject: str
    epoch: str
    executable: str

    @property
    def name(self) -> str:
        return self.subject

    @property
    def email(self) -> str:
        return f"{self.subject}@localhost"


def declared_identity(
    environment: dict[str, str], explicit_executable: str | None = None
) -> AgentIdentity:
    epoch = environment.get(EPOCH_DECLARATION)
    if epoch is None or EPOCH_RE.fullmatch(epoch) is None:
        raise ValueError(f"{EPOCH_DECLARATION} must match epoch-NNNN")
    try:
        tool = DETECTOR.detect_agent_tool(
            explicit=explicit_executable,
            environment=environment,
        )
    except DETECTOR.DetectionError as error:
        raise ValueError(str(error)) from error
    return AgentIdentity(
        subject=tool.subject,
        epoch=epoch,
        executable=tool.executable,
    )


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
            raise ValueError(f"{long_name} conflicts with detected agent identity")
        if argument.startswith("-") and not argument.startswith("--"):
            if "C" in argument[1:] or "c" in argument[1:]:
                raise ValueError("message reuse conflicts with detected agent identity")
    return arguments


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(
        description="Run git commit under detected agent-tool identity and Epoch."
    )
    result.add_argument(
        "--agent-tool", help="exact harness or CLI executable for identity detection"
    )
    result.add_argument("--print-identity", action="store_true")
    result.add_argument("git_arguments", nargs=argparse.REMAINDER)
    return result


def main() -> int:
    arguments = parser().parse_args()
    try:
        identity = declared_identity(
            dict(os.environ), explicit_executable=arguments.agent_tool
        )
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
            TOOL_EXECUTABLE_DECLARATION: identity.executable,
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
