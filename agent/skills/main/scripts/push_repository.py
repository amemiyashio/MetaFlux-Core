#!/usr/bin/env python3
"""Configure and use MetaFlux's canonical GitHub SSH push transport."""

from __future__ import annotations

import argparse
import importlib.util
import json
import os
import re
import shlex
import shutil
import stat
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Sequence


SCRIPT = Path(__file__).resolve()
REPOSITORY_HINT = SCRIPT.parents[4]
SKILL_ROOT = SCRIPT.parents[1]
POLICY_PATH = SKILL_ROOT / "transport.json"
TOPOLOGY_CHECKER_PATH = (
    SCRIPT.parents[2] / "main" / "scripts" / "check_git_topology.py"
)
sys.path.insert(0, str(REPOSITORY_HINT / "tools"))

from agent_diagnostics import (  # noqa: E402
    DiagnosticArgumentParser,
    DiagnosticError,
    add_diagnostic_format_argument,
    emit_diagnostics,
    task_stop_error,
)


FULL_OBJECT_ID_RE = re.compile(r"(?:[0-9a-f]{40}|[0-9a-f]{64})")
POLICY_FIELDS = {
    "schema_version",
    "remote_name",
    "remote_url",
    "destination_ref",
    "private_key_relative_to_repository",
    "public_key_relative_to_repository",
    "public_key_fingerprint",
    "ssh_options",
}
CONFIGURE_RETRY = (
    "nix develop . --ignore-environment --keep HOME --keep USER --command python3 -B "
    "agent/skills/main/scripts/push_repository.py configure"
)


def publish_error(
    *,
    code: str,
    summary: str,
    evidence: Sequence[object],
    responsibility: str,
    disposition: str,
    required_action: str,
    resume_when: str,
    retry_command: str | None = None,
) -> DiagnosticError:
    return task_stop_error(
        code=code,
        source="main",
        summary=summary,
        evidence=evidence,
        responsibility=responsibility,
        disposition=disposition,
        required_action=required_action,
        resume_when=resume_when,
        retry_command=retry_command,
    )


@dataclass(frozen=True)
class TransportPolicy:
    remote_name: str
    remote_url: str
    destination_ref: str
    private_key_relative: str
    public_key_relative: str
    public_key_fingerprint: str
    ssh_options: tuple[str, ...]

    @classmethod
    def load(cls, path: Path = POLICY_PATH) -> "TransportPolicy":
        try:
            value = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
            raise publish_error(
                code="git-publish.policy-invalid",
                summary="The canonical push transport policy cannot be loaded.",
                evidence=(f"policy: {path}", f"failure: {type(error).__name__}"),
                responsibility="current-agent",
                disposition="fix-and-retry",
                required_action="Repair the tracked transport policy and rerun the same command.",
                resume_when="The policy loads with its exact governed schema.",
            ) from error
        if not isinstance(value, dict) or set(value) != POLICY_FIELDS:
            raise publish_error(
                code="git-publish.policy-invalid",
                summary="The canonical push transport policy has invalid fields.",
                evidence=(f"policy: {path}",),
                responsibility="current-agent",
                disposition="fix-and-retry",
                required_action="Restore the exact transport policy schema.",
                resume_when="The policy contains only the governed fields.",
            )
        strings = POLICY_FIELDS - {"schema_version", "ssh_options"}
        if (
            value["schema_version"] != 1
            or any(
                not isinstance(value[field], str) or not value[field]
                for field in strings
            )
            or not isinstance(value["ssh_options"], list)
            or not value["ssh_options"]
            or any(
                not isinstance(option, str) or not option
                for option in value["ssh_options"]
            )
        ):
            raise publish_error(
                code="git-publish.policy-invalid",
                summary="The canonical push transport policy has invalid values.",
                evidence=(f"policy: {path}",),
                responsibility="current-agent",
                disposition="fix-and-retry",
                required_action="Restore non-empty versioned transport policy values.",
                resume_when="The policy values satisfy the governed schema.",
            )
        if not value["destination_ref"].startswith("refs/heads/"):
            raise publish_error(
                code="git-publish.policy-invalid",
                summary="The canonical push destination is not a branch ref.",
                evidence=(f"destination: {value['destination_ref']}",),
                responsibility="current-agent",
                disposition="fix-and-retry",
                required_action="Restore the approved canonical destination ref.",
                resume_when="The destination is one explicit refs/heads ref.",
            )
        return cls(
            remote_name=value["remote_name"],
            remote_url=value["remote_url"],
            destination_ref=value["destination_ref"],
            private_key_relative=value["private_key_relative_to_repository"],
            public_key_relative=value["public_key_relative_to_repository"],
            public_key_fingerprint=value["public_key_fingerprint"],
            ssh_options=tuple(value["ssh_options"]),
        )

    def private_key(self, repository_root: Path) -> Path:
        return (repository_root / self.private_key_relative).resolve()

    def public_key(self, repository_root: Path) -> Path:
        return (repository_root / self.public_key_relative).resolve()


