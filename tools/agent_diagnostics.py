#!/usr/bin/env python3
"""Structured task-stop diagnostics for MetaFlux Agent workflow gates."""

from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass
from typing import Iterable, Sequence, TextIO


DIAGNOSTIC_SCHEMA_VERSION = 1
CODE_RE = re.compile(
    r"[a-z0-9]+(?:-[a-z0-9]+)*(?:\.[a-z0-9]+(?:-[a-z0-9]+)*)+"
)
RESPONSIBILITIES = {
    "current-agent",
    "user-or-application",
    "batch-integrator",
    "epoch-governor",
    "host-operator",
}
DISPOSITIONS = {
    "fix-and-retry",
    "stop-and-report",
    "preserve-and-report",
}
DIAGNOSTIC_FORMATS = {"human", "json"}
MAX_EVIDENCE_ITEMS = 8
MAX_EVIDENCE_LENGTH = 512


def bounded_text(value: object, limit: int = MAX_EVIDENCE_LENGTH) -> str:
    """Collapse control layout and bound one diagnostic evidence value."""

    text = " ".join(str(value).split()).strip()
    if len(text) <= limit:
        return text
    return text[: limit - 3] + "..."


def bounded_evidence(values: Iterable[object]) -> tuple[str, ...]:
    result: list[str] = []
    for value in values:
        text = bounded_text(value)
        if text:
            result.append(text)
        if len(result) == MAX_EVIDENCE_ITEMS:
            break
    return tuple(result)


def _require_text(value: str, field: str) -> None:
    if not isinstance(value, str) or not value.strip():
        raise ValueError(f"diagnostic {field} must be non-empty text")


@dataclass(frozen=True)
class TaskStopDiagnostic:
    code: str
    source: str
    summary: str
    evidence: tuple[str, ...]
    responsibility: str
    disposition: str
    required_action: str
    resume_when: str
    retry_command: str | None = None

    def __post_init__(self) -> None:
        if CODE_RE.fullmatch(self.code) is None:
            raise ValueError("diagnostic code must be a dotted lowercase semantic name")
        for field in ("source", "summary", "required_action", "resume_when"):
            _require_text(getattr(self, field), field)
        if self.responsibility not in RESPONSIBILITIES:
            raise ValueError("diagnostic responsibility is invalid")
        if self.disposition not in DISPOSITIONS:
            raise ValueError("diagnostic disposition is invalid")
        if not isinstance(self.evidence, tuple) or not self.evidence:
            raise ValueError("diagnostic evidence must be a non-empty tuple")
        if len(self.evidence) > MAX_EVIDENCE_ITEMS:
            raise ValueError("diagnostic evidence contains too many items")
        for item in self.evidence:
            _require_text(item, "evidence item")
            if len(item) > MAX_EVIDENCE_LENGTH:
                raise ValueError("diagnostic evidence item is too long")
        if self.retry_command is not None:
            _require_text(self.retry_command, "retry_command")

    def as_dict(self) -> dict[str, object]:
        result: dict[str, object] = {
            "code": self.code,
            "source": self.source,
            "summary": self.summary,
            "evidence": list(self.evidence),
            "responsibility": self.responsibility,
            "disposition": self.disposition,
            "required_action": self.required_action,
            "resume_when": self.resume_when,
        }
        if self.retry_command is not None:
            result["retry_command"] = self.retry_command
        return result

    @classmethod
    def from_dict(cls, value: object) -> "TaskStopDiagnostic":
        if not isinstance(value, dict):
            raise ValueError("diagnostic entry must be an object")
        expected = {
            "code",
            "source",
            "summary",
            "evidence",
            "responsibility",
            "disposition",
            "required_action",
            "resume_when",
        }
        if set(value) not in (expected, expected | {"retry_command"}):
            raise ValueError("diagnostic entry fields are invalid")
        evidence = value.get("evidence")
        if not isinstance(evidence, list) or any(
            not isinstance(item, str) for item in evidence
        ):
            raise ValueError("diagnostic evidence must be a string list")
        fields = {name: value[name] for name in expected - {"evidence"}}
        if any(not isinstance(item, str) for item in fields.values()):
            raise ValueError("diagnostic text fields must be strings")
        retry_command = value.get("retry_command")
        if retry_command is not None and not isinstance(retry_command, str):
            raise ValueError("diagnostic retry_command must be text")
        return cls(
            code=fields["code"],
            source=fields["source"],
            summary=fields["summary"],
            evidence=tuple(evidence),
            responsibility=fields["responsibility"],
            disposition=fields["disposition"],
            required_action=fields["required_action"],
            resume_when=fields["resume_when"],
            retry_command=retry_command,
        )


class DiagnosticError(ValueError):
    """An exception whose public failure contract is fully structured."""

    def __init__(self, diagnostic: TaskStopDiagnostic):
        self.diagnostic = diagnostic
        super().__init__(diagnostic.summary)


def task_stop_error(
    *,
    code: str,
    source: str,
    summary: str,
    evidence: Iterable[object],
    responsibility: str,
    disposition: str,
    required_action: str,
    resume_when: str,
    retry_command: str | None = None,
) -> DiagnosticError:
    return DiagnosticError(
        TaskStopDiagnostic(
            code=code,
            source=source,
            summary=summary,
            evidence=bounded_evidence(evidence),
            responsibility=responsibility,
            disposition=disposition,
            required_action=required_action,
            resume_when=resume_when,
            retry_command=retry_command,
        )
    )


