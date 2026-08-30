#!/usr/bin/env python3
"""Manage transient expert-guidance packets for an active project session."""

from __future__ import annotations

import argparse
import fcntl
import hashlib
import json
import os
import re
import sys
from contextlib import contextmanager
from datetime import date, datetime
from pathlib import Path, PurePosixPath
from typing import Iterator


SESSION_ID_RE = re.compile(
    r"^S(?P<delivery>\d{4,})-(?P<date>\d{8})-"
    r"(?P<sequence>\d{3})-[a-z0-9]+(?:-[a-z0-9]+)*$"
)
DELIVERY_COORDINATE_RE = re.compile(
    r"^(0|[1-9]\d*)\.(0|[1-9]\d*)\."
    r"(0|[1-9]\d*)\.(0|[1-9]\d*)$"
)
GUIDANCE_ID_RE = re.compile(r"^G\d{3}$")
SLUG_RE = re.compile(r"^[a-z0-9]+(?:-[a-z0-9]+)*$")
PACKET_RE = re.compile(
    r"^(?P<id>G\d{3})-(?P<slug>[a-z0-9]+(?:-[a-z0-9]+)*)"
    r"\.(?P<state>draft|ready|processing)\.md$"
)
PATCH_RE = re.compile(
    r"^(?P<id>G\d{3})-(?P<slug>[a-z0-9]+(?:-[a-z0-9]+)*)\.patch$"
)
TEMP_PACKET_RE = re.compile(
    r"^\.(?P<id>G\d{3})-(?P<slug>[a-z0-9]+(?:-[a-z0-9]+)*)"
    r"\.draft\.md\.tmp$"
)
TEMP_PATCH_RE = re.compile(
    r"^\.(?P<id>G\d{3})-(?P<slug>[a-z0-9]+(?:-[a-z0-9]+)*)\.patch\.tmp$"
)
OUTPUT_REF_PATH_RE = re.compile(r"^outputs/\d{4}\.txt$")
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
ALLOWED_STATES = ("draft", "ready", "processing")
ALLOWED_DISPOSITIONS = ("adopted", "adapted", "rejected", "deferred")
ALLOWED_EVENT_TYPES = ("objective", "decision", "tool_call", "tool_result", "work_note")
MAX_INLINE_TEXT_BYTES = 65_536
REQUIRED_SECTIONS = ("Direction", "Evidence", "Constraints", "Expected Verification")
REQUIRED_PACKET_FIELDS = {
    "schema_version",
    "guidance_id",
    "target_session",
    "author",
    "expert_role",
    "created_at",
    "scope",
    "supersedes",
    "candidate_patch",
}


class GuidanceError(RuntimeError):
    """A user-correctable packet or session error."""


def _repo_default() -> Path:
    return Path(__file__).resolve().parents[4]


def _display(path: Path, repo: Path) -> str:
    try:
        return path.relative_to(repo).as_posix()
    except ValueError:
        return str(path)


def _json_string(value: str) -> str:
    return json.dumps(value, ensure_ascii=False)


def _load_json(path: Path) -> object:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError as exc:
        raise GuidanceError(f"missing required file: {path}") from exc
    except json.JSONDecodeError as exc:
        raise GuidanceError(f"invalid JSON in {path}: {exc.msg}") from exc
    except UnicodeDecodeError as exc:
        raise GuidanceError(f"invalid UTF-8 in {path}: {exc}") from exc


def _session_json_path(session_dir: Path) -> Path:
    path = session_dir / "session.json"
    if path.is_symlink() or not path.is_file():
        raise GuidanceError(f"session.json must be a regular non-symlink file: {path}")
    try:
        path.resolve().relative_to(session_dir.resolve())
    except ValueError as exc:
        raise GuidanceError(f"session.json escapes its session directory: {path}") from exc
    return path


def _validate_session_identity(
    session_dir: Path,
    document: dict[str, object],
) -> None:
    if document.get("id") != session_dir.name:
        raise GuidanceError("session.json id does not match its directory")
    match = SESSION_ID_RE.fullmatch(session_dir.name)
    if match is None:
        raise GuidanceError(f"session path has an unsupported shape: {session_dir}")
    delivery = document.get("delivery")
    if not isinstance(delivery, str) or not DELIVERY_COORDINATE_RE.fullmatch(delivery):
        raise GuidanceError(
            "session.json delivery must contain four canonical non-negative decimal components"
        )
    if "".join(delivery.split(".")) != match.group("delivery"):
        raise GuidanceError("session.json delivery does not match its session ID")


def _session_relative_file(session_dir: Path, value: object, label: str) -> Path:
    if not _nonempty_string(value):
        raise GuidanceError(f"{label} must be a non-empty relative path")
    raw = str(value).strip()
    if "\\" in raw:
        raise GuidanceError(f"{label} must use POSIX separators: {raw!r}")
    pure = PurePosixPath(raw)
    if pure.is_absolute() or raw in {".", ".."} or ".." in pure.parts:
        raise GuidanceError(f"{label} is unsafe: {raw!r}")
    candidate = session_dir.joinpath(*pure.parts)
    try:
        candidate.resolve(strict=False).relative_to(session_dir.resolve())
    except ValueError as exc:
        raise GuidanceError(f"{label} escapes its session directory: {raw!r}") from exc
    if candidate.is_symlink():
        raise GuidanceError(f"{label} must not reference a symlink: {raw!r}")
    if not candidate.is_file():
        raise GuidanceError(f"{label} must reference a regular file: {raw!r}")
    return candidate


def _resolve_repo(raw: str | None) -> Path:
    repo = Path(raw).expanduser() if raw else _repo_default()
    repo = repo.resolve()
    if not (repo / "agent" / "sessions").is_dir():
        raise GuidanceError(f"repository has no agent/sessions directory: {repo}")
    return repo


