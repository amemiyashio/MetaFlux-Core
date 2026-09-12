#!/usr/bin/env python3
"""Create one Git commit with detected agent-tool identity."""

from __future__ import annotations

import argparse
import importlib.util
import os
import re
import shlex
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import workflow_state as ws

sys.path.insert(0, str(Path(__file__).resolve().parents[4] / "tools"))

from agent_diagnostics import (  # noqa: E402
    DiagnosticArgumentParser,
    DiagnosticError,
    add_diagnostic_format_argument,
    emit_diagnostics,
    extract_diagnostic_envelopes,
    task_stop_error,
)


TOOL_NAME_DECLARATION = "METAFLUX_AGENT_TOOL"
COMMIT_GATE_ENVIRONMENT = {
    "expected_head": "METAFLUX_EXPECTED_HEAD",
    "expected_tree": "METAFLUX_EXPECTED_TREE",
    "receipt": "METAFLUX_VERIFICATION_RECEIPT",
    "kind": "METAFLUX_COMMIT_KIND",
}
COMMIT_KINDS = ("maintenance", "iteration", "batch", "epoch")
INPUT_REPAIR_ACTION = (
    "Follow the specific input diagnostic with $main skill. For staging alone, "
    "stage the reported reviewed paths and retry delivery with the same receipt. "
    "For stale evidence, inspect and repair that prerequisite; recover an "
    "interrupted commit or publication by its exact revision."
)
SCRIPT = Path(__file__).resolve()
DETECTOR_SCRIPT = (
    SCRIPT.parents[2]
    / "main"
    / "scripts"
    / "detect_agent_tool.py"
)
TOPOLOGY_CHECKER_SCRIPT = SCRIPT.with_name("check_git_topology.py")


def helper_error(
    *,
    code: str,
    summary: str,
    evidence: tuple[object, ...],
    required_action: str,
    resume_when: str,
    responsibility: str = "current-agent",
    disposition: str = "fix-and-retry",
) -> DiagnosticError:
    return task_stop_error(
        code=code,
        source="main / commit helper",
        summary=summary,
        evidence=evidence,
        responsibility=responsibility,
        disposition=disposition,
        required_action=required_action,
        resume_when=resume_when,
    )


def load_detector():
    spec = importlib.util.spec_from_file_location(
        "metaflux_agent_tool_for_commit", DETECTOR_SCRIPT
    )
    if spec is None or spec.loader is None:
        raise helper_error(
            code="commit-helper.detector-unavailable",
            summary="The agent-tool detector cannot be loaded.",
            evidence=(f"detector: {DETECTOR_SCRIPT}",),
            required_action="Restore the candidate-tree detector and rerun the helper.",
            resume_when="The detector module loads from the same candidate tree.",
        )
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    try:
        spec.loader.exec_module(module)
    except DiagnosticError:
        raise
    except (ImportError, OSError, RuntimeError, SyntaxError) as error:
        raise helper_error(
            code="commit-helper.detector-unavailable",
            summary="The agent-tool detector failed to load.",
            evidence=(
                f"detector: {DETECTOR_SCRIPT}",
                f"failure: {type(error).__name__}",
            ),
            required_action="Repair the candidate-tree detector and rerun the helper.",
            resume_when="The detector module loads from the same candidate tree.",
        ) from error
    return module


