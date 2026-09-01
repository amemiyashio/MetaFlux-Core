#!/usr/bin/env python3
"""Validate that MetaFlux work runs from a provisioned Git common directory."""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
from dataclasses import asdict, dataclass
from pathlib import Path


EXECUTION_COMMON_DIR_KEY = "metaflux.agentExecutionCommonDir"


class ContextError(ValueError):
    """Raised when the current checkout is not a provisioned execution context."""


@dataclass(frozen=True)
class ExecutionContext:
    repository_root: str
    git_common_dir: str
    git_dir: str
    context_kind: str


def isolated_git_environment(
    source: dict[str, str] | None = None,
) -> dict[str, str]:
    environment = dict(os.environ if source is None else source)
    result = subprocess.run(
        ["git", "rev-parse", "--local-env-vars"],
        check=False,
        capture_output=True,
        text=True,
        env=environment,
    )
    if result.returncode != 0:
        raise ContextError(
            f"cannot enumerate Git local environment: {result.stderr.strip()}"
        )
    for variable in result.stdout.splitlines():
        environment.pop(variable, None)
    return environment


def git(
    repository: Path,
    *arguments: str,
    environment: dict[str, str] | None = None,
    allowed_returncodes: tuple[int, ...] = (0,),
) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(
        ["git", "-C", str(repository), *arguments],
        check=False,
        capture_output=True,
        text=True,
        env=isolated_git_environment(environment),
    )
    if result.returncode not in allowed_returncodes:
        detail = result.stderr.strip() or result.stdout.strip()
        raise ContextError(
            f"git {' '.join(arguments)} failed in {repository}: {detail}"
        )
    return result


def resolved_git_path(repository: Path, *arguments: str) -> Path:
    value = git(repository, *arguments).stdout.strip()
    if not value:
        raise ContextError(f"git {' '.join(arguments)} returned an empty path")
    return Path(value).resolve()


def registered_worktrees(repository: Path) -> set[Path]:
    result = git(repository, "worktree", "list", "--porcelain", "-z")
    return {
        Path(field.removeprefix("worktree ")).resolve()
        for field in result.stdout.split("\0")
        if field.startswith("worktree ")
    }


def resolve_execution_context(
    repository: Path,
    environment: dict[str, str] | None = None,
) -> ExecutionContext:
    repository = repository.resolve()
    root = resolved_git_path(repository, "rev-parse", "--show-toplevel")
    common_dir = resolved_git_path(
        repository,
        "rev-parse",
        "--path-format=absolute",
        "--git-common-dir",
    )
    git_dir = resolved_git_path(repository, "rev-parse", "--absolute-git-dir")

    registration = git(
        repository,
        "config",
        "--local",
        "--get",
        EXECUTION_COMMON_DIR_KEY,
        environment=environment,
        allowed_returncodes=(0, 1),
    )
    if registration.returncode == 1 or not registration.stdout.strip():
        raise ContextError(
            "execution context is not provisioned: shared Git config key "
            f"{EXECUTION_COMMON_DIR_KEY} is missing"
        )
    registered_common_dir = Path(registration.stdout.strip())
    if not registered_common_dir.is_absolute():
        raise ContextError(
            f"{EXECUTION_COMMON_DIR_KEY} must contain an absolute path"
        )
    if registered_common_dir.resolve() != common_dir:
        raise ContextError(
            "execution context registration does not match its Git common "
            f"directory: registered {registered_common_dir.resolve()}, actual {common_dir}"
        )

    worktrees = registered_worktrees(repository)
    if root not in worktrees:
        raise ContextError(
            f"repository root is not registered by Git worktree metadata: {root}"
        )

    if git_dir == common_dir:
        context_kind = "primary"
    else:
        try:
            git_dir.relative_to(common_dir / "worktrees")
        except ValueError as error:
            raise ContextError(
                f"linked worktree Git directory is outside {common_dir / 'worktrees'}"
            ) from error
        context_kind = "linked-worktree"

    return ExecutionContext(
        repository_root=str(root),
        git_common_dir=str(common_dir),
        git_dir=str(git_dir),
        context_kind=context_kind,
    )


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(
        description="Validate a provisioned MetaFlux Git execution context."
    )
    result.add_argument("repository", nargs="?", default=".")
    result.add_argument("--json", action="store_true")
    return result


def main() -> int:
    arguments = parser().parse_args()
    try:
        context = resolve_execution_context(Path(arguments.repository))
    except ContextError as error:
        print(f"error: {error}", file=sys.stderr)
        return 2

    if arguments.json:
        print(json.dumps(asdict(context), sort_keys=True))
    else:
        print(
            f"execution context: {context.context_kind} "
            f"{context.repository_root} @ {context.git_common_dir}"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
