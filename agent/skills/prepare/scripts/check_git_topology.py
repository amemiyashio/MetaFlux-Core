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

sys.path.insert(0, str(Path(__file__).resolve().parents[4] / "tools"))

from agent_diagnostics import (  # noqa: E402
    DiagnosticArgumentParser,
    DiagnosticError,
    add_diagnostic_format_argument,
    emit_diagnostics,
    task_stop_error,
)


class TopologyError(DiagnosticError):
    """Raised when the current Git topology is not a valid execution surface."""


@dataclass(frozen=True)
class GitTopology:
    repository_root: str
    git_common_dir: str
    git_dir: str
    context_kind: str


PRESERVE_CONTEXT_ACTION = (
    "Preserve the current checkout and its Git evidence. The user or application "
    "must supply or repair an existing registered primary checkout or linked "
    "worktree; the Agent must not create a replacement context."
)
PRESERVE_CONTEXT_RESUME = (
    "$main skill Bootstrap passes in the user- or application-supplied registered Git context."
)


def topology_error(
    *,
    code: str,
    summary: str,
    evidence: tuple[object, ...],
    required_action: str = PRESERVE_CONTEXT_ACTION,
    resume_when: str = PRESERVE_CONTEXT_RESUME,
) -> TopologyError:
    diagnostic = task_stop_error(
        code=code,
        source="iteration / Git topology",
        summary=summary,
        evidence=evidence,
        responsibility="user-or-application",
        disposition="preserve-and-report",
        required_action=required_action,
        resume_when=resume_when,
    ).diagnostic
    return TopologyError(diagnostic)


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
        raise topology_error(
            code="git-topology.local-environment-unavailable",
            summary="Git local environment variables cannot be enumerated.",
            evidence=(
                f"return code: {result.returncode}",
                f"git stderr: {result.stderr.strip() or '<empty>'}",
            ),
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
        raise topology_error(
            code="git-topology.git-query-failed",
            summary="A read-only Git topology query failed.",
            evidence=(
                f"repository: {repository}",
                f"query: git {' '.join(arguments)}",
                f"return code: {result.returncode}",
                f"git output: {detail or '<empty>'}",
            ),
        )
    return result


def resolved_git_path(repository: Path, *arguments: str) -> Path:
    value = git(repository, *arguments).stdout.strip()
    if not value:
        raise topology_error(
            code="git-topology.empty-path",
            summary="A Git topology query returned an empty path.",
            evidence=(f"repository: {repository}", f"query: git {' '.join(arguments)}"),
        )
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
        raise topology_error(
            code="git-topology.unregistered-root",
            summary="The repository root is not registered by Git worktree metadata.",
            evidence=(f"repository root: {root}", f"Git common directory: {common_dir}"),
        )

    if git_dir == common_dir:
        context_kind = "primary"
    else:
        try:
            git_dir.relative_to(common_dir / "worktrees")
        except ValueError as error:
            raise topology_error(
                code="git-topology.invalid-linked-worktree",
                summary=(
                    "The linked worktree Git directory is outside the registered "
                    "common directory."
                ),
                evidence=(
                    f"Git directory: {git_dir}",
                    f"expected parent: {common_dir / 'worktrees'}",
                ),
            ) from error
        context_kind = "linked-worktree"

    clone_evidence = local_clone_evidence(repository)
    if clone_evidence:
        raise topology_error(
            code="git-topology.local-clone",
            summary="The current checkout is a standalone local clone.",
            evidence=tuple(clone_evidence),
        )

    return GitTopology(
        repository_root=str(root),
        git_common_dir=str(common_dir),
        git_dir=str(git_dir),
        context_kind=context_kind,
    )


def parser() -> argparse.ArgumentParser:
    result = DiagnosticArgumentParser(
        description="Validate MetaFlux execution using existing Git topology.",
        diagnostic_source="iteration / Git topology",
    )
    result.add_argument("repository", nargs="?", default=".")
    result.add_argument("--json", action="store_true")
    add_diagnostic_format_argument(result)
    return result


def main() -> int:
    arguments = parser().parse_args()
    try:
        topology = resolve_git_topology(Path(arguments.repository))
    except TopologyError as error:
        emit_diagnostics(
            (error.diagnostic,), diagnostic_format=arguments.diagnostic_format
        )
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