def load_topology_checker():
    spec = importlib.util.spec_from_file_location(
        "metaflux_git_topology_for_commit", TOPOLOGY_CHECKER_SCRIPT
    )
    if spec is None or spec.loader is None:
        raise helper_error(
            code="commit-helper.topology-checker-unavailable",
            summary="The Git-topology checker cannot be loaded.",
            evidence=(f"checker: {TOPOLOGY_CHECKER_SCRIPT}",),
            required_action=(
                "Restore the candidate-tree topology checker and rerun the helper."
            ),
            resume_when="The topology checker loads from the same candidate tree.",
        )
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    try:
        spec.loader.exec_module(module)
    except DiagnosticError:
        raise
    except (ImportError, OSError, RuntimeError, SyntaxError) as error:
        raise helper_error(
            code="commit-helper.topology-checker-unavailable",
            summary="The Git-topology checker failed to load.",
            evidence=(
                f"checker: {TOPOLOGY_CHECKER_SCRIPT}",
                f"failure: {type(error).__name__}",
            ),
            required_action=(
                "Repair the candidate-tree topology checker and rerun the helper."
            ),
            resume_when="The topology checker loads from the same candidate tree.",
        ) from error
    return module


DETECTOR = None
TOPOLOGY_CHECKER = None
MODULE_LOAD_ERROR: DiagnosticError | None = None
try:
    DETECTOR = load_detector()
    TOPOLOGY_CHECKER = load_topology_checker()
except DiagnosticError as error:
    MODULE_LOAD_ERROR = error


@dataclass(frozen=True)
class AgentIdentity:
    subject: str

    @property
    def name(self) -> str:
        return self.subject

    @property
    def email(self) -> str:
        return f"{self.subject}@localhost"


def declared_identity(
    environment: dict[str, str], explicit_name: str | None = None
) -> AgentIdentity:
    if MODULE_LOAD_ERROR is not None or DETECTOR is None:
        raise MODULE_LOAD_ERROR or helper_error(
            code="commit-helper.detector-unavailable",
            summary="The agent-tool detector is unavailable.",
            evidence=(f"detector: {DETECTOR_SCRIPT}",),
            required_action="Restore the detector and rerun the commit helper.",
            resume_when="The detector loads from the same candidate tree.",
        )
    tool = DETECTOR.detect_agent_tool(
        explicit=explicit_name,
        environment=environment,
    )
    return AgentIdentity(subject=tool.subject)


def commit_arguments(raw_arguments: list[str]) -> list[str]:
    arguments = list(raw_arguments)
    if arguments and arguments[0] == "--":
        arguments.pop(0)
    if not arguments:
        raise helper_error(
            code="commit-helper.missing-arguments",
            summary="Git commit arguments are required after --.",
            evidence=("no Git commit arguments were supplied",),
            required_action="Supply a commit message through the governed helper.",
            resume_when="The helper receives a valid git commit argument list.",
        )

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
            raise helper_error(
                code="commit-helper.identity-option-conflict",
                summary="A Git option conflicts with detected agent-tool identity.",
                evidence=(f"conflicting option: {long_name}",),
                required_action=(
                    "Remove the identity- or history-reuse option and create a new "
                    "commit through this helper."
                ),
                resume_when="No protected identity or message-reuse option is present.",
            )
        if argument.startswith("-") and not argument.startswith("--"):
            if "C" in argument[1:] or "c" in argument[1:]:
                raise helper_error(
                    code="commit-helper.identity-option-conflict",
                    summary="Git message reuse conflicts with detected agent-tool identity.",
                    evidence=(f"conflicting option: {argument}",),
                    required_action=(
                        "Remove the message-reuse option and create a new commit "
                        "through this helper."
                    ),
                    resume_when="No protected identity or message-reuse option is present.",
                )
    if len(arguments) != 2 or arguments[0] not in {"-m", "--message", "-F", "--file"}:
        raise helper_error(code="commit-helper.arguments-invalid", summary="The helper accepts one explicit commit message.",
            evidence=("use -m MESSAGE or -F FILE",), required_action="Stage the exact reviewed tree and supply its message.",
            resume_when="Commit arguments contain only a message option and its value.")
    return arguments


