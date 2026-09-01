#!/usr/bin/env python3
"""Detect bounded harness or CLI executable facts without model metadata."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
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


TOOL_EXECUTABLE_ENV = "METAFLUX_AGENT_TOOL_EXECUTABLE"
DEFAULT_CANDIDATES = (
    "codex",
    "claude",
    "claude-code",
    "gemini",
    "opencode",
    "aider",
    "cursor-agent",
    "copilot",
)
SUBJECT_RE = re.compile(r"[a-z0-9]+(?:-[a-z0-9]+)*")
VERSION_RE = re.compile(
    r"(?<![A-Za-z0-9])v?(\d+(?:\.\d+){1,3}(?:[-+][0-9A-Za-z.-]+)?)"
)
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
MODEL_OUTPUT_RE = re.compile(
    r"(?:^|\s)(?:model|model_id|model-name|backend|template|session|thread)\s*[:=]",
    re.IGNORECASE,
)


class DetectionError(DiagnosticError):
    """Raised when executable evidence is absent, ambiguous, or contaminated."""


@dataclass(frozen=True)
class AgentToolInfo:
    schema_version: int
    subject: str
    interface: str
    executable: str
    version: str
    source: str
    version_probe: str
    help_available: bool
    executable_sha256: str


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


EXACT_EXECUTABLE_ACTION = (
    "Supply the absolute harness or CLI executable through --executable or the "
    "launcher declaration; do not choose by PATH order or use model metadata."
)
EXACT_EXECUTABLE_RESUME = (
    "The exact executable resolves, passes bounded version/help probes, and emits "
    "only executable-tool facts."
)


def normalized_subject(executable: Path) -> str:
    raw = executable.name.lower()
    if raw.endswith(".exe"):
        raw = raw[:-4]
    subject = re.sub(r"[^a-z0-9]+", "-", raw).strip("-")
    if not subject or SUBJECT_RE.fullmatch(subject) is None:
        raise detection_error(
            code="agent-tool.invalid-subject",
            summary="The executable basename has no valid tool-shaped subject.",
            evidence=(f"executable: {executable}",),
            required_action=EXACT_EXECUTABLE_ACTION,
            resume_when=EXACT_EXECUTABLE_RESUME,
        )
    tokens = set(subject.split("-"))
    blocked = sorted(tokens & MODEL_SUBJECT_TOKENS)
    if blocked:
        raise detection_error(
            code="agent-tool.prohibited-subject-input",
            summary=(
                "The executable basename contains prohibited model or runtime "
                "metadata."
            ),
            evidence=("resolved executable basename is not valid tool evidence",),
            required_action=EXACT_EXECUTABLE_ACTION,
            resume_when=EXACT_EXECUTABLE_RESUME,
        )
    return subject


def resolve_executable(candidate: str, environment: Mapping[str, str]) -> Path:
    if not candidate.strip():
        raise detection_error(
            code="agent-tool.empty-declaration",
            summary="The agent-tool executable declaration is empty.",
            evidence=("declaration contains no executable",),
            required_action=EXACT_EXECUTABLE_ACTION,
            resume_when=EXACT_EXECUTABLE_RESUME,
        )
    if "/" in candidate:
        resolved = Path(os.path.abspath(Path(candidate).expanduser()))
    else:
        found = shutil.which(candidate, path=environment.get("PATH", ""))
        if found is None:
            raise detection_error(
                code="agent-tool.not-visible",
                summary="The declared agent-tool executable is not visible.",
                evidence=(f"declaration: {candidate}",),
                required_action=EXACT_EXECUTABLE_ACTION,
                resume_when=EXACT_EXECUTABLE_RESUME,
            )
        resolved = Path(os.path.abspath(found))
    if not resolved.is_file() or not os.access(resolved, os.X_OK):
        raise detection_error(
            code="agent-tool.not-executable",
            summary="The declared agent-tool path is not an executable file.",
            evidence=(f"resolved path: {resolved}",),
            required_action=EXACT_EXECUTABLE_ACTION,
            resume_when=EXACT_EXECUTABLE_RESUME,
        )
    normalized_subject(resolved)
    return resolved


def process_ancestor_candidates() -> list[Path]:
    candidates: list[Path] = []
    pid = os.getppid()
    visited: set[int] = set()
    while pid > 1 and pid not in visited:
        visited.add(pid)
        proc = Path("/proc") / str(pid)
        try:
            executable = (proc / "exe").resolve(strict=True)
            stat_text = (proc / "stat").read_text(encoding="utf-8")
            stat_tail = stat_text[stat_text.rindex(")") + 2 :].split()
            parent = int(stat_tail[1])
        except (OSError, UnicodeDecodeError, ValueError, IndexError):
            break
        if executable.name.lower() in DEFAULT_CANDIDATES:
            candidates.append(executable)
        pid = parent
    return candidates


def run_probe(
    executable: Path, flag: str, environment: Mapping[str, str]
) -> subprocess.CompletedProcess[str]:
    try:
        return subprocess.run(
            [str(executable), flag],
            check=False,
            capture_output=True,
            text=True,
            timeout=5,
            env=dict(environment),
        )
    except (OSError, subprocess.TimeoutExpired) as error:
        raise detection_error(
            code="agent-tool.probe-failed",
            summary=f"The bounded agent-tool {flag} probe did not complete.",
            evidence=(f"executable: {executable}", f"failure: {type(error).__name__}"),
            required_action=EXACT_EXECUTABLE_ACTION,
            resume_when=EXACT_EXECUTABLE_RESUME,
        ) from error


def extract_version(result: subprocess.CompletedProcess[str]) -> str:
    if result.returncode != 0:
        raise detection_error(
            code="agent-tool.version-probe-rejected",
            summary="The agent-tool --version probe returned failure.",
            evidence=(f"return code: {result.returncode}",),
            required_action=EXACT_EXECUTABLE_ACTION,
            resume_when=EXACT_EXECUTABLE_RESUME,
        )
    raw = (result.stdout + "\n" + result.stderr)[:4096]
    if MODEL_OUTPUT_RE.search(raw):
        raise detection_error(
            code="agent-tool.version-output-contaminated",
            summary="The agent-tool --version output contains prohibited metadata.",
            evidence=("raw version output was discarded",),
            required_action=EXACT_EXECUTABLE_ACTION,
            resume_when=EXACT_EXECUTABLE_RESUME,
        )
    match = VERSION_RE.search(raw)
    if match is None:
        raise detection_error(
            code="agent-tool.version-missing",
            summary="The agent-tool --version output has no numeric tool version.",
            evidence=("raw version output was discarded",),
            required_action=EXACT_EXECUTABLE_ACTION,
            resume_when=EXACT_EXECUTABLE_RESUME,
        )
    return match.group(1)


def executable_sha256(executable: Path) -> str:
    digest = hashlib.sha256()
    try:
        with executable.open("rb") as stream:
            for block in iter(lambda: stream.read(1024 * 1024), b""):
                digest.update(block)
    except OSError as error:
        raise detection_error(
            code="agent-tool.digest-failed",
            summary="The resolved agent-tool executable cannot be hashed.",
            evidence=(f"executable: {executable}", f"failure: {type(error).__name__}"),
            required_action=EXACT_EXECUTABLE_ACTION,
            resume_when=EXACT_EXECUTABLE_RESUME,
        ) from error
    return digest.hexdigest()


def choose_executable(
    *,
    explicit: str | None,
    environment: Mapping[str, str],
    candidate_names: Sequence[str],
    inspect_ancestors: bool,
) -> tuple[Path, str]:
    if explicit is not None:
        return resolve_executable(explicit, environment), "explicit"

    declared = environment.get(TOOL_EXECUTABLE_ENV)
    if declared is not None:
        return resolve_executable(declared, environment), "launcher-environment"

    if inspect_ancestors:
        ancestors = list(dict.fromkeys(process_ancestor_candidates()))
        if len(ancestors) == 1:
            return resolve_executable(str(ancestors[0]), environment), "process-ancestor"
        if len(ancestors) > 1:
            names = ", ".join(path.name for path in ancestors)
            raise detection_error(
                code="agent-tool.ambiguous-ancestry",
                summary="Multiple agent tools appear in process ancestry.",
                evidence=(f"candidates: {names}",),
                required_action=EXACT_EXECUTABLE_ACTION,
                resume_when=EXACT_EXECUTABLE_RESUME,
            )

    visible: list[Path] = []
    for name in candidate_names:
        found = shutil.which(name, path=environment.get("PATH", ""))
        if found is not None:
            path = Path(os.path.abspath(found))
            if path not in visible:
                visible.append(path)
    if not visible:
        raise detection_error(
            code="agent-tool.not-found",
            summary="No recognized agent harness or CLI executable is visible.",
            evidence=("bounded candidate search returned no executable",),
            required_action=EXACT_EXECUTABLE_ACTION,
            resume_when=EXACT_EXECUTABLE_RESUME,
        )
    if len(visible) > 1:
        names = ", ".join(path.name for path in visible)
        raise detection_error(
            code="agent-tool.ambiguous-visible-tools",
            summary="Multiple agent tools are visible in the Nix environment.",
            evidence=(f"candidates: {names}",),
            required_action=EXACT_EXECUTABLE_ACTION,
            resume_when=EXACT_EXECUTABLE_RESUME,
        )
    return resolve_executable(str(visible[0]), environment), "path-singleton"


def detect_agent_tool(
    explicit: str | None = None,
    environment: Mapping[str, str] | None = None,
    candidate_names: Sequence[str] = DEFAULT_CANDIDATES,
    inspect_ancestors: bool = True,
) -> AgentToolInfo:
    values = os.environ if environment is None else environment
    executable, source = choose_executable(
        explicit=explicit,
        environment=values,
        candidate_names=candidate_names,
        inspect_ancestors=inspect_ancestors,
    )
    subject = normalized_subject(executable)
    version = extract_version(run_probe(executable, "--version", values))
    help_result = run_probe(executable, "--help", values)
    return AgentToolInfo(
        schema_version=1,
        subject=subject,
        interface="cli",
        executable=str(executable),
        version=version,
        source=source,
        version_probe="--version",
        help_available=help_result.returncode == 0,
        executable_sha256=executable_sha256(executable),
    )


def parser() -> argparse.ArgumentParser:
    result = DiagnosticArgumentParser(
        description="Report bounded agent harness or CLI executable facts.",
        diagnostic_source="detect-agent-tool",
    )
    result.add_argument("--executable", help="exact harness or CLI executable")
    result.add_argument("--json", action="store_true", help="emit JSON")
    result.add_argument(
        "--subject", action="store_true", help="emit only the normalized subject"
    )
    add_diagnostic_format_argument(result)
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
        info = detect_agent_tool(explicit=arguments.executable)
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