def diagnostic_envelope(
    diagnostics: Sequence[TaskStopDiagnostic],
) -> dict[str, object]:
    if not diagnostics:
        raise ValueError("at least one diagnostic is required")
    return {
        "schema_version": DIAGNOSTIC_SCHEMA_VERSION,
        "status": "error",
        "errors": [diagnostic.as_dict() for diagnostic in diagnostics],
    }


def parse_diagnostic_envelope(raw: str) -> tuple[TaskStopDiagnostic, ...]:
    value = json.loads(raw)
    if not isinstance(value, dict) or set(value) != {
        "schema_version",
        "status",
        "errors",
    }:
        raise ValueError("diagnostic envelope fields are invalid")
    if (
        value.get("schema_version") != DIAGNOSTIC_SCHEMA_VERSION
        or value.get("status") != "error"
    ):
        raise ValueError("diagnostic envelope header is invalid")
    errors = value.get("errors")
    if not isinstance(errors, list) or not errors:
        raise ValueError("diagnostic envelope requires errors")
    return tuple(TaskStopDiagnostic.from_dict(error) for error in errors)


def extract_diagnostic_envelopes(
    raw: str,
) -> tuple[tuple[TaskStopDiagnostic, ...], str]:
    """Extract whole-line diagnostic envelopes without changing other output."""

    diagnostics: list[TaskStopDiagnostic] = []
    passthrough: list[str] = []
    for line in raw.splitlines(keepends=True):
        candidate = line.strip()
        if candidate.startswith("{"):
            try:
                diagnostics.extend(parse_diagnostic_envelope(candidate))
                continue
            except (ValueError, json.JSONDecodeError):
                pass
        passthrough.append(line)
    return _deduplicated(diagnostics), "".join(passthrough)


def _deduplicated(
    diagnostics: Iterable[TaskStopDiagnostic],
) -> tuple[TaskStopDiagnostic, ...]:
    return tuple(dict.fromkeys(diagnostics))


def emit_diagnostics(
    diagnostics: Iterable[TaskStopDiagnostic],
    *,
    diagnostic_format: str = "human",
    stream: TextIO | None = None,
) -> None:
    if diagnostic_format not in DIAGNOSTIC_FORMATS:
        raise ValueError("diagnostic format is invalid")
    output = sys.stderr if stream is None else stream
    errors = _deduplicated(diagnostics)
    if not errors:
        raise ValueError("at least one diagnostic is required")
    if diagnostic_format == "json":
        print(json.dumps(diagnostic_envelope(errors), sort_keys=True), file=output)
        return

    for index, diagnostic in enumerate(errors):
        if index:
            print(file=output)
        print(f"ERROR [{diagnostic.code}]", file=output)
        print(f"source: {diagnostic.source}", file=output)
        print(f"summary: {diagnostic.summary}", file=output)
        print("evidence:", file=output)
        for item in diagnostic.evidence:
            print(f"  - {item}", file=output)
        print(f"responsibility: {diagnostic.responsibility}", file=output)
        print(f"disposition: {diagnostic.disposition}", file=output)
        print(f"required_action: {diagnostic.required_action}", file=output)
        print(f"resume_when: {diagnostic.resume_when}", file=output)
        if diagnostic.retry_command is not None:
            print(f"retry_command: {diagnostic.retry_command}", file=output)


def diagnostic_format_from_argv(arguments: Sequence[str]) -> str:
    for index, argument in enumerate(arguments):
        if argument.startswith("--diagnostic-format="):
            value = argument.partition("=")[2]
            return value if value in DIAGNOSTIC_FORMATS else "human"
        if argument == "--diagnostic-format" and index + 1 < len(arguments):
            value = arguments[index + 1]
            return value if value in DIAGNOSTIC_FORMATS else "human"
    return "human"


class DiagnosticArgumentParser(argparse.ArgumentParser):
    def __init__(self, *args, diagnostic_source: str | None = None, **kwargs):
        self.diagnostic_source = diagnostic_source or str(
            kwargs.get("prog", "governed command")
        )
        super().__init__(*args, **kwargs)

    def error(self, message: str) -> None:
        diagnostic = task_stop_error(
            code="cli.invalid-arguments",
            source=self.diagnostic_source,
            summary="Command-line arguments do not match the governed interface.",
            evidence=(message, self.format_usage().strip()),
            responsibility="current-agent",
            disposition="fix-and-retry",
            required_action=(
                "Correct the invocation using --help and the owning Skill; do not "
                "bypass the gate."
            ),
            resume_when="The same governed command parses its arguments successfully.",
        ).diagnostic
        emit_diagnostics(
            (diagnostic,),
            diagnostic_format=diagnostic_format_from_argv(sys.argv[1:]),
        )
        raise SystemExit(2)


def add_diagnostic_format_argument(parser: argparse.ArgumentParser) -> None:
    parser.add_argument(
        "--diagnostic-format",
        choices=sorted(DIAGNOSTIC_FORMATS),
        default="human",
        help="render failures as a human block or JSON envelope",
    )
