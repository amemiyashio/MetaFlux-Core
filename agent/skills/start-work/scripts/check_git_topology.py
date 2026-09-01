#!/usr/bin/env python3
"""Reject local standalone clones without inventing repository identity."""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
from dataclasses import asdict, dataclass
from pathlib import Path
from urllib.parse import unquote, urlparse


class TopologyError(ValueError):
    """Raised when the current Git topology is not a valid execution surface."""


@dataclass(frozen=True)
class GitTopology:
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
        raise TopologyError(
            f"cannot enumerate Git local environment: {result.stderr.strip()}"
        )
    for variable in result.stdout.splitlines():
        environment.pop(variable, None)
    return environment


def git(
    repository: Path,
    *arguments: str,
    allowed_returncodes: tuple[int, ...] = (0,),
) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(
        ["git", "-C", str(repository), *arguments],
        check=False,
        capture_output=True,
        text=True,
        env=isolated_git_environment(),
    )
    if result.returncode not in allowed_returncodes:
        detail = result.stderr.strip() or result.stdout.strip()
        raise TopologyError(
            f"git {' '.join(arguments)} failed in {repository}: {detail}"
        )
    return result


def resolved_git_path(repository: Path, *arguments: str) -> Path:
    value = git(repository, *arguments).stdout.strip()
    if not value:
        raise TopologyError(f"git {' '.join(arguments)} returned an empty path")
    return Path(value).resolve()


def registered_worktrees(repository: Path) -> set[Path]:
    result = git(repository, "worktree", "list", "--porcelain", "-z")
    return {
        Path(field.removeprefix("worktree ")).resolve()
        for field in result.stdout.split("\0")
        if field.startswith("worktree ")
    }


def local_git_source(value: str) -> str | None:
    value = value.strip()
    if not value:
        return None
    parsed = urlparse(value)
    if parsed.scheme:
        return unquote(parsed.path) if parsed.scheme == "file" else None
    if re.match(r"^[^/]+:[^/].*", value):
        return None
    return value


def local_clone_evidence(repository: Path) -> list[str]:
    evidence: list[str] = []
    remotes = git(repository, "remote").stdout.splitlines()
    for remote in remotes:
        urls = git(repository, "remote", "get-url", "--all", remote)
        for url in urls.stdout.splitlines():
            if local_git_source(url) is not None:
                evidence.append(f"remote {remote} -> {url}")

    head = git(
        repository,
        "rev-parse",
        "--verify",
        "HEAD",
        allowed_returncodes=(0, 128),
    )
    if head.returncode == 0:
        reflog = git(repository, "reflog", "show", "--format=%gs", "HEAD")
        for message in reflog.stdout.splitlines():
            prefix = "clone: from "
            if message.startswith(prefix):
                source = message.removeprefix(prefix)
                if local_git_source(source) is not None:
                    evidence.append(f"HEAD reflog -> {source}")
    return list(dict.fromkeys(evidence))


def resolve_git_topology(repository: Path) -> GitTopology:
    repository = repository.resolve()
    root = resolved_git_path(repository, "rev-parse", "--show-toplevel")
    common_dir = resolved_git_path(
        repository,
        "rev-parse",
        "--path-format=absolute",
        "--git-common-dir",
    )
    git_dir = resolved_git_path(repository, "rev-parse", "--absolute-git-dir")

    if root not in registered_worktrees(repository):
        raise TopologyError(
            f"repository root is not registered by Git worktree metadata: {root}"
        )

    if git_dir == common_dir:
        context_kind = "primary"
    else:
        try:
            git_dir.relative_to(common_dir / "worktrees")
        except ValueError as error:
            raise TopologyError(
                f"linked worktree Git directory is outside {common_dir / 'worktrees'}"
            ) from error
        context_kind = "linked-worktree"

    clone_evidence = local_clone_evidence(repository)
    if clone_evidence:
        raise TopologyError(
            "local standalone clone execution is forbidden: "
            + "; ".join(clone_evidence)
        )

    return GitTopology(
        repository_root=str(root),
        git_common_dir=str(common_dir),
        git_dir=str(git_dir),
        context_kind=context_kind,
    )


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(
        description="Validate MetaFlux execution using existing Git topology."
    )
    result.add_argument("repository", nargs="?", default=".")
    result.add_argument("--json", action="store_true")
    return result


def main() -> int:
    arguments = parser().parse_args()
    try:
        topology = resolve_git_topology(Path(arguments.repository))
    except TopologyError as error:
        print(f"error: {error}", file=sys.stderr)
        return 2

    if arguments.json:
        print(json.dumps(asdict(topology), sort_keys=True))
    else:
        print(
            f"git topology: {topology.context_kind} "
            f"{topology.repository_root} @ {topology.git_common_dir}"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