def _resolve_session(repo: Path, raw: str) -> Path:
    sessions_root = (repo / "agent" / "sessions").resolve()
    supplied = Path(raw).expanduser()
    candidates: list[Path]
    if supplied.exists() or "/" in raw or os.sep in raw:
        candidate = supplied if supplied.is_absolute() else Path.cwd() / supplied
        if not candidate.exists() and not supplied.is_absolute():
            candidate = repo / supplied
        candidates = [candidate.resolve()]
    else:
        if not SESSION_ID_RE.fullmatch(raw):
            raise GuidanceError(f"invalid session ID or path: {raw!r}")
        candidates = sorted(path.resolve() for path in sessions_root.glob(f"*/*/{raw}"))

    if len(candidates) != 1:
        raise GuidanceError(f"expected one session for {raw!r}, found {len(candidates)}")
    session_dir = candidates[0]
    try:
        relative = session_dir.relative_to(sessions_root)
    except ValueError as exc:
        raise GuidanceError(f"session escapes agent/sessions: {session_dir}") from exc
    if len(relative.parts) != 3 or not SESSION_ID_RE.fullmatch(session_dir.name):
        raise GuidanceError(f"session path has an unsupported shape: {session_dir}")
    if session_dir.is_symlink() or not session_dir.is_dir():
        raise GuidanceError(f"session must be a regular directory: {session_dir}")
    _validate_active_session(session_dir)
    return session_dir


def _validate_active_session(session_dir: Path) -> dict[str, object]:
    document = _load_json(_session_json_path(session_dir))
    if not isinstance(document, dict):
        raise GuidanceError(f"session.json must contain an object: {session_dir}")
    _validate_session_identity(session_dir, document)
    if document.get("status") != "in_progress" or document.get("ended_at") is not None:
        raise GuidanceError(f"target session is not active: {session_dir.name}")
    return document


@contextmanager
def _locked_session(session_dir: Path) -> Iterator[dict[str, object]]:
    session_path = _session_json_path(session_dir)
    try:
        with session_path.open("r", encoding="utf-8") as session_file:
            fcntl.flock(session_file.fileno(), fcntl.LOCK_EX)
            try:
                document = json.load(session_file)
            except json.JSONDecodeError as exc:
                raise GuidanceError(f"invalid JSON in {session_path}: {exc.msg}") from exc
            if not isinstance(document, dict):
                raise GuidanceError(f"session.json must contain an object: {session_dir}")
            _validate_session_identity(session_dir, document)
            if document.get("status") != "in_progress" or document.get("ended_at") is not None:
                raise GuidanceError(f"target session is not active: {session_dir.name}")
            yield document
    except FileNotFoundError as exc:
        raise GuidanceError(f"missing required file: {session_path}") from exc


def _is_int(value: object) -> bool:
    return isinstance(value, int) and not isinstance(value, bool)


def _nonempty_string(value: object) -> bool:
    return isinstance(value, str) and bool(value.strip())


def _validate_event_timestamp(value: object, line_number: int) -> None:
    if value is None:
        return
    if not _nonempty_string(value):
        raise GuidanceError(
            f"events.jsonl line {line_number} timestamp must be null or a non-empty ISO-8601 string"
        )
    raw = str(value)
    try:
        datetime.fromisoformat(raw.replace("Z", "+00:00"))
        return
    except ValueError:
        pass
    try:
        date.fromisoformat(raw)
    except ValueError as exc:
        raise GuidanceError(
            f"events.jsonl line {line_number} timestamp is not ISO-8601: {raw!r}"
        ) from exc


def _validate_event_output_ref(
    session_dir: Path,
    value: object,
    line_number: int,
) -> None:
    if not isinstance(value, dict):
        raise GuidanceError(f"events.jsonl line {line_number} output_ref must be an object")
    required = {"path", "bytes", "sha256"}
    missing = required - value.keys()
    if missing:
        raise GuidanceError(
            f"events.jsonl line {line_number} output_ref missing fields: "
            f"{', '.join(sorted(missing))}"
        )
    raw_path = value.get("path")
    if not isinstance(raw_path, str) or not OUTPUT_REF_PATH_RE.fullmatch(raw_path):
        raise GuidanceError(
            f"events.jsonl line {line_number} output_ref.path must match outputs/NNNN.txt"
        )
    byte_count = value.get("bytes")
    if not _is_int(byte_count) or int(byte_count) < 0:
        raise GuidanceError(
            f"events.jsonl line {line_number} output_ref.bytes must be a non-negative integer"
        )
    digest = value.get("sha256")
    if not isinstance(digest, str) or not SHA256_RE.fullmatch(digest):
        raise GuidanceError(
            f"events.jsonl line {line_number} output_ref.sha256 must be lowercase SHA-256"
        )
    output_path = _session_relative_file(
        session_dir,
        raw_path,
        f"events.jsonl line {line_number} output_ref.path",
    )
    data = output_path.read_bytes()
    if len(data) != byte_count:
        raise GuidanceError(f"events.jsonl line {line_number} output_ref byte count mismatch")
    if hashlib.sha256(data).hexdigest() != digest:
        raise GuidanceError(f"events.jsonl line {line_number} output_ref SHA-256 mismatch")


def _session_schema_version(document: dict[str, object]) -> int:
    value = document.get("schema_version")
    if not _is_int(value) or int(value) <= 0:
        raise GuidanceError("session.json schema_version must be a positive integer")
    return int(value)