@dataclass(frozen=True)
class NixTools:
    git: Path
    ssh: Path
    ssh_keygen: Path


def resolve_nix_tool(name: str) -> Path:
    executable = shutil.which(name)
    if executable is None:
        raise publish_error(
            code="git-publish.nix-tool-invalid",
            summary=f"Required tool {name} is absent from the Nix environment.",
            evidence=(f"tool: {name}",),
            responsibility="current-agent",
            disposition="fix-and-retry",
            required_action=(
                "Use manage-toolchain to expose the required tool from the locked "
                "repository Nix declaration."
            ),
            resume_when=f"nix develop . resolves {name} from /nix/store.",
        )
    resolved = Path(executable).resolve()
    if not resolved.is_relative_to(Path("/nix/store")):
        raise publish_error(
            code="git-publish.nix-tool-invalid",
            summary=f"Required tool {name} resolved outside the Nix store.",
            evidence=(f"resolved executable: {resolved}",),
            responsibility="current-agent",
            disposition="fix-and-retry",
            required_action=(
                "Enter the Git-aware repository Nix shell and expose the tool from "
                "the locked declaration before retrying."
            ),
            resume_when=f"nix develop . resolves {name} from /nix/store.",
        )
    return resolved


def resolve_nix_tools() -> NixTools:
    return NixTools(
        git=resolve_nix_tool("git"),
        ssh=resolve_nix_tool("ssh"),
        ssh_keygen=resolve_nix_tool("ssh-keygen"),
    )


def load_topology_checker():
    spec = importlib.util.spec_from_file_location(
        "metaflux_git_topology_for_push", TOPOLOGY_CHECKER_PATH
    )
    if spec is None or spec.loader is None:
        raise publish_error(
            code="git-publish.topology-checker-unavailable",
            summary="The canonical Git-topology checker cannot be loaded.",
            evidence=(f"checker: {TOPOLOGY_CHECKER_PATH}",),
            responsibility="current-agent",
            disposition="fix-and-retry",
            required_action="Restore main's shared topology checker.",
            resume_when="The checker loads from the same candidate tree.",
        )
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    try:
        spec.loader.exec_module(module)
    except DiagnosticError:
        raise
    except (ImportError, OSError, RuntimeError, SyntaxError) as error:
        raise publish_error(
            code="git-publish.topology-checker-unavailable",
            summary="The canonical Git-topology checker failed to load.",
            evidence=(
                f"checker: {TOPOLOGY_CHECKER_PATH}",
                f"failure: {type(error).__name__}",
            ),
            responsibility="current-agent",
            disposition="fix-and-retry",
            required_action="Repair main's shared topology checker.",
            resume_when="The checker loads from the same candidate tree.",
        ) from error
    return module


def repository_root(requested_root: Path) -> Path:
    checker = load_topology_checker()
    context = checker.resolve_git_topology(requested_root.resolve())
    return Path(context.repository_root)


