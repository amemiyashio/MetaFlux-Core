#!/usr/bin/env python3
"""Report the conversation-emitted harness name without probing executables."""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Mapping, Sequence

sys.path.insert(0, str(Path(__file__).resolve().parents[4] / "tools"))

from agent_diagnostics import (  # noqa: E402
    DiagnosticArgumentParser,
    DiagnosticError,
    add_diagnostic_format_argument,
    emit_diagnostics,
    task_stop_error,
)


TOOL_NAME_ENV = "METAFLUX_AGENT_TOOL"
SUBJECT_RE = re.compile(r"[a-z0-9]+(?:-[a-z0-9]+)*")
MODEL_SUBJECT_TOKENS = {
    "backend",
    "gpt",
    "haiku",
    "model",
    "opus",
    "session",
    "sonnet",
    "template",
    "thread",
}


class DetectionError(DiagnosticError):
    """Raised when the declared harness name is absent or contaminated."""


@dataclass(frozen=True)
class AgentToolInfo:
    schema_version: int
    subject: str
    interface: str
    source: str


def detection_error(
    *,
    code: str,
    summary: str,
    evidence: Sequence[object],
    responsibility: str = "user-or-application",
    disposition: str = "stop-and-report",
    required_action: str,
    resume_when: str,
    retry_command: str | None = None,
) -> DetectionError:
    diagnostic = task_stop_error(
        code=code,
        source="detect-agent-tool",
        summary=summary,
        evidence=evidence,
        responsibility=responsibility,
        disposition=disposition,
        required_action=required_action,
        resume_when=resume_when,
        retry_command=retry_command,
    ).diagnostic
    return DetectionError(diagnostic)


DECLARED_NAME_ACTION = (
    "Pass the harness name already emitted in this conversation through "
    "--agent-tool or METAFLUX_AGENT_TOOL; do not scan PATH, processes, or "
    "probe an executable, and do not use model metadata."
)
DECLARED_NAME_RESUME = (
    "The conversation-emitted harness name is a valid tool-shaped subject."
)


def normalized_subject(raw_name: str) -> str:
    candidate = raw_name.strip()
    if not candidate:
        raise detection_error(
            code="agent-tool.empty-declaration",
            summary="The agent-tool name declaration is empty.",
            evidence=("declaration contains no harness name",),
            required_action=DECLARED_NAME_ACTION,
            resume_when=DECLARED_NAME_RESUME,
        )
    if "/" in candidate or candidate.endswith(".exe"):
        candidate = Path(candidate).name
        if candidate.lower().endswith(".exe"):
            candidate = candidate[:-4]
    subject = re.sub(r"[^a-z0-9]+", "-", candidate.lower()).strip("-")
    if not subject or SUBJECT_RE.fullmatch(subject) is None:
        raise detection_error(
            code="agent-tool.invalid-subject",
            summary="The declared harness name has no valid tool-shaped subject.",
            evidence=(f"declaration: {raw_name}",),
            required_action=DECLARED_NAME_ACTION,
            resume_when=DECLARED_NAME_RESUME,
        )
    blocked = sorted(set(subject.split("-")) & MODEL_SUBJECT_TOKENS)
    if blocked:
        raise detection_error(
            code="agent-tool.prohibited-subject-input",
            summary=(
                "The declared harness name contains prohibited model or runtime "
                "metadata."
            ),
            evidence=("declared name is not valid tool evidence",),
            required_action=DECLARED_NAME_ACTION,
            resume_when=DECLARED_NAME_RESUME,
        )
    return subject


def choose_declaration(
    *,
    explicit: str | None,
    environment: Mapping[str, str],
) -> tuple[str, str]:
    if explicit is not None:
        return explicit, "declared"
    declared = environment.get(TOOL_NAME_ENV)
    if declared is not None:
        return declared, "declared"
    raise detection_error(
        code="agent-tool.missing-declaration",
        summary="No conversation-emitted harness name was declared.",
        evidence=("no --agent-tool argument or METAFLUX_AGENT_TOOL value",),
        required_action=DECLARED_NAME_ACTION,
        resume_when=DECLARED_NAME_RESUME,
    )


def detect_agent_tool(
    explicit: str | None = None,
    environment: Mapping[str, str] | None = None,
) -> AgentToolInfo:
    values = os.environ if environment is None else environment
    raw, source = choose_declaration(explicit=explicit, environment=values)
    return AgentToolInfo(
        schema_version=1,
        subject=normalized_subject(raw),
        interface="cli",
        source=source,
    )


def parser() -> argparse.ArgumentParser:
    result = DiagnosticArgumentParser(
        description="Report the conversation-emitted agent harness name.",
        diagnostic_source="detect-agent-tool",
    )
    result.add_argument(
        "--agent-tool",
        help="harness name already emitted in this conversation",
    )
    add_diagnostic_format_argument(result)
    result.add_argument("--json", action="store_true", help="emit JSON")
    result.add_argument(
        "--subject", action="store_true", help="emit only the normalized subject"
    )
    return result


def main() -> int:
    arguments = parser().parse_args()
    if arguments.json and arguments.subject:
        error = detection_error(
            code="agent-tool.output-mode-conflict",
            summary="The detector received conflicting output modes.",
            evidence=("--json and --subject were supplied together",),
            responsibility="current-agent",
            disposition="fix-and-retry",
            required_action="Choose exactly one of --json or --subject.",
            resume_when="The detector is invoked with at most one output mode.",
        )
        emit_diagnostics(
            (error.diagnostic,), diagnostic_format=arguments.diagnostic_format
        )
        return 2
    try:
        info = detect_agent_tool(explicit=arguments.agent_tool)
    except DetectionError as error:
        emit_diagnostics(
            (error.diagnostic,), diagnostic_format=arguments.diagnostic_format
        )
        return 2
    if arguments.subject:
        print(info.subject)
    else:
        print(json.dumps(asdict(info), sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