def _read_events(
    session_dir: Path,
    schema_version: int,
    declared_event_log: object,
) -> list[dict[str, object]]:
    if declared_event_log != "events.jsonl":
        raise GuidanceError("session.json event_log must be the canonical 'events.jsonl'")
    events_path = _session_relative_file(
        session_dir,
        declared_event_log,
        "session.json event_log",
    )
    try:
        lines = events_path.read_text(encoding="utf-8").splitlines()
    except FileNotFoundError as exc:
        raise GuidanceError(f"missing required file: {events_path}") from exc
    except UnicodeDecodeError as exc:
        raise GuidanceError(f"invalid UTF-8 in {events_path}: {exc}") from exc

    events: list[dict[str, object]] = []
    disposed_guidance: dict[str, int] = {}
    expected_seq = 1
    for line_number, line in enumerate(lines, start=1):
        if not line.strip():
            raise GuidanceError(f"blank events.jsonl line {line_number}")
        try:
            event = json.loads(line)
        except json.JSONDecodeError as exc:
            raise GuidanceError(
                f"invalid events.jsonl line {line_number}: {exc.msg}"
            ) from exc
        if not isinstance(event, dict):
            raise GuidanceError(f"events.jsonl line {line_number} is not an object")

        required = {"schema_version", "seq", "timestamp", "type", "actor"}
        missing = required - event.keys()
        if missing:
            raise GuidanceError(
                f"events.jsonl line {line_number} missing fields: {', '.join(sorted(missing))}"
            )
        if event.get("schema_version") != schema_version:
            raise GuidanceError(f"events.jsonl line {line_number} schema_version mismatch")
        sequence = event.get("seq")
        if not _is_int(sequence):
            raise GuidanceError(f"events.jsonl line {line_number} seq must be an integer")
        if sequence != expected_seq:
            raise GuidanceError(
                f"events.jsonl line {line_number} expected contiguous seq "
                f"{expected_seq}, got {sequence}"
            )
        expected_seq += 1
        _validate_event_timestamp(event.get("timestamp"), line_number)
        if event.get("type") not in ALLOWED_EVENT_TYPES:
            raise GuidanceError(f"events.jsonl line {line_number} has unsupported type")
        if not _nonempty_string(event.get("actor")):
            raise GuidanceError(
                f"events.jsonl line {line_number} actor must be a non-empty string"
            )

        has_guidance_id = "guidance_id" in event
        has_disposition = "disposition" in event
        has_deferred_to = "deferred_to" in event
        if has_guidance_id != has_disposition:
            raise GuidanceError(
                f"events.jsonl line {line_number} guidance_id and disposition must appear together"
            )
        if has_guidance_id:
            guidance_id = event.get("guidance_id")
            if not isinstance(guidance_id, str) or not GUIDANCE_ID_RE.fullmatch(guidance_id):
                raise GuidanceError(
                    f"events.jsonl line {line_number} guidance_id must match GNNN"
                )
            disposition = event.get("disposition")
            if disposition not in ALLOWED_DISPOSITIONS:
                raise GuidanceError(
                    f"events.jsonl line {line_number} disposition has an unsupported value"
                )
            if event.get("type") not in {"decision", "work_note"}:
                raise GuidanceError(
                    f"events.jsonl line {line_number} guidance fields require decision or work_note"
                )
            if disposition == "deferred":
                if not _nonempty_string(event.get("deferred_to")):
                    raise GuidanceError(
                        f"events.jsonl line {line_number} deferred guidance requires deferred_to"
                    )
            elif has_deferred_to:
                raise GuidanceError(
                    f"events.jsonl line {line_number} deferred_to is only valid for deferred guidance"
                )
            previous_line = disposed_guidance.get(guidance_id)
            if previous_line is not None:
                raise GuidanceError(
                    f"events.jsonl line {line_number} duplicate disposition for {guidance_id}; "
                    f"first recorded on line {previous_line}"
                )
            disposed_guidance[guidance_id] = line_number
        elif has_deferred_to:
            raise GuidanceError(
                f"events.jsonl line {line_number} deferred_to is only valid with guidance disposition"
            )

        has_content = "content" in event
        has_output_ref = "output_ref" in event
        if has_content == has_output_ref:
            raise GuidanceError(
                f"events.jsonl line {line_number} requires exactly one of content or output_ref"
            )
        if has_content:
            content = event.get("content")
            if not isinstance(content, str):
                raise GuidanceError(f"events.jsonl line {line_number} content must be a string")
            if len(content.encode("utf-8")) > MAX_INLINE_TEXT_BYTES:
                raise GuidanceError(
                    f"events.jsonl line {line_number} content exceeds {MAX_INLINE_TEXT_BYTES} bytes"
                )
        else:
            _validate_event_output_ref(session_dir, event.get("output_ref"), line_number)

        omitted = event.get("omitted", False)
        if not isinstance(omitted, bool):
            raise GuidanceError(f"events.jsonl line {line_number} omitted must be boolean")
        if omitted and not _nonempty_string(event.get("reason")):
            raise GuidanceError(
                f"events.jsonl line {line_number} omitted events require a reason"
            )
        if "reason" in event and not _nonempty_string(event.get("reason")):
            raise GuidanceError(
                f"events.jsonl line {line_number} reason must be a non-empty string"
            )
        if "redactions" in event:
            raise GuidanceError(
                f"events.jsonl line {line_number} redactions are not stored in session events"
            )
        events.append(event)
    return events


def _packet_files(guidance_dir: Path) -> list[tuple[Path, re.Match[str]]]:
    if not guidance_dir.exists():
        return []
    if guidance_dir.is_symlink() or not guidance_dir.is_dir():
        raise GuidanceError(f"guidance path must be a regular directory: {guidance_dir}")
    packets: list[tuple[Path, re.Match[str]]] = []
    for path in sorted(guidance_dir.iterdir()):
        match = PACKET_RE.fullmatch(path.name)
        if match:
            if path.is_symlink() or not path.is_file():
                raise GuidanceError(f"packet must be a regular file: {path}")
            packets.append((path, match))
    return packets


