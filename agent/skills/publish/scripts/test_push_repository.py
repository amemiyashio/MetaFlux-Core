#!/usr/bin/env python3
"""Behavioral tests for the governed MetaFlux repository push helper."""

from __future__ import annotations

import importlib.util
import os
import subprocess
import sys
import tempfile
from pathlib import Path


SCRIPT = Path(__file__).with_name("push_repository.py").resolve()


def load_module():
    spec = importlib.util.spec_from_file_location("metaflux_push_repository", SCRIPT)
    if spec is None or spec.loader is None:
        raise AssertionError("push helper cannot be loaded")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


PUSH = load_module()


def isolated_git_environment() -> dict[str, str]:
    environment = dict(os.environ)
    result = subprocess.run(
        ["git", "rev-parse", "--local-env-vars"],
        check=False,
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        raise AssertionError(result.stderr)
    for variable in result.stdout.splitlines():
        environment.pop(variable, None)
    return environment


def run(root: Path, *arguments: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        list(arguments),
        cwd=root,
        env=isolated_git_environment(),
        check=False,
        capture_output=True,
        text=True,
    )


def require(result: subprocess.CompletedProcess[str], context: str) -> None:
    if result.returncode != 0:
        raise AssertionError(f"{context}:\n{result.stdout}\n{result.stderr}")


def expect_error(action, code: str):
    try:
        action()
    except ValueError as error:
        diagnostic = error.diagnostic
        assert diagnostic.code == code
        assert diagnostic.required_action
        assert diagnostic.resume_when
        return diagnostic
    raise AssertionError(f"expected diagnostic {code}")


def initialize_repository(repository: Path, tools) -> str:
    repository.mkdir(parents=True)
    require(run(repository, str(tools.git), "init", "-q"), "git init")
    require(
        run(repository, str(tools.git), "config", "user.name", "Fixture"),
        "git name",
    )
    require(
        run(
            repository,
            str(tools.git),
            "config",
            "user.email",
            "fixture@example.invalid",
        ),
        "git email",
    )
    (repository / "value.txt").write_text("fixture\n", encoding="utf-8")
    require(run(repository, str(tools.git), "add", "value.txt"), "git add")
    require(
        run(repository, str(tools.git), "commit", "-q", "-m", "fixture"),
        "git commit",
    )
    result = run(repository, str(tools.git), "rev-parse", "HEAD")
    require(result, "git revision")
    return result.stdout.strip()


def generate_key_pair(keys: Path, tools) -> str:
    keys.mkdir(parents=True)
    private_key = keys / "github-ssh-key"
    require(
        run(
            keys,
            str(tools.ssh_keygen),
            "-q",
            "-t",
            "ed25519",
            "-N",
            "",
            "-C",
            "fixture",
            "-f",
            str(private_key),
        ),
        "generate fixture key",
    )
    result = run(keys, str(tools.ssh_keygen), "-lf", str(private_key) + ".pub")
    require(result, "fingerprint fixture key")
    return result.stdout.split()[1]


def policy(fingerprint: str):
    return PUSH.TransportPolicy(
        remote_name="origin",
        remote_url="git@github.com:amemiyashio/MetaFlux-Core.git",
        destination_ref="refs/heads/main",
        private_key_relative="../../keys/github-ssh-key",
        public_key_relative="../../keys/github-ssh-key.pub",
        public_key_fingerprint=fingerprint,
        ssh_options=(
            "IdentitiesOnly=yes",
            "BatchMode=yes",
            "StrictHostKeyChecking=yes",
            "ConnectTimeout=10",
            "LogLevel=ERROR",
        ),
    )


def test_nix_tools_and_policy() -> None:
    tools = PUSH.resolve_nix_tools()
    for executable in (tools.git, tools.ssh, tools.ssh_keygen):
        assert executable.is_relative_to(Path("/nix/store"))
    loaded = PUSH.TransportPolicy.load()
    assert loaded.remote_name == "origin"
    assert loaded.destination_ref == "refs/heads/main"


def test_configuration_and_revision(root: Path) -> None:
    tools = PUSH.resolve_nix_tools()
    workspace = root / "workspace"
    repository = workspace / "repos" / "MetaFlux-Core"
    revision = initialize_repository(repository, tools)
    fingerprint = generate_key_pair(workspace / "keys", tools)
    configured_policy = policy(fingerprint)
    require(
        run(
            repository,
            str(tools.git),
            "remote",
            "add",
            "origin",
            configured_policy.remote_url,
        ),
        "add canonical remote",
    )

    assert PUSH.repository_root(repository) == repository.resolve()
    PUSH.validate_remote(configured_policy, repository, tools)
    private_key, _ = PUSH.validate_key_pair(
        configured_policy, repository, tools
    )
    PUSH.configure_repository(configured_policy, repository, tools, private_key)
    PUSH.validate_local_configuration(
        configured_policy, repository, tools, private_key
    )
    assert PUSH.validate_revision(revision, repository, tools) == revision
    invalid = expect_error(
        lambda: PUSH.validate_revision("HEAD", repository, tools),
        "git-publish.revision-invalid",
    )
    assert invalid.responsibility == "user-or-application"
    assert invalid.disposition == "preserve-and-report"

    fetch_url = run(
        repository, str(tools.git), "remote", "get-url", "origin"
    )
    require(fetch_url, "read fetch URL")
    assert fetch_url.stdout.strip() == configured_policy.remote_url
    branch = run(repository, str(tools.git), "branch", "--show-current")
    require(branch, "read branch")
    assert branch.stdout.strip() in {"main", "master"}


def test_remote_and_key_failures(root: Path) -> None:
    tools = PUSH.resolve_nix_tools()
    workspace = root / "workspace"
    repository = workspace / "repos" / "MetaFlux-Core"
    initialize_repository(repository, tools)
    fingerprint = generate_key_pair(workspace / "keys", tools)
    configured_policy = policy(fingerprint)
    require(
        run(
            repository,
            str(tools.git),
            "remote",
            "add",
            "origin",
            "git@example.invalid:other/repository.git",
        ),
        "add mismatched remote",
    )
    mismatch = expect_error(
        lambda: PUSH.validate_remote(configured_policy, repository, tools),
        "git-publish.remote-mismatch",
    )
    assert mismatch.responsibility == "user-or-application"
    private_key = configured_policy.private_key(repository)
    private_key.chmod(0o644)
    key_error = expect_error(
        lambda: PUSH.validate_key_pair(configured_policy, repository, tools),
        "git-publish.key-invalid",
    )
    assert key_error.responsibility == "host-operator"


def test_push_shape(root: Path) -> None:
    tools = PUSH.resolve_nix_tools()
    workspace = root / "workspace"
    repository = workspace / "repos" / "MetaFlux-Core"
    revision = initialize_repository(repository, tools)
    fingerprint = generate_key_pair(workspace / "keys", tools)
    configured_policy = policy(fingerprint)
    private_key = configured_policy.private_key(repository)
    calls: list[tuple[str, ...]] = []
    destination_present = False
    original_run = PUSH.run

    def fake_run(command, *, cwd, environment=None):
        calls.append(tuple(command))
        if "ls-remote" in command:
            return subprocess.CompletedProcess(
                command,
                0,
                stdout=(
                    f"{revision}\trefs/heads/main\n"
                    if destination_present
                    else ""
                ),
                stderr="",
            )
        if "push" in command:
            return subprocess.CompletedProcess(command, 0, stdout="", stderr="")
        return original_run(command, cwd=cwd, environment=environment)

    PUSH.run = fake_run
    try:
        assert not PUSH.read_remote_destination_state(
            configured_policy, repository, tools, private_key
        )
        PUSH.push_revision(
            configured_policy,
            repository,
            tools,
            private_key,
            revision,
            dry_run=True,
        )
    finally:
        PUSH.run = original_run
    push_command = next(command for command in calls if "push" in command)
    remote_check = next(command for command in calls if "ls-remote" in command)
    assert remote_check[-1] == configured_policy.destination_ref
    assert "--dry-run" in push_command
    assert "--force" not in push_command
    assert f"{revision}:refs/heads/main" in push_command


def test_local_clone_is_preserved(root: Path) -> None:
    tools = PUSH.resolve_nix_tools()
    source = root / "source"
    initialize_repository(source, tools)
    clone = root / "clone"
    require(
        run(root, str(tools.git), "clone", "-q", str(source), str(clone)),
        "local clone",
    )
    diagnostic = expect_error(
        lambda: PUSH.repository_root(clone), "git-topology.local-clone"
    )
    assert diagnostic.disposition == "preserve-and-report"


def test_publication_recovery_and_remote_graph(root: Path) -> None:
    tools = PUSH.resolve_nix_tools()
    repository = root / "repository"
    base = initialize_repository(repository, tools)
    (repository / "value.txt").write_text("delivery\n")
    require(run(repository, "git", "commit", "-am", "delivery", "-q"), "delivery")
    tip = run(repository, "git", "rev-parse", "HEAD").stdout.strip()
    (repository / "value.txt").write_text("remote advancement\n")
    require(run(repository, "git", "commit", "-am", "next", "-q"), "next")
    ahead = run(repository, "git", "rev-parse", "HEAD").stdout.strip()
    fork = run(repository, "git", "commit-tree", "HEAD^{tree}", "-p", base, "-m", "divergent").stdout.strip()
    configured = policy("unused-fixture-fingerprint")
    private_key = root / "unused-fixture-key"
    original_run, calls = PUSH.run, []
    remote = base
    def transport(command, *, cwd, environment=None):
        nonlocal remote
        if "ls-remote" in command:
            return subprocess.CompletedProcess(command, 0, remote + "\trefs/heads/main\n", "")
        if "push" in command:
            calls.append(command)
            remote = tip
            return subprocess.CompletedProcess(command, 1, "", "fixture: response lost after remote accepted commit\n")
        return original_run(command, cwd=cwd, environment=environment)
    PUSH.run = transport
    try:
        assert PUSH.push_revision(configured, repository, tools, private_key, tip, dry_run=False) == tip
        assert PUSH.push_revision(configured, repository, tools, private_key, tip, dry_run=False) == tip
        assert len(calls) == 1
        remote = ahead
        assert PUSH.push_revision(configured, repository, tools, private_key, tip, dry_run=False) == ahead
        assert len(calls) == 1
        remote = fork
        expect_error(lambda: PUSH.push_revision(configured, repository, tools, private_key, tip, dry_run=False), "git-publish.remote-diverged")
        assert len(calls) == 1
        assert not any("--force" in c for c in calls)
    finally:
        PUSH.run = original_run


def main() -> int:
    test_nix_tools_and_policy()
    with tempfile.TemporaryDirectory(prefix="metaflux-main-") as temp:
        root = Path(temp)
        test_configuration_and_revision(root / "configuration")
        test_remote_and_key_failures(root / "failures")
        test_push_shape(root / "push")
        test_local_clone_is_preserved(root / "topology")
        test_publication_recovery_and_remote_graph(root / "recovery")
    print("push repository self-test: 6/6 passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
