#!/usr/bin/env python3
"""Isolated behavioral tests for commit_as_harness.py."""

from __future__ import annotations

import importlib.util
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path
from types import ModuleType
from typing import Callable, cast


SCRIPT = Path(__file__).with_name("commit_as_harness.py").resolve()
SKILL = SCRIPT.parents[1] / "SKILL.md"
GIT_IDENTITY_VARIABLES = {
    "GIT_AUTHOR_NAME",
    "GIT_AUTHOR_EMAIL",
    "GIT_COMMITTER_NAME",
    "GIT_COMMITTER_EMAIL",
}


def load_harness_module() -> ModuleType:
    spec = importlib.util.spec_from_file_location("metaflux_commit_harness", SCRIPT)
    if spec is None or spec.loader is None:
        raise AssertionError(f"cannot load {SCRIPT}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


HARNESS = load_harness_module()


def clean_environment() -> dict[str, str]:
    environment = os.environ.copy()
    for name in tuple(environment):
        if name == HARNESS.HARNESS_DECLARATION or name in GIT_IDENTITY_VARIABLES:
            environment.pop(name)
    return environment


def harness_environment(subject: str) -> dict[str, str]:
    environment = clean_environment()
    environment[HARNESS.HARNESS_DECLARATION] = subject
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


def expect_value_error(action: Callable[[], object], text: str) -> None:
    try:
        action()
    except ValueError as error:
        if text not in str(error):
            raise AssertionError(f"expected {text!r} in {error!r}") from error
        return
    raise AssertionError(f"expected ValueError containing {text!r}")


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
    return cast(tuple[str, str, str, str], fields)


def commit(
    repository: Path,
    environment: dict[str, str],
    *arguments: str,
) -> subprocess.CompletedProcess[str]:
    return run(repository, sys.executable, str(SCRIPT), *arguments, environment=environment)


def test_unseen_harness_declaration_is_derived(root: Path) -> None:
    del root
    environment = {HARNESS.HARNESS_DECLARATION: "future-agent"}
    subject = HARNESS.declared_harness(environment)
    assert subject == "future-agent"
    derived = HARNESS.identity_for_subject(subject)
    assert derived.name == "Agent Harness (future-agent)"
    assert derived.email == "future-agent@localhost"


def test_codex_subject_excludes_model_and_cli_labels(root: Path) -> None:
    del root
    identity = HARNESS.resolve_identity({HARNESS.HARNESS_DECLARATION: "codex"})
    assert identity.name == "Agent Harness (codex)"
    assert identity.email == "codex@localhost"

    non_harness_subjects = (
        "gpt-5",
        "codex-cli",
        "agent-template",
        "runtime-session",
        "github-gpt-5-6-sol-unrestricted-33b86c71",
    )
    for invalid in non_harness_subjects:
        expect_value_error(
            lambda invalid=invalid: HARNESS.declared_harness(
                {HARNESS.HARNESS_DECLARATION: invalid}
            ),
            "harness subject",
        )


def test_process_and_namespace_inference_is_absent(root: Path) -> None:
    del root
    assert not hasattr(HARNESS, "read_process_ancestry")
    source = SCRIPT.read_text(encoding="utf-8")
    forbidden_inference_tokens = (
        "/proc",
        "getppid",
        "HARNESS_SIGNAL",
        "SESSION_ID",
        "THREAD_ID",
        "PROJECT_DIR",
    )
    for token in forbidden_inference_tokens:
        assert token not in source
    expect_value_error(
        lambda: HARNESS.resolve_identity(
            {
                "CODEX_THREAD_ID": "outer-thread",
                "ZCODE_SESSION_ID": "inner-session",
                "CLAUDE_PROJECT_DIR": "/fixture/project",
            }
        ),
        "declaration is missing",
    )


def test_start_work_policy_is_nix_first(root: Path) -> None:
    del root
    source = SKILL.read_text(encoding="utf-8")
    required = (
        "## Stage Zero: Resolve Runtime And Enter Nix",
        "For Codex, the subject is\n   exactly `codex`",
        "Do not search for an\n   agent binary or CLI",
        "nix develop . --command ...",
        "add it to the\n   repository Nix declaration before use",
        "mandatory before staging",
        "Never probe ambient host\n   tools first",
    )
    for fragment in required:
        assert fragment in source
    assert source.index("## Stage Zero") < source.index("## Steps")
    assert re.search(r"(?m)^\s*python3\s+", source) is None


def test_declaration_validation(root: Path) -> None:
    del root
    direct = {HARNESS.HARNESS_DECLARATION: "future-agent"}
    resolved = HARNESS.resolve_identity(direct)
    assert resolved.subject == "future-agent"
    assert resolved.name == "Agent Harness (future-agent)"

    invalid_subjects = (
        "Codex",
        "../codex",
        "codex@example",
        "codex--nested",
        "x" * (HARNESS.MAX_SUBJECT_LENGTH + 1),
    )
    for invalid in invalid_subjects:
        expect_value_error(
            lambda invalid=invalid: HARNESS.declared_harness(
                {HARNESS.HARNESS_DECLARATION: invalid}
            ),
            "harness subject",
        )


def test_commit_overrides_without_config_mutation(root: Path) -> None:
    repository = root / "derived-identity"
    repository.mkdir()
    initialize(repository)
    stage(repository, "one.txt", "one\n")
    environment = harness_environment("fixture-agent")
    environment.update(
        {
            "GIT_AUTHOR_NAME": "Stale Agent",
            "GIT_AUTHOR_EMAIL": "stale-author@example.invalid",
            "GIT_COMMITTER_NAME": "Stale Harness",
            "GIT_COMMITTER_EMAIL": "stale-committer@example.invalid",
        }
    )
    result = commit(repository, environment, "--", "-m", "one")
    require(result, "derived harness commit")
    expected = (
        "Agent Harness (fixture-agent)",
        "fixture-agent@localhost",
        "Agent Harness (fixture-agent)",
        "fixture-agent@localhost",
    )
    assert identity(repository) == expected
    assert run(repository, "git", "config", "user.name").stdout.strip() == "Human User"
    assert (
        run(repository, "git", "config", "user.email").stdout.strip()
        == "human@example.invalid"
    )


def test_harness_handoff_is_not_sticky(root: Path) -> None:
    repository = root / "harness-handoff"
    repository.mkdir()
    initialize(repository)

    stage(repository, "alpha.txt", "alpha\n")
    first = commit(
        repository,
        harness_environment("alpha-agent"),
        "--",
        "-m",
        "Alpha stage",
    )
    require(first, "alpha harness commit")
    assert identity(repository) == (
        "Agent Harness (alpha-agent)",
        "alpha-agent@localhost",
        "Agent Harness (alpha-agent)",
        "alpha-agent@localhost",
    )

    stage(repository, "beta.txt", "beta\n")
    second = commit(
        repository,
        harness_environment("beta-agent"),
        "--",
        "-m",
        "Beta stage",
    )
    require(second, "beta harness commit")
    assert identity(repository) == (
        "Agent Harness (beta-agent)",
        "beta-agent@localhost",
        "Agent Harness (beta-agent)",
        "beta-agent@localhost",
    )
    assert run(repository, "git", "config", "user.name").stdout.strip() == "Human User"


def test_missing_legacy_and_conflicting_options_are_rejected(root: Path) -> None:
    missing = commit(root, clean_environment(), "--print-identity")
    assert missing.returncode == 2
    assert "declaration is missing" in missing.stderr

    legacy = commit(
        root,
        clean_environment(),
        "--harness",
        "codex",
        "--print-identity",
    )
    assert legacy.returncode == 2
    assert "--harness was removed" in legacy.stderr

    repository = root / "rejected-options"
    repository.mkdir()
    initialize(repository)
    stage(repository, "options.txt", "options\n")
    environment = harness_environment("fixture-agent")
    conflicting = (
        ("--author=Human User <human@example.invalid>", "-m", "options"),
        ("--auth=Human User <human@example.invalid>", "-m", "options"),
        ("--amend", "-m", "options"),
        ("--amen", "-m", "options"),
    )
    for arguments in conflicting:
        result = commit(repository, environment, "--", *arguments)
        assert result.returncode == 2
    assert run(repository, "git", "rev-list", "--all", "--count").stdout.strip() == "0"


def test_authorship_reuse_options_are_rejected(root: Path) -> None:
    repository = root / "rejected-authorship-reuse"
    repository.mkdir()
    initialize(repository)
    stage(repository, "source.txt", "source\n")
    require(
        run(
            repository,
            "git",
            "commit",
            "-m",
            "Human source",
            environment=clean_environment(),
        ),
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
            harness_environment("fixture-agent"),
            "--",
            *reuse_form,
        )
        assert result.returncode == 2
        assert "reuse another commit's authorship" in result.stderr
        assert run(repository, "git", "rev-list", "--all", "--count").stdout.strip() == "1"


def main() -> int:
    tests = (
        test_unseen_harness_declaration_is_derived,
        test_codex_subject_excludes_model_and_cli_labels,
        test_process_and_namespace_inference_is_absent,
        test_start_work_policy_is_nix_first,
        test_declaration_validation,
        test_commit_overrides_without_config_mutation,
        test_harness_handoff_is_not_sticky,
        test_missing_legacy_and_conflicting_options_are_rejected,
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