def run(
    command: Sequence[str],
    *,
    cwd: Path,
    environment: dict[str, str] | None = None,
) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        list(command),
        cwd=cwd,
        env=environment,
        check=False,
        capture_output=True,
        text=True,
    )


def validate_key_pair(
    policy: TransportPolicy, repository: Path, tools: NixTools
) -> tuple[Path, Path]:
    private_key = policy.private_key(repository)
    public_key = policy.public_key(repository)
    try:
        private_metadata = private_key.stat()
        public_metadata = public_key.stat()
    except OSError as error:
        raise publish_error(
            code="git-publish.key-invalid",
            summary="The canonical external SSH key pair is unavailable.",
            evidence=(f"failure: {type(error).__name__}",),
            responsibility="host-operator",
            disposition="stop-and-report",
            required_action=(
                "Restore the configured key pair at the canonical workspace paths; "
                "do not copy a credential into the repository."
            ),
            resume_when="Both canonical key files exist with valid metadata.",
        ) from error
    private_mode = stat.S_IMODE(private_metadata.st_mode)
    if (
        not stat.S_ISREG(private_metadata.st_mode)
        or not stat.S_ISREG(public_metadata.st_mode)
        or private_mode & 0o077
    ):
        raise publish_error(
            code="git-publish.key-invalid",
            summary="The canonical SSH key files or private permissions are invalid.",
            evidence=(
                f"private regular file: {stat.S_ISREG(private_metadata.st_mode)}",
                f"private mode: {private_mode:03o}",
                f"public regular file: {stat.S_ISREG(public_metadata.st_mode)}",
            ),
            responsibility="host-operator",
            disposition="stop-and-report",
            required_action=(
                "Restore regular key files and remove all group/other private-key "
                "permissions at the canonical paths."
            ),
            resume_when="The private key is a regular file with no group/other access.",
        )
    result = run([str(tools.ssh_keygen), "-lf", str(public_key)], cwd=repository)
    fields = result.stdout.split()
    fingerprint = fields[1] if result.returncode == 0 and len(fields) >= 2 else ""
    if fingerprint != policy.public_key_fingerprint:
        raise publish_error(
            code="git-publish.key-invalid",
            summary="The canonical public SSH key fingerprint does not match policy.",
            evidence=(
                f"observed fingerprint: {fingerprint or '<unavailable>'}",
                f"expected fingerprint: {policy.public_key_fingerprint}",
                f"ssh-keygen return code: {result.returncode}",
            ),
            responsibility="host-operator",
            disposition="stop-and-report",
            required_action=(
                "Restore the intended key pair or explicitly govern a rotated public "
                "fingerprint before any push."
            ),
            resume_when="The public key matches the governed fingerprint.",
        )
    return private_key, public_key


def ssh_command(policy: TransportPolicy, tools: NixTools, private_key: Path) -> str:
    arguments = [str(tools.ssh), "-F", "/dev/null", "-i", str(private_key)]
    for option in policy.ssh_options:
        arguments.extend(("-o", option))
    return shlex.join(arguments)


def git_config_values(
    tools: NixTools, repository: Path, key: str
) -> tuple[str, ...]:
    result = run(
        [str(tools.git), "config", "--local", "--get-all", key], cwd=repository
    )
    if result.returncode == 1:
        return ()
    if result.returncode != 0:
        raise publish_error(
            code="git-publish.local-config-invalid",
            summary="Repository-local Git configuration could not be read.",
            evidence=(f"config key: {key}", f"return code: {result.returncode}"),
            responsibility="current-agent",
            disposition="fix-and-retry",
            required_action="Repair the existing repository-local Git configuration.",
            resume_when="The same local Git config query succeeds.",
        )
    return tuple(result.stdout.splitlines())