def _assert_unique_packet_ids(packets: list[tuple[Path, re.Match[str]]]) -> None:
    by_id: dict[str, list[str]] = {}
    for path, match in packets:
        by_id.setdefault(match.group("id"), []).append(path.name)
    duplicates = {key: names for key, names in by_id.items() if len(names) > 1}
    if duplicates:
        detail = "; ".join(f"{key}: {', '.join(names)}" for key, names in duplicates.items())
        raise GuidanceError(f"conflicting guidance packet IDs: {detail}")


def _creation_residues(
    guidance_dir: Path,
    packets: list[tuple[Path, re.Match[str]]],
) -> list[tuple[Path, str, str]]:
    referenced_patches: set[str] = set()
    residues: list[tuple[Path, str, str]] = []
    for path, match in packets:
        try:
            metadata, _, state, _ = _validate_packet(
                path,
                guidance_dir.parent,
                require_complete=match.group("state") != "draft",
            )
        except GuidanceError as exc:
            if match.group("state") == "draft":
                command = (
                    "guidance.py recover "
                    f"--session {guidance_dir.parent.name} "
                    f"--guidance-id {match.group('id')} --slug {match.group('slug')} "
                    "--reason REASON"
                )
                raise GuidanceError(
                    f"malformed draft {path.name}; inspect it, then run {command}: {exc}"
                ) from exc
            raise
        candidate_patch = metadata["candidate_patch"]
        if isinstance(candidate_patch, str):
            referenced_patches.add(candidate_patch)

    for path in sorted(guidance_dir.iterdir()):
        temp_match = TEMP_PACKET_RE.fullmatch(path.name) or TEMP_PATCH_RE.fullmatch(path.name)
        if temp_match:
            residues.append((path, temp_match.group("id"), temp_match.group("slug")))
            continue
        patch_match = PATCH_RE.fullmatch(path.name)
        if patch_match and path.name not in referenced_patches:
            residues.append((path, patch_match.group("id"), patch_match.group("slug")))
    return residues


def _next_guidance_id(
    guidance_dir: Path,
    packets: list[tuple[Path, re.Match[str]]],
    events: list[dict[str, object]],
) -> str:
    numbers = [int(match.group("id")[1:]) for _, match in packets]
    if guidance_dir.is_dir():
        for path in guidance_dir.iterdir():
            for pattern in (PATCH_RE, TEMP_PACKET_RE, TEMP_PATCH_RE):
                match = pattern.fullmatch(path.name)
                if match:
                    numbers.append(int(match.group("id")[1:]))
                    break
    for event in events:
        guidance_id = event.get("guidance_id")
        if isinstance(guidance_id, str) and GUIDANCE_ID_RE.fullmatch(guidance_id):
            numbers.append(int(guidance_id[1:]))
    number = max(numbers, default=0) + 1
    if number > 999:
        raise GuidanceError("session exhausted the GNNN guidance ID space")
    return f"G{number:03d}"


def _parse_packet(path: Path) -> tuple[dict[str, object], str, str, str]:
    match = PACKET_RE.fullmatch(path.name)
    if not match:
        raise GuidanceError(
            "packet name must match GNNN-slug.(draft|ready|processing).md"
        )
    try:
        text = path.read_text(encoding="utf-8")
    except FileNotFoundError as exc:
        raise GuidanceError(f"packet does not exist: {path}") from exc
    except UnicodeDecodeError as exc:
        raise GuidanceError(f"packet is not valid UTF-8: {path}: {exc}") from exc
    lines = text.splitlines()
    if not lines or lines[0] != "---":
        raise GuidanceError(f"packet is missing frontmatter: {path}")
    try:
        closing = lines.index("---", 1)
    except ValueError as exc:
        raise GuidanceError(f"packet frontmatter is unterminated: {path}") from exc

    metadata: dict[str, object] = {}
    for line_number, line in enumerate(lines[1:closing], start=2):
        field_match = re.fullmatch(r"([a-z][a-z0-9_]*):[ ]*(.+)", line)
        if not field_match:
            raise GuidanceError(f"invalid packet frontmatter line {line_number}: {path}")
        key, raw_value = field_match.groups()
        if key in metadata:
            raise GuidanceError(f"duplicate packet field {key!r}: {path}")
        try:
            metadata[key] = json.loads(raw_value)
        except json.JSONDecodeError as exc:
            raise GuidanceError(
                f"invalid JSON value for packet field {key!r}: {exc.msg}"
            ) from exc
    body = "\n".join(lines[closing + 1 :]) + "\n"
    return metadata, body, match.group("state"), match.group("slug")


def _section_content(body: str, heading: str) -> str | None:
    pattern = re.compile(
        rf"^## {re.escape(heading)}[ \t]*\n(?P<body>.*?)(?=^## |\Z)",
        re.MULTILINE | re.DOTALL,
    )
    match = pattern.search(body)
    if not match:
        return None
    content = re.sub(r"<!--.*?-->", "", match.group("body"), flags=re.DOTALL)
    return content.strip()


