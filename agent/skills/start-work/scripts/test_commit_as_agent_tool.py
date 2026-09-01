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


def environment(tool: Path, epoch: str = "epoch-0002") -> dict[str, str]:
    result = os.environ.copy()
    result[HELPER.TOOL_EXECUTABLE_DECLARATION] = str(tool)
    result[HELPER.EPOCH_DECLARATION] = epoch
    return result


def run(root: Path, *arguments: str, env=None):
    return subprocess.run(
        arguments,
        cwd=root,
        env=env,
        check=False,
        capture_output=True,
        text=True,
    )


def require(result, context: str) -> None:
    if result.returncode != 0:
        raise AssertionError(f"{context}:\n{result.stdout}\n{result.stderr}")


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
    assert identity.epoch == "epoch-0002"
    assert identity.executable == str(tool.resolve())
    expect_error(
        lambda: HELPER.declared_identity(
            {HELPER.EPOCH_DECLARATION: "epoch-0002", "PATH": ""},
            explicit_executable=str(root / "missing-agent-tool"),
        ),
        "not executable",
    )
    expect_error(
        lambda: HELPER.declared_identity(
            environment(tool, epoch="E" + "0002")
        ),
        "epoch-NNNN",
    )


def test_identity_output(root: Path) -> None:
    tool = make_tool(root / "identity")
    result = run(
        root,
        sys.executable,
        str(SCRIPT),
        "--print-identity",
        env=environment(tool),
    )
    require(result, "identity preflight")
    assert result.stdout.strip() == (
        "fixture-agent <fixture-agent@localhost> @ epoch-0002"
    )


def test_commit_identity(root: Path) -> None:
    tool = make_tool(root / "tools")
    repository = root / "repository"
    repository.mkdir()
    require(run(repository, "git", "init", "-q"), "git init")
    require(run(repository, "git", "config", "user.name", "Human"), "git name")
    require(
        run(repository, "git", "config", "user.email", "human@example.invalid"),
        "git email",
    )
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
        env={**os.environ, HELPER.EPOCH_DECLARATION: "epoch-0002"},
    )
    require(result, "agent commit")
    identity = run(repository, "git", "show", "-s", "--format=%an|%ae|%cn|%ce")
    require(identity, "read identity")
    assert identity.stdout.strip() == (
        "fixture-agent|fixture-agent@localhost|"
        "fixture-agent|fixture-agent@localhost"
    )
    assert run(repository, "git", "config", "user.name").stdout.strip() == "Human"


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
    assert "METAFLUX_AGENT_EPOCH" in text
    assert "goal.json" in text
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
    print("commit helper tests: 5 passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