def validate_remote(
    policy: TransportPolicy, repository: Path, tools: NixTools
) -> None:
    result = run(
        [str(tools.git), "remote", "get-url", policy.remote_name], cwd=repository
    )
    observed = result.stdout.strip() if result.returncode == 0 else "<missing>"
    if observed != policy.remote_url:
        raise publish_error(
            code="git-publish.remote-mismatch",
            summary="The existing Git remote does not match the canonical repository.",
            evidence=(
                f"remote: {policy.remote_name}",
                f"observed URL: {observed}",
                f"expected URL: {policy.remote_url}",
            ),
            responsibility="user-or-application",
            disposition="preserve-and-report",
            required_action=(
                "Preserve the remote and local revisions, then explicitly supply or "
                "restore the canonical repository context."
            ),
            resume_when="The existing remote URL exactly matches transport policy.",
        )


def expected_configuration(
    policy: TransportPolicy, tools: NixTools, private_key: Path
) -> dict[str, str]:
    return {
        f"remote.{policy.remote_name}.pushurl": policy.remote_url,
        "core.sshCommand": ssh_command(policy, tools, private_key),
        "ssh.variant": "ssh",
        "push.default": "nothing",
    }


def configure_repository(
    policy: TransportPolicy, repository: Path, tools: NixTools, private_key: Path
) -> None:
    for key, value in expected_configuration(policy, tools, private_key).items():
        result = run(
            [
                str(tools.git),
                "config",
                "--local",
                "--replace-all",
                key,
                value,
            ],
            cwd=repository,
        )
        if result.returncode != 0:
            raise publish_error(
                code="git-publish.local-config-write-failed",
                summary="Repository-local push transport configuration was not written.",
                evidence=(f"config key: {key}", f"return code: {result.returncode}"),
                responsibility="current-agent",
                disposition="fix-and-retry",
                required_action=(
                    "Preserve existing repository state, repair the local config "
                    "write failure, and rerun bounded configure mode."
                ),
                resume_when="All governed local Git values are written exactly once.",
                retry_command=CONFIGURE_RETRY,
            )


def validate_local_configuration(
    policy: TransportPolicy, repository: Path, tools: NixTools, private_key: Path
) -> None:
    mismatches: list[str] = []
    for key, expected in expected_configuration(policy, tools, private_key).items():
        observed = git_config_values(tools, repository, key)
        if observed != (expected,):
            mismatches.append(f"{key}: {len(observed)} configured value(s)")
    if mismatches:
        raise publish_error(
            code="git-publish.local-config-mismatch",
            summary="Repository-local push transport does not match policy.",
            evidence=mismatches,
            responsibility="current-agent",
            disposition="fix-and-retry",
            required_action=(
                "Run the bounded configure mode in this existing repository; do not "
                "change global Git or SSH configuration."
            ),
            resume_when="A local check observes every governed value exactly once.",
            retry_command=CONFIGURE_RETRY,
        )


def command_environment() -> dict[str, str]:
    environment = os.environ.copy()
    environment["GIT_TERMINAL_PROMPT"] = "0"
    return environment


def read_remote_destination_state(
    policy: TransportPolicy,
    repository: Path,
    tools: NixTools,
    private_key: Path,
) -> str | None:
    command = [
        str(tools.git),
        "-c",
        f"core.sshCommand={ssh_command(policy, tools, private_key)}",
        "-c",
        "ssh.variant=ssh",
        "ls-remote",
        policy.remote_name,
        policy.destination_ref,
    ]
    result = run(command, cwd=repository, environment=command_environment())
    if result.returncode != 0:
        if result.stdout:
            print(result.stdout, end="", file=sys.stdout)
        if result.stderr:
            print(result.stderr, end="", file=sys.stderr)
        raise publish_error(
            code="git-publish.remote-command-failed",
            summary="The canonical Git remote could not be authenticated or read.",
            evidence=(
                f"command: {shlex.join(command)}",
                f"return code: {result.returncode}",
            ),
            responsibility="user-or-application",
            disposition="preserve-and-report",
            required_action=(
                "Preserve local revisions; ensure the governed public key is "
                "registered for the GitHub account or repository with required "
                "access, then inspect known-host and connectivity state."
            ),
            resume_when="The same read-only command authenticates successfully.",
        )
    matches = [line.partition("\t")[0] for line in result.stdout.splitlines()
               if line.partition("\t")[2] == policy.destination_ref]
    if not matches:
        return None
    if len(matches) != 1 or FULL_OBJECT_ID_RE.fullmatch(matches[0]) is None:
        raise publish_error(code="git-publish.remote-revision-invalid", summary="Remote main did not resolve to one complete revision.",
            evidence=tuple(matches), responsibility="user-or-application", disposition="preserve-and-report",
            required_action="Inspect the canonical remote and preserve the local commit.",
            resume_when="Remote main resolves to one full commit ID.")
    return matches[0]


