#!/usr/bin/env python3
"""Behavioral tests for the MetaFlux detected-agent commit helper."""

from __future__ import annotations

import importlib.util
import os
import subprocess
import sys
import tempfile
from pathlib import Path


SCRIPT = Path(__file__).with_name("commit_as_agent_tool.py").resolve()
SKILL = SCRIPT.parents[1] / "SKILL.md"


def load_module():
    spec = importlib.util.spec_from_file_location("metaflux_commit_helper", SCRIPT)
    if spec is None or spec.loader is None:
        raise AssertionError("commit helper cannot be loaded")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


HELPER = load_module()
TOPOLOGY_CHECKER = HELPER.TOPOLOGY_CHECKER


def isolated_git_environment(
    source: dict[str, str] | None = None,
) -> dict[str, str]:
    environment = dict(os.environ if source is None else source)
    result = subprocess.run(
        ["git", "rev-parse", "--local-env-vars"],
        check=False,
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        raise AssertionError(
            f"cannot enumerate Git local environment: {result.stderr}"
        )
    for variable in result.stdout.splitlines():
        environment.pop(variable, None)
    return environment


def make_tool(root: Path, name: str = "fixture-agent") -> Path:
    root.mkdir(parents=True, exist_ok=True)
    path = root / name
    path.write_text(
        "#!/bin/sh\n"
        "if [ \"$1\" = \"--version\" ]; then\n"
        "  printf '%s\\n' 'fixture-agent-cli 1.2.3'\n"
        "  exit 0\n"
        "fi\n"
        "if [ \"$1\" = \"--help\" ]; then\n"
        "  exit 0\n"
        "fi\n"
        "exit 2\n",
        encoding="utf-8",
    )
    path.chmod(0o755)
    return path


def environment(tool: Path) -> dict[str, str]:
    result = isolated_git_environment()
    result[HELPER.TOOL_EXECUTABLE_DECLARATION] = str(tool)
    return result


def run(root: Path, *arguments: str, env=None):
    return subprocess.run(
        arguments,
        cwd=root,
        env=isolated_git_environment(env),
        check=False,
        capture_output=True,
        text=True,
    )


def require(result, context: str) -> None:
    if result.returncode != 0:
        raise AssertionError(f"{context}:\n{result.stdout}\n{result.stderr}")


def initialize_repository(repository: Path) -> None:
    repository.mkdir(parents=True)
    require(run(repository, "git", "init", "-q"), "git init")
    require(run(repository, "git", "config", "user.name", "Human"), "git name")
    require(
        run(repository, "git", "config", "user.email", "human@example.invalid"),
        "git email",
    )


def expect_error(action, fragment: str) -> None:
    try:
        action()
    except ValueError as error:
        if fragment not in str(error):
            raise AssertionError(f"expected {fragment!r}, got {error!r}") from error
        return
    raise AssertionError(f"expected error containing {fragment!r}")


def test_declarations(root: Path) -> None:
    tool = make_tool(root)
    identity = HELPER.declared_identity(environment(tool))
    assert identity.name == "fixture-agent"
    assert identity.email == "fixture-agent@localhost"
    assert identity.executable == str(tool.resolve())
    expect_error(
        lambda: HELPER.declared_identity(
            {"PATH": ""},
            explicit_executable=str(root / "missing-agent-tool"),
        ),
        "not executable",
    )


def test_identity_output(root: Path) -> None:
    tool = make_tool(root / "identity")
    repository = root / "repository"
    initialize_repository(repository)
    result = run(
        repository,
        sys.executable,
        str(SCRIPT),
        "--print-identity",
        env=environment(tool),
    )
    require(result, "identity preflight")
    assert result.stdout.strip() == "fixture-agent <fixture-agent@localhost>"


def test_commit_identity(root: Path) -> None:
    tool = make_tool(root / "tools")
    repository = root / "repository"
    initialize_repository(repository)
    (repository / "value.txt").write_text("value\n", encoding="utf-8")
    require(run(repository, "git", "add", "value.txt"), "git add")
    result = run(
        repository,
        sys.executable,
        str(SCRIPT),
        "--agent-tool",
        str(tool),
        "--",
        "-m",
        "candidate",
        env=isolated_git_environment(),
    )
    require(result, "agent commit")
    identity = run(repository, "git", "show", "-s", "--format=%an|%ae|%cn|%ce")
    require(identity, "read identity")
    assert identity.stdout.strip() == (
        "fixture-agent|fixture-agent@localhost|"
        "fixture-agent|fixture-agent@localhost"
    )
    assert run(repository, "git", "config", "user.name").stdout.strip() == "Human"


def test_git_topology_boundary(root: Path) -> None:
    source = root / "source"
    initialize_repository(source)
    (source / "value.txt").write_text("value\n", encoding="utf-8")
    require(run(source, "git", "add", "value.txt"), "git add")
    require(run(source, "git", "commit", "-q", "-m", "base"), "base commit")

    primary = TOPOLOGY_CHECKER.resolve_git_topology(source)
    assert primary.context_kind == "primary"
    assert primary.repository_root == str(source.resolve())

    linked = root / "linked"
    require(
        run(source, "git", "worktree", "add", "-q", "--detach", str(linked), "HEAD"),
        "linked worktree",
    )
    linked_context = TOPOLOGY_CHECKER.resolve_git_topology(linked)
    assert linked_context.context_kind == "linked-worktree"
    assert linked_context.git_common_dir == primary.git_common_dir

    clone = root / "clone"
    require(run(root, "git", "clone", "-q", str(source), str(clone)), "local clone")
    expect_error(
        lambda: TOPOLOGY_CHECKER.resolve_git_topology(clone),
        "local standalone clone",
    )
    require(run(clone, "git", "remote", "remove", "origin"), "remove clone remote")
    expect_error(
        lambda: TOPOLOGY_CHECKER.resolve_git_topology(clone),
        "local standalone clone",
    )

    require(
        run(source, "git", "remote", "add", "upstream", "https://example.invalid/repo.git"),
        "add network remote",
    )
    TOPOLOGY_CHECKER.resolve_git_topology(source)


def test_conflicting_options() -> None:
    for arguments in (["--", "--amend"], ["--", "--author=x"], ["--", "-C", "HEAD"]):
        expect_error(
            lambda arguments=arguments: HELPER.commit_arguments(arguments), "identity"
        )


def test_policy_text() -> None:
    text = SKILL.read_text(encoding="utf-8")
    assert text.index("## Stage Zero") < text.index("## Load Current Authority")
    assert "$detect-agent-tool" in text
    assert "nix develop . --command ..." in text
    assert "goal.json" in text
    assert "check_git_topology.py" in text
    duplicate_epoch = "METAFLUX_AGENT_" + "EPOCH"
    assert duplicate_epoch not in text
    legacy_marker = "METAFLUX_AGENT_" + "HARNESS"
    assert legacy_marker not in text


def main() -> int:
    test_conflicting_options()
    test_policy_text()
    with tempfile.TemporaryDirectory(prefix="metaflux-commit-helper-") as temp:
        root = Path(temp)
        test_declarations(root / "declarations")
        test_identity_output(root / "output")
        test_commit_identity(root / "commit")
        test_git_topology_boundary(root / "context")
    print("commit helper tests: 6 passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