def parser() -> argparse.ArgumentParser:
    result = DiagnosticArgumentParser(
        description="Run git commit under detected agent-tool identity.",
        diagnostic_source="iteration / commit helper",
    )
    result.add_argument(
        "--agent-tool", help="harness name already emitted in this conversation"
    )
    result.add_argument("--print-identity", action="store_true")
    result.add_argument("--check-commit-gate", action="store_true",
                        help="validate the exact helper inputs inherited by the Git hook")
    result.add_argument("--expected-head")
    result.add_argument("--expected-tree")
    result.add_argument("--receipt", type=Path)
    result.add_argument("--kind", choices=COMMIT_KINDS)
    add_diagnostic_format_argument(result)
    result.add_argument("git_arguments", nargs=argparse.REMAINDER)
    return result


def check_commit_gate(root: Path, environment: dict[str, str]) -> None:
    inputs = {key: environment.get(name, "") for key, name in COMMIT_GATE_ENVIRONMENT.items()}
    missing = [name for key, name in COMMIT_GATE_ENVIRONMENT.items() if not inputs[key]]
    if missing:
        raise helper_error(
            code="commit-gate.evidence-required",
            summary="The Git commit gate requires the governed helper's exact verification inputs.",
            evidence=("missing: " + ", ".join(missing),),
            required_action="Use $main skill to review, evaluate, and deliver through the commit helper.",
            resume_when="Exact HEAD, staged tree, verification receipt, and operation kind are supplied.",
        )
    try:
        ws.exact_commit(root, inputs["expected_head"])
        ws.require(bool(re.fullmatch(r"[0-9a-f]{40}", inputs["expected_tree"])),
                   "The expected tree must be a full object ID")
        ws.require(inputs["kind"] in COMMIT_KINDS, "Unknown commit operation kind")
        receipt_path = Path(inputs["receipt"])
        ws.require(receipt_path.is_absolute(), "The verification receipt path must be absolute")
        receipt = ws.read_json(receipt_path)
        ws.commit_guard(root, inputs["expected_head"], inputs["expected_tree"], receipt, kind=inputs["kind"])
    except (ws.WorkflowError, OSError, ValueError, KeyError, TypeError) as error:
        raise helper_error(
            code="commit-gate.input-changed",
            summary="The Git commit gate's verification inputs need revalidation.",
            evidence=(str(error),),
            required_action=INPUT_REPAIR_ACTION,
            resume_when="Expected HEAD/tree, operation kind, and the current receipt agree.",
        ) from error