def remote_relation(policy: TransportPolicy, repository: Path, tools: NixTools,
                    private_key: Path, revision: str, remote: str | None) -> str:
    if remote is None:
        return "absent"
    if remote == revision:
        return "same"
    exists = run([str(tools.git), "cat-file", "-e", remote + "^{commit}"], cwd=repository)
    if exists.returncode:
        fetched = run([str(tools.git), "-c", f"core.sshCommand={ssh_command(policy, tools, private_key)}",
                       "-c", "ssh.variant=ssh", "fetch", "--no-tags", "--no-write-fetch-head",
                       "--no-recurse-submodules", policy.remote_name, remote], cwd=repository,
                      environment=command_environment())
        if fetched.returncode:
            raise publish_error(code="git-publish.remote-command-failed", summary="Remote ancestry could not be verified.",
                evidence=(remote, fetched.stderr), responsibility="user-or-application", disposition="preserve-and-report",
                required_action="Preserve the exact commit and restore access to the canonical remote object.",
                resume_when="The exact remote revision is available for an ancestry check.")
    if run([str(tools.git), "merge-base", "--is-ancestor", revision, remote], cwd=repository).returncode == 0:
        return "ahead"
    if run([str(tools.git), "merge-base", "--is-ancestor", remote, revision], cwd=repository).returncode == 0:
        return "behind"
    return "divergent"


def validate_revision(revision: str, repository: Path, tools: NixTools) -> str:
    if FULL_OBJECT_ID_RE.fullmatch(revision) is None:
        raise publish_error(
            code="git-publish.revision-invalid",
            summary="Push input is not one full commit object ID.",
            evidence=(f"supplied length: {len(revision)}",),
            responsibility="user-or-application",
            disposition="preserve-and-report",
            required_action="Supply the exact full commit object ID intended for main.",
            resume_when="The supplied ID names one existing local commit object.",
        )
    result = run(
        [
            str(tools.git),
            "rev-parse",
            "--verify",
            "--end-of-options",
            f"{revision}^{{commit}}",
        ],
        cwd=repository,
    )
    resolved = result.stdout.strip() if result.returncode == 0 else ""
    if resolved != revision:
        raise publish_error(
            code="git-publish.revision-invalid",
            summary="The supplied object ID does not resolve to that exact commit.",
            evidence=(
                f"supplied object: {revision}",
                f"resolved object: {resolved or '<missing>'}",
            ),
            responsibility="user-or-application",
            disposition="preserve-and-report",
            required_action="Supply one full existing local commit object ID.",
            resume_when="The exact object resolves as a commit without abbreviation.",
        )
    return resolved