def _validate_packet(
    path: Path,
    session_dir: Path,
    *,
    require_complete: bool,
    require_attachment: bool = True,
) -> tuple[dict[str, object], str, str, str]:
    metadata, body, state, slug = _parse_packet(path)
    missing = REQUIRED_PACKET_FIELDS - metadata.keys()
    extra = metadata.keys() - REQUIRED_PACKET_FIELDS
    if missing:
        raise GuidanceError(f"packet is missing fields: {', '.join(sorted(missing))}")
    if extra:
        raise GuidanceError(f"packet has unsupported fields: {', '.join(sorted(extra))}")
    if metadata["schema_version"] != 1:
        raise GuidanceError("packet schema_version must be 1")
    guidance_id = metadata["guidance_id"]
    if guidance_id != PACKET_RE.fullmatch(path.name).group("id"):
        raise GuidanceError("packet guidance_id does not match its filename")
    if metadata["target_session"] != session_dir.name:
        raise GuidanceError("packet target_session does not match its session directory")
    for field in ("author", "expert_role", "created_at", "scope"):
        if not isinstance(metadata[field], str) or not metadata[field].strip():
            raise GuidanceError(f"packet field {field!r} must be a non-empty string")
    try:
        created_at = datetime.fromisoformat(str(metadata["created_at"]))
    except ValueError as exc:
        raise GuidanceError("packet created_at must be an ISO 8601 timestamp") from exc
    if created_at.tzinfo is None or created_at.utcoffset() is None:
        raise GuidanceError("packet created_at must include a UTC offset")
    supersedes = metadata["supersedes"]
    if not isinstance(supersedes, list) or any(
        not isinstance(item, str) or not GUIDANCE_ID_RE.fullmatch(item)
        for item in supersedes
    ):
        raise GuidanceError("packet supersedes must be an array of GNNN IDs")
    if len(set(supersedes)) != len(supersedes) or guidance_id in supersedes:
        raise GuidanceError("packet supersedes must contain unique earlier IDs")
    current_number = int(str(guidance_id)[1:])
    if any(int(item[1:]) >= current_number for item in supersedes):
        raise GuidanceError("packet supersedes must contain only lower-numbered IDs")

    expected_patch = f"{guidance_id}-{slug}.patch"
    candidate_patch = metadata["candidate_patch"]
    if candidate_patch is not None and candidate_patch != expected_patch:
        raise GuidanceError(f"candidate_patch must be null or {expected_patch!r}")
    if candidate_patch is not None and require_attachment:
        patch_path = path.parent / candidate_patch
        if patch_path.is_symlink() or not patch_path.is_file():
            raise GuidanceError(f"candidate patch is missing or not a regular file: {patch_path}")

    if require_complete:
        for heading in REQUIRED_SECTIONS:
            content = _section_content(body, heading)
            if not content:
                raise GuidanceError(f"packet section {heading!r} must be completed")
    return metadata, body, state, slug


def _resolve_packet_path(repo: Path, raw: str) -> tuple[Path, Path]:
    supplied = Path(raw).expanduser()
    candidate = supplied if supplied.is_absolute() else Path.cwd() / supplied
    if not candidate.exists() and not supplied.is_absolute():
        candidate = repo / supplied
    if candidate.is_symlink():
        raise GuidanceError(f"packet must not be a symlink: {candidate}")
    path = candidate.resolve()
    if path.parent.name != "guidance":
        raise GuidanceError(f"packet is not under a session guidance directory: {path}")
    session_dir = path.parent.parent
    resolved_session = _resolve_session(repo, str(session_dir))
    if resolved_session != session_dir.resolve():
        raise GuidanceError("packet session path changed during resolution")
    return path, session_dir


def _fsync_directory(path: Path) -> None:
    descriptor = os.open(path, os.O_RDONLY | getattr(os, "O_DIRECTORY", 0))
    try:
        os.fsync(descriptor)
    finally:
        os.close(descriptor)


def _transition(path: Path, state: str) -> Path:
    match = PACKET_RE.fullmatch(path.name)
    if not match:
        raise GuidanceError(f"invalid packet name: {path.name}")
    destination = path.with_name(
        f"{match.group('id')}-{match.group('slug')}.{state}.md"
    )
    if destination.exists():
        raise GuidanceError(f"transition destination already exists: {destination}")
    os.rename(path, destination)
    _fsync_directory(destination.parent)
    return destination


def _render_packet(arguments: argparse.Namespace, guidance_id: str, patch_name: str | None) -> str:
    template_path = Path(__file__).resolve().parents[1] / "assets" / "guidance-packet.md"
    template = template_path.read_text(encoding="utf-8")
    title = arguments.title or " ".join(word.capitalize() for word in arguments.slug.split("-"))
    values = {
        "SCHEMA_VERSION": "1",
        "GUIDANCE_ID": _json_string(guidance_id),
        "TARGET_SESSION": _json_string(arguments.session_id),
        "AUTHOR": _json_string(arguments.author),
        "EXPERT_ROLE": _json_string(arguments.role),
        "CREATED_AT": _json_string(datetime.now().astimezone().isoformat(timespec="seconds")),
        "SCOPE": _json_string(arguments.scope),
        "SUPERSEDES": json.dumps(arguments.supersedes, ensure_ascii=False),
        "CANDIDATE_PATCH": "null" if patch_name is None else _json_string(patch_name),
        "TITLE": title,
        "DIRECTION": arguments.direction
        or "<!-- State the recommended direction and the boundary it changes. -->",
        "EVIDENCE": arguments.evidence
        or "<!-- Link current source, tests, measurements, or a reproducible observation. -->",
        "CONSTRAINTS": arguments.constraints
        or "<!-- Name the canonical constraints and compatibility limits that still apply. -->",
        "PATCH_NOTE": (
            f"See [candidate patch]({patch_name})."
            if patch_name
            else "<!-- Optional: explain an inline candidate change; no patch is attached. -->"
        ),
        "VERIFICATION": arguments.verification
        or "<!-- State the command or observable result that would validate this direction. -->",
    }
    expected_placeholders = {"{{" + key + "}}" for key in values}
    actual_placeholders = set(re.findall(r"\{\{[A-Z_]+\}\}", template))
    if actual_placeholders != expected_placeholders:
        missing = sorted(expected_placeholders - actual_placeholders)
        extra = sorted(actual_placeholders - expected_placeholders)
        detail = []
        if missing:
            detail.append(f"missing {', '.join(missing)}")
        if extra:
            detail.append(f"unexpected {', '.join(extra)}")
        raise GuidanceError(f"packet template placeholder mismatch: {'; '.join(detail)}")
    for key, value in values.items():
        template = template.replace("{{" + key + "}}", value)
    return template


