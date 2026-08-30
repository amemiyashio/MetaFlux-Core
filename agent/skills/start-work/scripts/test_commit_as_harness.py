#!/usr/bin/env python3
"""Isolated behavioral tests for commit_as_harness.py."""

from __future__ import annotations

import os
import subprocess
import sys
import tempfile
from pathlib import Path


SCRIPT = Path(__file__).with_name("commit_as_harness.py").resolve()
HARNESS_SIGNALS = (
    "CODEX_SESSION_ID",
    "CODEX_THREAD_ID",
    "CLAUDE_PROJECT_DIR",
    "METAFLUX_AGENT_HARNESS",
    "GIT_AUTHOR_NAME",
    "GIT_AUTHOR_EMAIL",
    "GIT_COMMITTER_NAME",
    "GIT_COMMITTER_EMAIL",
)


def clean_environment() -> dict[str, str]:
    environment = os.environ.copy()
    for name in HARNESS_SIGNALS:
        environment.pop(name, None)
    return environment


def run(
    repository: Path,
    *arguments: str,
    environment: dict[str, str] | None = None,
) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        arguments,
        cwd=repository,
        env=environment,
        check=False,
        capture_output=True,
        text=True,
    )


def require(result: subprocess.CompletedProcess[str], context: str) -> None:
    if result.returncode != 0:
        raise AssertionError(
            f"{context} failed with {result.returncode}:\n{result.stdout}\n{result.stderr}"
        )


def initialize(repository: Path) -> None:
    require(run(repository, "git", "init", "-q"), "git init")
    require(run(repository, "git", "config", "user.name", "Human User"), "set user.name")
    require(
        run(repository, "git", "config", "user.email", "human@example.invalid"),
        "set user.email",
    )


def stage(repository: Path, filename: str, content: str) -> None:
    (repository / filename).write_text(content, encoding="utf-8")
    require(run(repository, "git", "add", "--", filename), f"stage {filename}")


def identity(repository: Path) -> tuple[str, str, str, str]:
    result = run(repository, "git", "show", "-s", "--format=%an%x00%ae%x00%cn%x00%ce")
    require(result, "read commit identity")
    fields = tuple(result.stdout.rstrip("\n").split("\0"))
    if len(fields) != 4:
        raise AssertionError(f"unexpected identity fields: {fields!r}")
    return fields  # type: ignore[return-value]


def commit(
    repository: Path,
    environment: dict[str, str],
    *arguments: str,
) -> subprocess.CompletedProcess[str]:
    return run(repository, sys.executable, str(SCRIPT), *arguments, environment=environment)


def test_explicit_codex_overrides_without_config_mutation(root: Path) -> None:
    repository = root / "explicit-codex"
    repository.mkdir()
    initialize(repository)
    stage(repository, "one.txt", "one\n")
    environment = clean_environment()
    environment.update(
        {
            "GIT_AUTHOR_NAME": "Stale Agent",
            "GIT_AUTHOR_EMAIL": "stale-author@example.invalid",
            "GIT_COMMITTER_NAME": "Stale Harness",
            "GIT_COMMITTER_EMAIL": "stale-committer@example.invalid",
        }
    )
    result = commit(
        repository,
        environment,
        "--harness",
        "codex",
        "--",
        "-m",
        "one",
    )
    require(result, "explicit Codex commit")
    assert identity(repository) == (
        "Codex",
        "codex@localhost",
        "Codex",
        "codex@localhost",
    )
    assert run(repository, "git", "config", "user.name").stdout.strip() == "Human User"
    assert (
        run(repository, "git", "config", "user.email").stdout.strip()
        == "human@example.invalid"
    )


def test_codex_environment_detection(root: Path) -> None:
    repository = root / "detect-codex"
    repository.mkdir()
    initialize(repository)
    stage(repository, "two.txt", "two\n")
    environment = clean_environment()
    environment["CODEX_SESSION_ID"] = "fixture-session"
    result = commit(repository, environment, "--", "-m", "two")
    require(result, "detected Codex commit")
    assert identity(repository) == (
        "Codex",
        "codex@localhost",
        "Codex",
        "codex@localhost",
    )


def test_claude_code_environment_detection(root: Path) -> None:
    repository = root / "detect-claude"
    repository.mkdir()
    initialize(repository)
    stage(repository, "three.txt", "three\n")
    environment = clean_environment()
    environment["CLAUDE_PROJECT_DIR"] = str(repository)
    result = commit(
        repository,
        environment,
        "--",
        "-m",
        "three",
    )
    require(result, "detected Claude Code commit")
    assert identity(repository) == (
        "Claude Code",
        "claude-code@localhost",
        "Claude Code",
        "claude-code@localhost",
    )