def push_revision(
    policy: TransportPolicy,
    repository: Path,
    tools: NixTools,
    private_key: Path,
    revision: str,
    *,
    dry_run: bool,
) -> str | None:
    remote = read_remote_destination_state(policy, repository, tools, private_key)
    relation = remote_relation(policy, repository, tools, private_key, revision, remote)
    if relation in {"same", "ahead"}:
        return remote
    if relation == "divergent":
        raise publish_error(code="git-publish.remote-diverged", summary="Local delivery and remote main have diverged.",
            evidence=(f"delivery: {revision}", f"remote main: {remote}"), responsibility="user-or-application",
            disposition="preserve-and-report", required_action="Supply a new exact integration context; retain the existing commit and do not force push.",
            resume_when="The application supplies a current dependency-valid execution base.")
    command = [
        str(tools.git),
        "-c",
        f"core.sshCommand={ssh_command(policy, tools, private_key)}",
        "-c",
        "ssh.variant=ssh",
        "push",
        "--porcelain",
        "--atomic",
    ]
    if dry_run:
        command.append("--dry-run")
    command.extend(
        ("--", policy.remote_name, f"{revision}:{policy.destination_ref}")
    )
    result = run(command, cwd=repository, environment=command_environment())
    if result.stdout:
        print(result.stdout, end="", file=sys.stdout)
    if result.stderr:
        print(result.stderr, end="", file=sys.stderr)
    observed = read_remote_destination_state(policy, repository, tools, private_key) if not dry_run else remote
    if not dry_run and remote_relation(policy, repository, tools, private_key, revision, observed) in {"same", "ahead"}:
        return observed
    if result.returncode != 0 or not dry_run:
        raise publish_error(
            code="git-publish.remote-command-failed",
            summary="The exact governed Git push did not complete.",
            evidence=(
                f"revision: {revision}",
                f"destination: {policy.destination_ref}",
                f"return code: {result.returncode}",
                f"dry run: {dry_run}",
            ),
            responsibility="user-or-application",
            disposition="preserve-and-report",
            required_action=(
                "Preserve the local revision and inspect the raw Git/SSH output; "
                "retry only after remote, authentication, trust, or connectivity "
                "evidence changes."
            ),
            resume_when="The same exact refspec push exits successfully without force.",
        )
    return observed


def parser() -> argparse.ArgumentParser:
    result = DiagnosticArgumentParser(
        description=__doc__, diagnostic_source="main"
    )
    result.add_argument("--root", type=Path, default=Path("."))
    add_diagnostic_format_argument(result)
    commands = result.add_subparsers(dest="command", required=True)
    check = commands.add_parser("check")
    check.add_argument("--remote-access", action="store_true")
    commands.add_parser("configure")
    push = commands.add_parser("push")
    push.add_argument("--revision", required=True)
    push.add_argument("--dry-run", action="store_true")
    return result


def main() -> int:
    arguments = parser().parse_args()
    try:
        policy = TransportPolicy.load()
        tools = resolve_nix_tools()
        repository = repository_root(arguments.root)
        validate_remote(policy, repository, tools)
        private_key, _ = validate_key_pair(policy, repository, tools)
        if arguments.command == "configure":
            configure_repository(policy, repository, tools, private_key)
            validate_local_configuration(policy, repository, tools, private_key)
            print(
                "push repository: configured "
                f"{policy.remote_name} -> {policy.destination_ref} "
                f"({policy.public_key_fingerprint})"
            )
            return 0
        validate_local_configuration(policy, repository, tools, private_key)
        if arguments.command == "check":
            destination_state = "not-checked"
            if arguments.remote_access:
                destination_exists = read_remote_destination_state(
                    policy, repository, tools, private_key
                )
                destination_state = "present" if destination_exists else "absent"
            print(
                "push repository: ok "
                f"git={tools.git} ssh={tools.ssh} "
                f"remote_access={'verified' if arguments.remote_access else 'not-requested'} "
                f"destination={destination_state}"
            )
            return 0
        revision = validate_revision(arguments.revision, repository, tools)
        remote = push_revision(
            policy,
            repository,
            tools,
            private_key,
            revision,
            dry_run=arguments.dry_run,
        )
        print(
            "push repository: "
            f"{'dry-run ' if arguments.dry_run else ''}published "
            f"{revision} -> {policy.destination_ref}"
        )
        print(json.dumps({"published_revision": revision if not arguments.dry_run else None,
                          "remote_main_revision": remote,
                          "context_refresh_required": bool(remote and remote != revision and not arguments.dry_run)}))
        return 0
    except DiagnosticError as error:
        emit_diagnostics(
            (error.diagnostic,), diagnostic_format=arguments.diagnostic_format
        )
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