def main() -> int:
    arguments = parser().parse_args()
    try:
        if arguments.check_commit_gate:
            if arguments.print_identity or arguments.git_arguments:
                raise helper_error(
                    code="commit-gate.output-mode-conflict",
                    summary="Commit-gate inspection requires its own operating mode.",
                    evidence=("identity output or commit arguments were supplied",),
                    required_action="Run the hook's evidence inspection without another helper mode.",
                    resume_when="Only --check-commit-gate and diagnostic formatting are requested.",
                )
            check_commit_gate(Path.cwd(), dict(os.environ))
            return 0
        if MODULE_LOAD_ERROR is not None or TOPOLOGY_CHECKER is None:
            raise MODULE_LOAD_ERROR or helper_error(
                code="commit-helper.topology-checker-unavailable",
                summary="The Git-topology checker is unavailable.",
                evidence=(f"checker: {TOPOLOGY_CHECKER_SCRIPT}",),
                required_action="Restore the checker and rerun the commit helper.",
                resume_when="The checker loads from the same candidate tree.",
            )
        identity = declared_identity(
            dict(os.environ), explicit_name=arguments.agent_tool
        )
        TOPOLOGY_CHECKER.resolve_git_topology(Path.cwd())
        if arguments.print_identity:
            if arguments.git_arguments:
                raise helper_error(
                    code="commit-helper.output-mode-conflict",
                    summary="--print-identity does not accept commit arguments.",
                    evidence=("identity preflight received Git commit arguments",),
                    required_action=(
                        "Run identity preflight alone, or remove --print-identity to commit."
                    ),
                    resume_when="The helper receives exactly one operating mode.",
                )
            print(f"{identity.name} <{identity.email}>")
            return 0
        git_arguments = commit_arguments(arguments.git_arguments)
        if not all((arguments.expected_head, arguments.expected_tree, arguments.receipt, arguments.kind)):
            raise helper_error(code="commit-helper.evidence-required", summary="A guarded commit requires exact input and executed verification.",
                evidence=("expected-head, expected-tree, receipt, and kind are required",),
                required_action="Use $main skill to review and evaluate the current tree before delivery.",
                resume_when="The exact staged tree has a current verification receipt.")
    except DiagnosticError as error:
        emit_diagnostics(
            (error.diagnostic,), diagnostic_format=arguments.diagnostic_format
        )
        return 2

    environment = os.environ.copy()
    environment.pop("METAFLUX_MAIN_LOCK_FD", None)
    environment.update(
        {
            TOOL_NAME_DECLARATION: identity.subject,
            "METAFLUX_DIAGNOSTIC_FORMAT": "json",
            "GIT_AUTHOR_NAME": identity.name,
            "GIT_AUTHOR_EMAIL": identity.email,
            "GIT_COMMITTER_NAME": identity.name,
            "GIT_COMMITTER_EMAIL": identity.email,
        }
    )
    print(
        f"agent identity: {identity.name} <{identity.email}>",
        file=sys.stderr,
    )
    environment["METAFLUX_EXPECTED_HEAD"] = arguments.expected_head
    environment["METAFLUX_EXPECTED_TREE"] = arguments.expected_tree
    environment["METAFLUX_VERIFICATION_RECEIPT"] = str(arguments.receipt.resolve())
    environment["METAFLUX_COMMIT_KIND"] = arguments.kind
    try:
        with ws.lock(Path.cwd()):
            receipt = ws.read_json(arguments.receipt)
            ws.commit_guard(Path.cwd(), arguments.expected_head, arguments.expected_tree, receipt, kind=arguments.kind)
            result = subprocess.run(["git", "commit", *git_arguments], check=False,
                                    capture_output=True, text=True, env=environment)
            if result.returncode == 0:
                ws.require(ws.oid(Path.cwd(), "HEAD^{tree}") == arguments.expected_tree,
                           "Committed tree differs from expected tree; preserve the exact revision for inspection")
    except (ws.WorkflowError, OSError, ValueError, KeyError, TypeError) as error:
        emit_diagnostics((helper_error(code="commit-helper.input-changed", summary="The guarded commit inputs need revalidation.",
            evidence=(str(error),), required_action=INPUT_REPAIR_ACTION,
            resume_when="Expected HEAD/tree and the current verification receipt agree.").diagnostic,),
            diagnostic_format=arguments.diagnostic_format)
        return 2
    if result.stdout:
        print(result.stdout, end="", file=sys.stdout)
    if result.returncode == 0:
        if result.stderr:
            print(result.stderr, end="", file=sys.stderr)
        return 0

    child_diagnostics, passthrough_stderr = extract_diagnostic_envelopes(
        result.stderr
    )
    if passthrough_stderr:
        print(passthrough_stderr, end="", file=sys.stderr)
    if result.returncode != 0:
        diagnostics = child_diagnostics
        if not diagnostics:
            diagnostics = (
                helper_error(
                    code="verification.required-gate-failed",
                    summary="The required Git commit gate did not pass.",
                    evidence=(
                        "command: " + shlex.join(["git", "commit", *git_arguments]),
                        f"return code: {result.returncode}",
                    ),
                    required_action=(
                        "Correct the evidenced candidate or invocation in this "
                        "existing context, then rerun the same governed helper; do "
                        "not bypass the hook or create replacement source state."
                    ),
                    resume_when=(
                        "The same governed commit helper and required hook pass."
                    ),
                ).diagnostic,
            )
        emit_diagnostics(
            diagnostics, diagnostic_format=arguments.diagnostic_format
        )
    return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