def test_harness_handoff_is_not_sticky(root: Path) -> None:
    repository = root / "harness-handoff"
    repository.mkdir()
    initialize(repository)

    stage(repository, "codex.txt", "codex\n")
    first = commit(
        repository,
        clean_environment(),
        "--harness",
        "codex",
        "--",
        "-m",
        "Codex stage",
    )
    require(first, "Codex handoff commit")
    assert identity(repository) == (
        "Codex",
        "codex@localhost",
        "Codex",
        "codex@localhost",
    )

    stage(repository, "claude.txt", "claude\n")
    second = commit(
        repository,
        clean_environment(),
        "--harness",
        "claude-code",
        "--",
        "-m",
        "Claude Code stage",
    )
    require(second, "Claude Code handoff commit")
    assert identity(repository) == (
        "Claude Code",
        "claude-code@localhost",
        "Claude Code",
        "claude-code@localhost",
    )
    assert run(repository, "git", "config", "user.name").stdout.strip() == "Human User"
    assert (
        run(repository, "git", "config", "user.email").stdout.strip()
        == "human@example.invalid"
    )


def test_missing_and_ambiguous_detection(root: Path) -> None:
    missing = commit(root, clean_environment(), "--print-identity")
    assert missing.returncode == 2
    assert "not detectable" in missing.stderr

    environment = clean_environment()
    environment["CODEX_SESSION_ID"] = "fixture-session"
    environment["CLAUDE_PROJECT_DIR"] = str(root)
    ambiguous = commit(root, environment, "--print-identity")
    assert ambiguous.returncode == 2
    assert "multiple agent harnesses" in ambiguous.stderr


def test_conflicting_commit_options_are_rejected(root: Path) -> None:
    repository = root / "rejected-options"
    repository.mkdir()
    initialize(repository)
    stage(repository, "four.txt", "four\n")
    environment = clean_environment()

    author = commit(
        repository,
        environment,
        "--harness",
        "codex",
        "--",
        "--author=Human User <human@example.invalid>",
        "-m",
        "four",
    )
    assert author.returncode == 2
    assert "--author conflicts" in author.stderr

    abbreviated_author = commit(
        repository,
        environment,
        "--harness",
        "codex",
        "--",
        "--auth=Human User <human@example.invalid>",
        "-m",
        "four",
    )
    assert abbreviated_author.returncode == 2
    assert "--author conflicts" in abbreviated_author.stderr

    amend = commit(
        repository,
        environment,
        "--harness",
        "codex",
        "--",
        "--amend",
        "-m",
        "four",
    )
    assert amend.returncode == 2
    assert "--amend is excluded" in amend.stderr

    abbreviated_amend = commit(
        repository,
        environment,
        "--harness",
        "codex",
        "--",
        "--amen",
        "-m",
        "four",
    )
    assert abbreviated_amend.returncode == 2
    assert "--amend is excluded" in abbreviated_amend.stderr

    assert (
        run(repository, "git", "rev-list", "--all", "--count").stdout.strip()
        == "0"
    )


def test_authorship_reuse_options_are_rejected(root: Path) -> None:
    repository = root / "rejected-authorship-reuse"
    repository.mkdir()
    initialize(repository)
    stage(repository, "source.txt", "source\n")
    require(
        run(repository, "git", "commit", "-m", "Human source"),
        "human source commit",
    )
    assert identity(repository) == (
        "Human User",
        "human@example.invalid",
        "Human User",
        "human@example.invalid",
    )

    stage(repository, "pending.txt", "pending\n")
    reuse_forms = (
        ("-C", "HEAD"),
        ("-CHEAD",),
        ("-qCHEAD",),
        ("-c", "HEAD"),
        ("-cHEAD",),
        ("-qcHEAD",),
        ("--reuse-message", "HEAD"),
        ("--reuse-message=HEAD",),
        ("--reu=HEAD",),
        ("--reedit-message", "HEAD"),
        ("--reedit-message=HEAD",),
        ("--ree=HEAD",),
    )
    for reuse_form in reuse_forms:
        result = commit(
            repository,
            clean_environment(),
            "--harness",
            "codex",
            "--",
            *reuse_form,
        )
        assert result.returncode == 2
        assert "reuse another commit's authorship" in result.stderr
        assert (
            run(repository, "git", "rev-list", "--all", "--count").stdout.strip()
            == "1"
        )


def main() -> int:
    tests = (
        test_explicit_codex_overrides_without_config_mutation,
        test_codex_environment_detection,
        test_claude_code_environment_detection,
        test_harness_handoff_is_not_sticky,
        test_missing_and_ambiguous_detection,
        test_conflicting_commit_options_are_rejected,
        test_authorship_reuse_options_are_rejected,
    )
    with tempfile.TemporaryDirectory(prefix="metaflux-harness-identity-") as temporary:
        root = Path(temporary)
        for test in tests:
            test(root)
    print(f"commit-as-harness self-test: {len(tests)}/{len(tests)} passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