def _write_staged(path: Path, data: bytes) -> None:
    created = False
    try:
        with path.open("xb") as stream:
            created = True
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
    except FileExistsError as exc:
        raise GuidanceError(f"create residue already exists: {path}") from exc
    except Exception:
        if created:
            try:
                path.unlink()
            except FileNotFoundError:
                pass
        raise


def _publish_staged(staged: Path, destination: Path) -> None:
    if destination.exists():
        raise GuidanceError(f"refusing to overwrite existing file: {destination}")
    os.rename(staged, destination)


def _remove_owned_path(path: Path) -> None:
    if path.is_symlink():
        raise GuidanceError(f"refusing to remove symlink residue: {path}")
    try:
        path.unlink()
    except FileNotFoundError:
        pass


def command_create(arguments: argparse.Namespace, repo: Path) -> None:
    if not SLUG_RE.fullmatch(arguments.slug):
        raise GuidanceError("slug must be lowercase hyphenated text")
    for label in ("author", "role", "scope"):
        if not getattr(arguments, label).strip():
            raise GuidanceError(f"--{label} must be non-empty")
    if len(set(arguments.supersedes)) != len(arguments.supersedes):
        raise GuidanceError("--supersedes values must be unique")
    for guidance_id in arguments.supersedes:
        if not GUIDANCE_ID_RE.fullmatch(guidance_id):
            raise GuidanceError(f"invalid superseded guidance ID: {guidance_id!r}")
    patch_data: bytes | None = None
    if arguments.patch:
        patch_source = Path(arguments.patch).expanduser()
        if patch_source.is_symlink() or not patch_source.is_file():
            raise GuidanceError(f"candidate patch must be a regular file: {patch_source}")
        patch_data = patch_source.read_bytes()

    session_dir = _resolve_session(repo, arguments.session)
    guidance_dir = session_dir / "guidance"
    with _locked_session(session_dir) as session_document:
        schema_version = _session_schema_version(session_document)
        events = _read_events(
            session_dir,
            schema_version,
            session_document.get("event_log"),
        )
        guidance_dir_preexisted = guidance_dir.exists()
        guidance_dir.mkdir(mode=0o755, exist_ok=True)
        if not guidance_dir_preexisted:
            _fsync_directory(session_dir)
        packets = _packet_files(guidance_dir)
        _assert_unique_packet_ids(packets)
        residues = _creation_residues(guidance_dir, packets)
        if residues:
            descriptions = ", ".join(path.name for path, _, _ in residues)
            _, residue_id, residue_slug = residues[0]
            raise GuidanceError(
                f"create residue requires explicit recovery: {descriptions}; run "
                f"guidance.py recover --session {session_dir.name} "
                f"--guidance-id {residue_id} --slug {residue_slug} --reason REASON"
            )
        guidance_id = _next_guidance_id(guidance_dir, packets, events)
        packet_name = f"{guidance_id}-{arguments.slug}.draft.md"
        patch_name = f"{guidance_id}-{arguments.slug}.patch" if patch_data is not None else None
        arguments.session_id = session_dir.name
        rendered = _render_packet(arguments, guidance_id, patch_name)
        packet_path = guidance_dir / packet_name
        patch_path = guidance_dir / patch_name if patch_name else None
        packet_temp = guidance_dir / f".{guidance_id}-{arguments.slug}.draft.md.tmp"
        patch_temp = (
            guidance_dir / f".{guidance_id}-{arguments.slug}.patch.tmp"
            if patch_data is not None
            else None
        )
        staged_paths: list[Path] = []
        published_paths: list[Path] = []
        try:
            if patch_temp is not None and patch_data is not None:
                _write_staged(patch_temp, patch_data)
                staged_paths.append(patch_temp)
            _write_staged(packet_temp, rendered.encode("utf-8"))
            staged_paths.append(packet_temp)
            _fsync_directory(guidance_dir)

            if patch_temp is not None and patch_path is not None:
                _publish_staged(patch_temp, patch_path)
                staged_paths.remove(patch_temp)
                published_paths.append(patch_path)
                _fsync_directory(guidance_dir)
            _publish_staged(packet_temp, packet_path)
            staged_paths.remove(packet_temp)
            published_paths.append(packet_path)
            _fsync_directory(guidance_dir)
        except Exception:
            cleanup_errors: list[str] = []
            for owned_path in reversed(staged_paths + published_paths):
                try:
                    _remove_owned_path(owned_path)
                except (GuidanceError, OSError) as exc:
                    cleanup_errors.append(str(exc))
            if cleanup_errors:
                raise GuidanceError(
                    "create failed and left exact recovery residue: "
                    + "; ".join(cleanup_errors)
                )
            _fsync_directory(guidance_dir)
            raise
    print(_display(packet_path, repo))


def _matching_dispositions(
    events: list[dict[str, object]],
    guidance_id: str,
) -> list[tuple[str, dict[str, object]]]:
    matches: list[tuple[str, dict[str, object]]] = []
    for event in events:
        if event.get("guidance_id") != guidance_id:
            continue
        disposition = event.get("disposition")
        if disposition in ALLOWED_DISPOSITIONS:
            matches.append((str(disposition), event))
    return matches


def _assert_no_material_disposition(
    events: list[dict[str, object]],
    guidance_id: str,
    operation: str,
) -> None:
    matches = _matching_dispositions(events, guidance_id)
    if matches:
        disposition, _ = matches[0]
        raise GuidanceError(
            f"{operation} is invalid because {guidance_id} already has material "
            f"disposition {disposition!r}"
        )


