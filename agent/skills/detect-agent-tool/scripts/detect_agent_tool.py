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


class DetectionError(ValueError):
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


def normalized_subject(executable: Path) -> str:
    raw = executable.name.lower()
    if raw.endswith(".exe"):
        raw = raw[:-4]
    subject = re.sub(r"[^a-z0-9]+", "-", raw).strip("-")
    if not subject or SUBJECT_RE.fullmatch(subject) is None:
        raise DetectionError("agent tool executable has no valid tool-shaped subject")
    tokens = set(subject.split("-"))
    blocked = sorted(tokens & MODEL_SUBJECT_TOKENS)
    if blocked:
        raise DetectionError(
            "agent tool executable subject contains model or runtime metadata token: "
            + ", ".join(blocked)
        )
    return subject


def resolve_executable(candidate: str, environment: Mapping[str, str]) -> Path:
    if not candidate.strip():
        raise DetectionError("agent tool executable declaration is empty")
    if "/" in candidate:
        resolved = Path(os.path.abspath(Path(candidate).expanduser()))
    else:
        found = shutil.which(candidate, path=environment.get("PATH", ""))
        if found is None:
            raise DetectionError(f"agent tool executable is not visible: {candidate}")
        resolved = Path(os.path.abspath(found))
    if not resolved.is_file() or not os.access(resolved, os.X_OK):
        raise DetectionError(f"agent tool path is not executable: {resolved}")
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
        raise DetectionError(f"agent tool {flag} probe failed") from error


def extract_version(result: subprocess.CompletedProcess[str]) -> str:
    if result.returncode != 0:
        raise DetectionError("agent tool --version probe returned failure")
    raw = (result.stdout + "\n" + result.stderr)[:4096]
    if MODEL_OUTPUT_RE.search(raw):
        raise DetectionError("agent tool --version output contains model metadata")
    match = VERSION_RE.search(raw)
    if match is None:
        raise DetectionError("agent tool --version output has no numeric tool version")
    return match.group(1)


def executable_sha256(executable: Path) -> str:
    digest = hashlib.sha256()
    try:
        with executable.open("rb") as stream:
            for block in iter(lambda: stream.read(1024 * 1024), b""):
                digest.update(block)
    except OSError as error:
        raise DetectionError("agent tool executable cannot be hashed") from error
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
            raise DetectionError(
                f"multiple agent tools appear in process ancestry: {names}; "
                "supply an exact executable"
            )

    visible: list[Path] = []
    for name in candidate_names:
        found = shutil.which(name, path=environment.get("PATH", ""))
        if found is not None:
            path = Path(os.path.abspath(found))
            if path not in visible:
                visible.append(path)
    if not visible:
        raise DetectionError(
            "no recognized agent harness or CLI executable is visible; "
            "supply an exact executable"
        )
    if len(visible) > 1:
        names = ", ".join(path.name for path in visible)
        raise DetectionError(
            f"multiple agent tools are visible: {names}; supply an exact executable"
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
    result = argparse.ArgumentParser(
        description="Report bounded agent harness or CLI executable facts."
    )
    result.add_argument("--executable", help="exact harness or CLI executable")
    result.add_argument("--json", action="store_true", help="emit JSON")
    result.add_argument(
        "--subject", action="store_true", help="emit only the normalized subject"
    )
    return result


def main() -> int:
    arguments = parser().parse_args()
    if arguments.json and arguments.subject:
        print("error: choose either --json or --subject", file=sys.stderr)
        return 2
    try:
        info = detect_agent_tool(explicit=arguments.executable)
    except DetectionError as error:
        print(f"error: {error}", file=sys.stderr)
        return 2
    if arguments.subject:
        print(info.subject)
    else:
        print(json.dumps(asdict(info), sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