def _recovery_paths(
    guidance_dir: Path,
    guidance_id: str,
    slug: str,
) -> dict[str, Path]:
    return {
        "packet_temp": guidance_dir / f".{guidance_id}-{slug}.draft.md.tmp",
        "patch_temp": guidance_dir / f".{guidance_id}-{slug}.patch.tmp",
        "draft": guidance_dir / f"{guidance_id}-{slug}.draft.md",
        "patch": guidance_dir / f"{guidance_id}-{slug}.patch",
    }


def command_recover(arguments: argparse.Namespace, repo: Path) -> None:
    if not GUIDANCE_ID_RE.fullmatch(arguments.guidance_id):
        raise GuidanceError("--guidance-id must match GNNN")
    if not SLUG_RE.fullmatch(arguments.slug):
        raise GuidanceError("--slug must be lowercase hyphenated text")
    if not arguments.reason.strip():
        raise GuidanceError("--reason must be non-empty")

    session_dir = _resolve_session(repo, arguments.session)
    guidance_dir = session_dir / "guidance"
    with _locked_session(session_dir) as session_document:
        events = _read_events(
            session_dir,
            _session_schema_version(session_document),
            session_document.get("event_log"),
        )
        _assert_no_material_disposition(
            events,
            arguments.guidance_id,
            "create recovery",
        )
        if guidance_dir.is_symlink() or not guidance_dir.is_dir():
            raise GuidanceError(f"guidance directory does not exist: {guidance_dir}")

        target_paths = _recovery_paths(
            guidance_dir,
            arguments.guidance_id,
            arguments.slug,
        )
        for path in guidance_dir.iterdir():
            match = (
                PACKET_RE.fullmatch(path.name)
                or PATCH_RE.fullmatch(path.name)
                or TEMP_PACKET_RE.fullmatch(path.name)
                or TEMP_PATCH_RE.fullmatch(path.name)
            )
            if not match or match.group("id") != arguments.guidance_id:
                continue
            if match.group("slug") != arguments.slug:
                raise GuidanceError(
                    f"guidance ID {arguments.guidance_id} also belongs to {path.name}; "
                    "recover the identity conflict manually"
                )
            packet_match = PACKET_RE.fullmatch(path.name)
            if packet_match and packet_match.group("state") != "draft":
                raise GuidanceError(
                    f"recovery refuses published or claimed packet: {path.name}"
                )

        existing = {name: path for name, path in target_paths.items() if path.exists()}
        if not existing:
            raise GuidanceError(
                f"no exact create residue for {arguments.guidance_id}-{arguments.slug}"
            )
        for path in existing.values():
            if path.is_symlink() or not path.is_file():
                raise GuidanceError(f"recovery target must be a regular file: {path}")

        draft = target_paths["draft"]
        valid_draft_metadata: dict[str, object] | None = None
        if draft.exists():
            try:
                valid_draft_metadata, _, _, _ = _validate_packet(
                    draft,
                    session_dir,
                    require_complete=False,
                    require_attachment=False,
                )
            except GuidanceError:
                valid_draft_metadata = None

        removable = [
            target_paths["packet_temp"],
            target_paths["patch_temp"],
        ]
        if valid_draft_metadata is None:
            removable.extend((target_paths["patch"], draft))
        else:
            candidate_patch = valid_draft_metadata["candidate_patch"]
            if candidate_patch != target_paths["patch"].name:
                removable.append(target_paths["patch"])

        removable = [path for path in removable if path.exists()]
        if not removable:
            raise GuidanceError(
                f"{draft.name} is a structurally valid draft; use resolve --no-material "
                "for an intentional discard"
            )
        for path in removable:
            _remove_owned_path(path)
        _fsync_directory(guidance_dir)
        try:
            guidance_dir.rmdir()
        except OSError:
            pass
    removed = ", ".join(path.name for path in removable)
    print(
        f"recovered {arguments.guidance_id}-{arguments.slug}: removed {removed}; "
        f"reason: {arguments.reason.strip()}"
    )


def command_publish(arguments: argparse.Namespace, repo: Path) -> None:
    path, session_dir = _resolve_packet_path(repo, arguments.packet)
    with _locked_session(session_dir):
        packets = _packet_files(path.parent)
        _assert_unique_packet_ids(packets)
        _, _, state, _ = _validate_packet(path, session_dir, require_complete=True)
        if state != "draft":
            raise GuidanceError("publish requires a draft packet")
        destination = _transition(path, "ready")
    print(_display(destination, repo))


def command_list(arguments: argparse.Namespace, repo: Path) -> None:
    session_dir = _resolve_session(repo, arguments.session)
    with _locked_session(session_dir):
        packets = _packet_files(session_dir / "guidance")
        _assert_unique_packet_ids(packets)
        entries: list[dict[str, object]] = []
        for path, match in packets:
            state = match.group("state")
            if arguments.state != "all" and state != arguments.state:
                continue
            metadata, _, _, _ = _validate_packet(
                path,
                session_dir,
                require_complete=state != "draft",
            )
            entries.append(
                {
                    "guidance_id": metadata["guidance_id"],
                    "state": state,
                    "path": _display(path, repo),
                    "author": metadata["author"],
                    "expert_role": metadata["expert_role"],
                    "scope": metadata["scope"],
                    "supersedes": metadata["supersedes"],
                    "candidate_patch": metadata["candidate_patch"],
                }
            )
    if arguments.json:
        print(json.dumps(entries, indent=2, ensure_ascii=False))
        return
    for entry in entries:
        print(
            f"{entry['guidance_id']}\t{entry['state']}\t{entry['path']}\t"
            f"{entry['expert_role']}\t{entry['scope']}"
        )


def command_claim(arguments: argparse.Namespace, repo: Path) -> None:
    path, session_dir = _resolve_packet_path(repo, arguments.packet)
    with _locked_session(session_dir):
        packets = _packet_files(path.parent)
        _assert_unique_packet_ids(packets)
        _, _, state, _ = _validate_packet(path, session_dir, require_complete=True)
        if state != "ready":
            raise GuidanceError("claim requires a ready packet")
        destination = _transition(path, "processing")
    print(_display(destination, repo))


def _remove_packet(path: Path, metadata: dict[str, object]) -> None:
    candidate_patch = metadata["candidate_patch"]
    if isinstance(candidate_patch, str):
        patch_path = path.parent / candidate_patch
        if patch_path.is_symlink():
            raise GuidanceError(f"refusing to remove symlink candidate patch: {patch_path}")
        try:
            patch_path.unlink()
        except FileNotFoundError:
            pass
    path.unlink()
    _fsync_directory(path.parent)
    try:
        path.parent.rmdir()
    except OSError:
        pass


def command_resolve(arguments: argparse.Namespace, repo: Path) -> None:
    path, session_dir = _resolve_packet_path(repo, arguments.packet)
    with _locked_session(session_dir) as session_document:
        events = _read_events(
            session_dir,
            _session_schema_version(session_document),
            session_document.get("event_log"),
        )
        packets = _packet_files(path.parent)
        _assert_unique_packet_ids(packets)
        metadata, _, state, _ = _validate_packet(
            path,
            session_dir,
            require_complete=state_from_name(path.name) != "draft",
            require_attachment=False,
        )
        guidance_id = str(metadata["guidance_id"])
        if arguments.no_material:
            if not arguments.reason or not arguments.reason.strip():
                raise GuidanceError("--no-material requires a non-empty --reason")
            _assert_no_material_disposition(events, guidance_id, "no-material discard")
            _remove_packet(path, metadata)
            print(f"discarded {guidance_id} with no material outcome: {arguments.reason.strip()}")
            return

        if state != "processing":
            raise GuidanceError("material resolution requires a processing packet")
        matches = _matching_dispositions(events, guidance_id)
        if len(matches) > 1:
            raise GuidanceError(
                f"multiple disposition events for {guidance_id}; expected exactly one"
            )
        exact = [event for disposition, event in matches if disposition == arguments.disposition]
        if not exact:
            raise GuidanceError(
                f"events.jsonl has no {guidance_id!r} disposition {arguments.disposition!r}"
            )
        if arguments.disposition == "deferred" and not any(
            isinstance(event.get("deferred_to"), str) and event["deferred_to"].strip()
            for event in exact
        ):
            raise GuidanceError("deferred guidance requires a non-empty event deferred_to")
        _remove_packet(path, metadata)
    print(f"resolved {guidance_id} as {arguments.disposition}")


def state_from_name(name: str) -> str:
    match = PACKET_RE.fullmatch(name)
    if not match:
        raise GuidanceError(f"invalid packet name: {name}")
    return match.group("state")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Manage session-local expert-guidance packets.",
    )
    parser.add_argument(
        "--repo",
        help="repository root (defaults to the root containing this skill)",
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    create = subparsers.add_parser("create", help="create the next draft packet")
    create.add_argument("--session", required=True, help="active session ID or directory")
    create.add_argument("--slug", required=True, help="lowercase hyphenated packet slug")
    create.add_argument("--author", required=True, help="guidance author identifier")
    create.add_argument("--role", required=True, help="author's expert role")
    create.add_argument("--scope", required=True, help="bounded affected area")
    create.add_argument("--title", help="packet title (defaults to the slug title)")
    create.add_argument(
        "--supersedes",
        action="append",
        default=[],
        metavar="GNNN",
        help="earlier guidance ID replaced by this packet; repeat as needed",
    )
    create.add_argument("--patch", help="candidate patch copied beside the packet")
    create.add_argument("--direction", help="recommended direction")
    create.add_argument("--evidence", help="supporting evidence")
    create.add_argument("--constraints", help="constraints that remain binding")
    create.add_argument("--verification", help="expected verification")
    create.set_defaults(handler=command_create)

    recover = subparsers.add_parser(
        "recover",
        help="remove exact interrupted-create residue or a malformed draft",
    )
    recover.add_argument("--session", required=True, help="active session ID or directory")
    recover.add_argument("--guidance-id", required=True, help="exact GNNN identity")
    recover.add_argument("--slug", required=True, help="exact packet slug")
    recover.add_argument("--reason", required=True, help="why this residue is disposable")
    recover.set_defaults(handler=command_recover)

    publish = subparsers.add_parser("publish", help="atomically publish a completed draft")
    publish.add_argument("packet", help="draft packet path")
    publish.set_defaults(handler=command_publish)

    listing = subparsers.add_parser("list", help="list packets for an active session")
    listing.add_argument("--session", required=True, help="active session ID or directory")
    listing.add_argument(
        "--state",
        choices=(*ALLOWED_STATES, "all"),
        default="ready",
        help="state to list (default: ready)",
    )
    listing.add_argument("--json", action="store_true", help="emit a JSON array")
    listing.set_defaults(handler=command_list)

    claim = subparsers.add_parser("claim", help="atomically claim a ready packet")
    claim.add_argument("packet", help="ready packet path")
    claim.set_defaults(handler=command_claim)

    resolve = subparsers.add_parser("resolve", help="verify disposition and remove a packet")
    resolve.add_argument("packet", help="packet path")
    resolution = resolve.add_mutually_exclusive_group(required=True)
    resolution.add_argument("--disposition", choices=ALLOWED_DISPOSITIONS)
    resolution.add_argument(
        "--no-material",
        action="store_true",
        help="discard a packet that produced no material outcome",
    )
    resolve.add_argument("--reason", help="required explanation for --no-material")
    resolve.set_defaults(handler=command_resolve)

    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    arguments = parser.parse_args(argv)
    try:
        repo = _resolve_repo(arguments.repo)
        arguments.handler(arguments, repo)
    except (GuidanceError, OSError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
