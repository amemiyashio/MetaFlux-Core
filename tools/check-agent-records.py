#!/usr/bin/env python3
"""Validate MetaFlux project work records using only the stdlib."""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import re
import shlex
import sys
from pathlib import Path, PurePosixPath
from urllib.parse import unquote, urlsplit


REQUIRED_SESSION_FIELDS = {
    "schema_version",
    "id",
    "repository",
    "started_at",
    "ended_at",
    "time_precision",
    "status",
    "fidelity",
    "agents",
    "milestones",
    "work_items",
    "base_revision",
    "final_revision",
    "event_log",
    "summary",
    "notes",
}
ALLOWED_EVENT_TYPES = {
    "objective",
    "decision",
    "tool_call",
    "tool_result",
    "work_note",
}
ALLOWED_STATUSES = {"in_progress", "complete", "blocked", "abandoned"}
ALLOWED_RECORD_STATUSES = ALLOWED_STATUSES | {"active", "queued"}
ALLOWED_FIDELITY = {"exact", "reconstructed"}
ALLOWED_TIME_PRECISION = {
    "date",
    "minute",
    "second",
    "millisecond",
    "microsecond",
}
MAX_INLINE_TEXT_BYTES = 65_536
SESSION_ID_RE = re.compile(r"^S\d{8}-\d{3}-[a-z0-9][a-z0-9-]*$")
SESSION_PATH_RE = re.compile(
    r"^(?P<year>\d{4})/(?P<month>0[1-9]|1[0-2])/(?P<id>S\d{8}-\d{3}-[a-z0-9][a-z0-9-]*)$"
)
MILESTONE_ID_RE = re.compile(r"^M\d{4}$")
WORK_ITEM_ID_RE = re.compile(r"^M\d{4}-W\d{2}$")
EXPERIENCE_ID_RE = re.compile(r"^E\d{4}$")
CHECKPOINT_ID_RE = re.compile(r"^P\d{8}-\d{3}$")
STABLE_AGENT_ID_RE = re.compile(
    r"^(?:M\d{4}(?:-W\d{2})?|E\d{4}|P\d{8}-\d{3})$"
)
OUTPUT_REF_PATH_RE = re.compile(r"^outputs/\d{4}\.txt$")
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
MARKDOWN_LINK_RE = re.compile(r"!?\[[^\]]*\]\(([^)]+)\)")
TEXT_SUFFIXES = {".json", ".jsonl", ".md", ".txt", ".yaml", ".yml"}

CREDENTIAL_ASSIGNMENT_RE = re.compile(
    r"(?ix)\b(?:password|passwd|pwd|api[_-]?key|access[_-]?token|auth[_-]?token|"
    r"client[_-]?secret|secret)\b\s*(?:=|:|\bis\b)\s*"
    r"(?P<value>\"[^\"\r\n]*\"|'[^'\r\n]*'|[^\s,;]+)"
)
CHINESE_CREDENTIAL_RE = re.compile(
    r"(?:密码|口令|令牌)\s*(?:=|:|：|是|为)?\s*"
    r"(?P<value>[^\s,，;；]+)",
    re.IGNORECASE,
)
BEARER_RE = re.compile(r"(?i)\bBearer\s+(?P<value>[^\s,;]+)")
URL_CREDENTIAL_RE = re.compile(r"[A-Za-z][A-Za-z0-9+.-]*://[^/\s:@]+:[^/\s@]+@")
KNOWN_SECRET_RE = re.compile(
    r"(?:AKIA[0-9A-Z]{16}|gh[pousr]_[A-Za-z0-9_]{20,}|"
    r"-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----)"
)


class Validator:
    def __init__(self, repo_root: Path) -> None:
        self.repo_root = repo_root
        self.agent_root = repo_root / "agent"
        self.sessions_root = self.agent_root / "sessions"
        self.errors: list[str] = []
        self.session_ids: set[str] = set()
        self.session_count = 0
        self.event_count = 0
        self.markdown_count = 0
        self.agent_record_ids: dict[str, Path] = {}

    def add_error(self, path: Path, message: str) -> None:
        try:
            display = path.relative_to(self.repo_root)
        except ValueError:
            display = path
        self.errors.append(f"{display}: {message}")

    @staticmethod
    def is_int(value: object) -> bool:
        return isinstance(value, int) and not isinstance(value, bool)

    @staticmethod
    def nonempty_string(value: object) -> bool:
        return isinstance(value, str) and bool(value.strip())

    def load_json(self, path: Path) -> object | None:
        try:
            return json.loads(path.read_text(encoding="utf-8"))
        except FileNotFoundError:
            self.add_error(path, "required file is missing")
        except UnicodeDecodeError as exc:
            self.add_error(path, f"is not valid UTF-8: {exc}")
        except json.JSONDecodeError as exc:
            self.add_error(path, f"invalid JSON at line {exc.lineno}, column {exc.colno}: {exc.msg}")
        return None

    def validate_relative_file(self, base: Path, value: object, label: str) -> Path | None:
        if not self.nonempty_string(value):
            self.add_error(base, f"{label} must be a non-empty relative path")
            return None

        raw = value.strip()
        if "\\" in raw:
            self.add_error(base, f"{label} must use POSIX separators: {raw!r}")
            return None

        pure = PurePosixPath(raw)
        if pure.is_absolute() or raw in {".", ".."} or ".." in pure.parts:
            self.add_error(base, f"{label} is unsafe: {raw!r}")
            return None

        candidate = base.joinpath(*pure.parts)
        resolved_base = base.resolve()
        resolved_candidate = candidate.resolve(strict=False)
        try:
            resolved_candidate.relative_to(resolved_base)
        except ValueError:
            self.add_error(base, f"{label} escapes its session directory: {raw!r}")
            return None

        if not candidate.exists():
            self.add_error(candidate, f"referenced by {label} but does not exist")
            return None
        if candidate.is_symlink():
            self.add_error(candidate, f"{label} must not reference a symlink")
            return None
        if not candidate.is_file():
            self.add_error(candidate, f"{label} must reference a regular file")
            return None
        return candidate

    def validate_optional_timestamp(self, path: Path, value: object, label: str) -> None:
        if value is None:
            return
        if not self.nonempty_string(value):
            self.add_error(path, f"{label} must be null or a non-empty ISO-8601 string")
            return
        try:
            dt.datetime.fromisoformat(value.replace("Z", "+00:00"))
        except ValueError:
            try:
                dt.date.fromisoformat(value)
            except ValueError:
                self.add_error(path, f"{label} is not an ISO-8601 date or timestamp: {value!r}")

    def validate_named_records(
        self,
        session_path: Path,
        records: object,
        collection: str,
        required_fields: set[str],
        global_ids: set[str],
    ) -> dict[str, dict[str, object]]:
        result: dict[str, dict[str, object]] = {}
        if not isinstance(records, list):
            self.add_error(session_path, f"{collection} must be an array")
            return result

        for index, item in enumerate(records):
            label = f"{collection}[{index}]"
            if not isinstance(item, dict):
                self.add_error(session_path, f"{label} must be an object")
                continue
            missing = required_fields - item.keys()
            if missing:
                self.add_error(session_path, f"{label} is missing fields: {', '.join(sorted(missing))}")
                continue
            record_id = item.get("id")
            if not self.nonempty_string(record_id):
                self.add_error(session_path, f"{label}.id must be a non-empty string")
                continue
            if record_id in global_ids:
                self.add_error(session_path, f"duplicate record id: {record_id}")
                continue
            global_ids.add(record_id)
            if not self.nonempty_string(item.get("title", item.get("role"))):
                self.add_error(session_path, f"{label} must have a non-empty title or role")
            result[record_id] = item
        return result

    def validate_session(self, session_dir: Path) -> None:
        session_file = session_dir / "session.json"
        loaded = self.load_json(session_file)
        if not isinstance(loaded, dict):
            if loaded is not None:
                self.add_error(session_file, "top-level value must be an object")
            return
        session = loaded

        missing = REQUIRED_SESSION_FIELDS - session.keys()
        if missing:
            self.add_error(session_file, f"missing required fields: {', '.join(sorted(missing))}")

        schema_version = session.get("schema_version")
        if not self.is_int(schema_version) or schema_version < 1:
            self.add_error(session_file, "schema_version must be a positive integer")

        session_id = session.get("id")
        if not self.nonempty_string(session_id) or not SESSION_ID_RE.fullmatch(session_id):
            self.add_error(session_file, "id must match SYYYYMMDD-NNN-slug")
        else:
            if session_id != session_dir.name:
                self.add_error(session_file, "id must match the session directory name")
            if session_id in self.session_ids:
                self.add_error(session_file, f"duplicate session id: {session_id}")
            self.session_ids.add(session_id)

        relative_session_dir = session_dir.relative_to(self.sessions_root).as_posix()
        path_match = SESSION_PATH_RE.fullmatch(relative_session_dir)
        if path_match is None:
            self.add_error(
                session_file,
                "session directory must match sessions/YYYY/MM/SYYYYMMDD-NNN-slug",
            )
        elif self.nonempty_string(session_id):
            if path_match.group("id") != session_id:
                self.add_error(session_file, "session path id does not match session.json id")
            date_digits = session_id[1:9]
            if date_digits[:4] != path_match.group("year") or date_digits[4:6] != path_match.group("month"):
                self.add_error(session_file, "session date must match its YYYY/MM directory")
            try:
                dt.datetime.strptime(date_digits, "%Y%m%d")
            except ValueError:
                self.add_error(session_file, "session id contains an invalid calendar date")

        if not self.nonempty_string(session.get("repository")):
            self.add_error(session_file, "repository must be a non-empty string")
        self.validate_optional_timestamp(session_file, session.get("started_at"), "started_at")
        self.validate_optional_timestamp(session_file, session.get("ended_at"), "ended_at")

        if session.get("time_precision") not in ALLOWED_TIME_PRECISION:
            self.add_error(session_file, "time_precision has an unsupported value")
        if session.get("status") not in ALLOWED_STATUSES:
            self.add_error(session_file, "status has an unsupported value")
        if session.get("fidelity") not in ALLOWED_FIDELITY:
            self.add_error(session_file, "fidelity has an unsupported value")

        for revision_name in ("base_revision", "final_revision"):
            revision = session.get(revision_name)
            if revision is not None and not self.nonempty_string(revision):
                self.add_error(session_file, f"{revision_name} must be null or a non-empty string")

        global_ids: set[str] = set()
        agents = self.validate_named_records(
            session_file, session.get("agents"), "agents", {"id", "role"}, global_ids
        )
        milestones = self.validate_named_records(
            session_file,
            session.get("milestones"),
            "milestones",
            {"id", "title", "status", "event_seqs"},
            global_ids,
        )
        work_items = self.validate_named_records(
            session_file,
            session.get("work_items"),
            "work_items",
            {"id", "milestone_id", "title", "status"},
            global_ids,
        )

        for milestone_id in milestones:
            if not MILESTONE_ID_RE.fullmatch(milestone_id):
                self.add_error(session_file, f"milestone id is not stable: {milestone_id}")
            elif milestone_id not in self.agent_record_ids:
                self.add_error(session_file, f"milestone id does not resolve to an Agent record: {milestone_id}")
        for work_id in work_items:
            if not WORK_ITEM_ID_RE.fullmatch(work_id):
                self.add_error(session_file, f"work item id is not stable: {work_id}")
            elif work_id not in self.agent_record_ids:
                self.add_error(session_file, f"work item id does not resolve to an Agent record: {work_id}")

        for collection_name, records in (("milestones", milestones), ("work_items", work_items)):
            for record_id, record in records.items():
                if record.get("status") not in ALLOWED_RECORD_STATUSES:
                    self.add_error(
                        session_file,
                        f"{collection_name} record {record_id} has an unsupported status",
                    )
        for work_id, work_item in work_items.items():
            milestone_id = work_item.get("milestone_id")
            if milestone_id not in milestones:
                self.add_error(
                    session_file,
                    f"work item {work_id} references unknown milestone {milestone_id!r}",
                )
            elif WORK_ITEM_ID_RE.fullmatch(work_id) and not work_id.startswith(f"{milestone_id}-"):
                self.add_error(
                    session_file,
                    f"work item {work_id} does not belong to milestone {milestone_id}",
                )

        event_log_path = self.validate_relative_file(
            session_dir, session.get("event_log"), "event_log"
        )
        self.validate_relative_file(session_dir, session.get("summary"), "summary")
        self.validate_relative_file(session_dir, session.get("notes"), "notes")

        events = self.validate_events(session_dir, event_log_path, schema_version)
        valid_event_seqs = {event["seq"] for event in events if self.is_int(event.get("seq"))}
        for milestone_id, milestone in milestones.items():
            event_seqs = milestone.get("event_seqs")
            if not isinstance(event_seqs, list) or not event_seqs:
                self.add_error(session_file, f"milestone {milestone_id}.event_seqs must be a non-empty array")
                continue
            seen: set[int] = set()
            for event_seq in event_seqs:
                if not self.is_int(event_seq):
                    self.add_error(session_file, f"milestone {milestone_id} has a non-integer event seq")
                elif event_seq in seen:
                    self.add_error(session_file, f"milestone {milestone_id} repeats event seq {event_seq}")
                elif event_seq not in valid_event_seqs:
                    self.add_error(session_file, f"milestone {milestone_id} references missing event seq {event_seq}")
                seen.add(event_seq)

        self.scan_session_credentials(session_dir)
        self.session_count += 1

    def validate_events(
        self, session_dir: Path, event_log_path: Path | None, schema_version: object
    ) -> list[dict[str, object]]:
        if event_log_path is None:
            return []
        try:
            lines = event_log_path.read_text(encoding="utf-8").splitlines()
        except UnicodeDecodeError as exc:
            self.add_error(event_log_path, f"is not valid UTF-8: {exc}")
            return []

        events: list[dict[str, object]] = []
        expected_seq = 1
        for line_number, line in enumerate(lines, start=1):
            if not line.strip():
                self.add_error(event_log_path, f"line {line_number}: blank JSONL lines are not allowed")
                continue
            try:
                event = json.loads(line)
            except json.JSONDecodeError as exc:
                self.add_error(event_log_path, f"line {line_number}: invalid JSON: {exc.msg}")
                continue
            if not isinstance(event, dict):
                self.add_error(event_log_path, f"line {line_number}: event must be an object")
                continue

            required = {"schema_version", "seq", "timestamp", "type", "actor"}
            missing = required - event.keys()
            if missing:
                self.add_error(
                    event_log_path,
                    f"line {line_number}: missing fields: {', '.join(sorted(missing))}",
                )
            if event.get("schema_version") != schema_version:
                self.add_error(event_log_path, f"line {line_number}: schema_version mismatch")

            seq = event.get("seq")
            if not self.is_int(seq):
                self.add_error(event_log_path, f"line {line_number}: seq must be an integer")
            elif seq != expected_seq:
                self.add_error(
                    event_log_path,
                    f"line {line_number}: expected contiguous seq {expected_seq}, got {seq}",
                )
                expected_seq = seq
            expected_seq += 1

            self.validate_optional_timestamp(
                event_log_path, event.get("timestamp"), f"line {line_number} timestamp"
            )
            if event.get("type") not in ALLOWED_EVENT_TYPES:
                self.add_error(event_log_path, f"line {line_number}: unsupported event type")
            if not self.nonempty_string(event.get("actor")):
                self.add_error(event_log_path, f"line {line_number}: actor must be a non-empty string")

            has_content = "content" in event
            has_output_ref = "output_ref" in event
            if has_content == has_output_ref:
                self.add_error(
                    event_log_path,
                    f"line {line_number}: exactly one of content or output_ref is required",
                )
            if has_content:
                content = event.get("content")
                if not isinstance(content, str):
                    self.add_error(event_log_path, f"line {line_number}: content must be a string")
                elif len(content.encode("utf-8")) > MAX_INLINE_TEXT_BYTES:
                    self.add_error(
                        event_log_path,
                        f"line {line_number}: content exceeds {MAX_INLINE_TEXT_BYTES} UTF-8 bytes",
                    )
            if has_output_ref:
                self.validate_output_ref(
                    session_dir, event_log_path, line_number, event.get("output_ref")
                )

            omitted = event.get("omitted", False)
            if not isinstance(omitted, bool):
                self.add_error(event_log_path, f"line {line_number}: omitted must be boolean")
            if omitted and not self.nonempty_string(event.get("reason")):
                self.add_error(event_log_path, f"line {line_number}: omitted events require a reason")
            if "reason" in event and not self.nonempty_string(event.get("reason")):
                self.add_error(event_log_path, f"line {line_number}: reason must be a non-empty string")
            if "redactions" in event:
                self.add_error(
                    event_log_path,
                    f"line {line_number}: redactions are not stored in project records",
                )

            events.append(event)

        self.event_count += len(events)
        return events

    def validate_output_ref(
        self,
        session_dir: Path,
        event_log_path: Path,
        line_number: int,
        value: object,
    ) -> None:
        if not isinstance(value, dict):
            self.add_error(event_log_path, f"line {line_number}: output_ref must be an object")
            return
        required = {"path", "bytes", "sha256"}
        missing = required - value.keys()
        if missing:
            self.add_error(
                event_log_path,
                f"line {line_number}: output_ref missing fields: {', '.join(sorted(missing))}",
            )
            return

        raw_output_path = value.get("path")
        if not isinstance(raw_output_path, str) or not OUTPUT_REF_PATH_RE.fullmatch(raw_output_path):
            self.add_error(
                event_log_path,
                f"line {line_number}: output_ref.path must match outputs/NNNN.txt",
            )
            output_path = None
        else:
            output_path = self.validate_relative_file(
                session_dir, raw_output_path, f"line {line_number} output_ref.path"
            )
        byte_count = value.get("bytes")
        sha256 = value.get("sha256")
        if not self.is_int(byte_count) or byte_count < 0:
            self.add_error(event_log_path, f"line {line_number}: output_ref.bytes must be non-negative")
        if not isinstance(sha256, str) or not SHA256_RE.fullmatch(sha256):
            self.add_error(event_log_path, f"line {line_number}: output_ref.sha256 is invalid")

        if output_path is None:
            return
        data = output_path.read_bytes()
        if self.is_int(byte_count) and len(data) != byte_count:
            self.add_error(
                event_log_path,
                f"line {line_number}: output_ref byte count is {byte_count}, actual is {len(data)}",
            )
        actual_sha256 = hashlib.sha256(data).hexdigest()
        if isinstance(sha256, str) and actual_sha256 != sha256:
            self.add_error(event_log_path, f"line {line_number}: output_ref SHA-256 mismatch")

    def scan_session_credentials(self, session_dir: Path) -> None:
        for path in sorted(session_dir.rglob("*")):
            if not path.is_file() or path.is_symlink() or path.suffix.lower() not in TEXT_SUFFIXES:
                continue
            try:
                text = path.read_text(encoding="utf-8")
            except UnicodeDecodeError:
                continue
            for line_number, line in enumerate(text.splitlines(), start=1):
                if KNOWN_SECRET_RE.search(line) or URL_CREDENTIAL_RE.search(line):
                    self.add_error(path, f"line {line_number}: possible credential or private key leak")
                for pattern in (CREDENTIAL_ASSIGNMENT_RE, CHINESE_CREDENTIAL_RE, BEARER_RE):
                    for match in pattern.finditer(line):
                        self.add_error(
                            path,
                            f"line {line_number}: credential-bearing text is not allowed",
                        )

    def validate_markdown_links(self) -> None:
        if not self.agent_root.is_dir():
            self.add_error(self.agent_root, "agent directory is missing")
            return

        for markdown_path in sorted(self.agent_root.rglob("*.md")):
            self.markdown_count += 1
            try:
                text = markdown_path.read_text(encoding="utf-8")
            except UnicodeDecodeError as exc:
                self.add_error(markdown_path, f"is not valid UTF-8: {exc}")
                continue
            for match in MARKDOWN_LINK_RE.finditer(text):
                raw_target = match.group(1).strip()
                try:
                    if raw_target.startswith("<"):
                        closing = raw_target.find(">")
                        target = raw_target[1:closing] if closing >= 0 else raw_target
                    else:
                        parts = shlex.split(raw_target)
                        target = parts[0] if parts else ""
                except ValueError as exc:
                    self.add_error(markdown_path, f"malformed Markdown link {raw_target!r}: {exc}")
                    continue

                if not target or target.startswith("#"):
                    continue
                split = urlsplit(target)
                if split.scheme in {"http", "https", "mailto"}:
                    continue
                if split.scheme or target.startswith("//") or target.startswith("/"):
                    self.add_error(markdown_path, f"unsafe or unsupported link target: {target!r}")
                    continue
                if "\\" in target:
                    self.add_error(markdown_path, f"relative link must use POSIX separators: {target!r}")
                    continue

                decoded_path = unquote(split.path)
                candidate = markdown_path.parent.joinpath(*PurePosixPath(decoded_path).parts)
                resolved_candidate = candidate.resolve(strict=False)
                try:
                    resolved_candidate.relative_to(self.repo_root)
                except ValueError:
                    self.add_error(markdown_path, f"relative link escapes the repository: {target!r}")
                    continue
                if not candidate.exists():
                    self.add_error(markdown_path, f"relative link target does not exist: {target!r}")

    @staticmethod
    def read_frontmatter_id(path: Path) -> tuple[str | None, str | None]:
        """Return the frontmatter id and a parse error, if any."""
        try:
            lines = path.read_text(encoding="utf-8").splitlines()
        except UnicodeDecodeError as exc:
            return None, f"is not valid UTF-8: {exc}"
        if not lines or lines[0].strip() != "---":
            return None, "is missing YAML frontmatter"

        try:
            closing = next(index for index, line in enumerate(lines[1:], start=1) if line.strip() == "---")
        except StopIteration:
            return None, "has unterminated YAML frontmatter"

        values: list[str] = []
        for line in lines[1:closing]:
            match = re.fullmatch(r"id:\s*(.*?)\s*", line)
            if not match:
                continue
            value = match.group(1)
            if len(value) >= 2 and value[0] == value[-1] and value[0] in {"'", '"'}:
                value = value[1:-1]
            values.append(value)
        if not values:
            return None, "frontmatter is missing id"
        if len(values) != 1:
            return None, "frontmatter contains multiple id fields"
        if not values[0]:
            return None, "frontmatter id is empty"
        return values[0], None

    def expected_instance_id(self, path: Path) -> str | None:
        relative = path.relative_to(self.agent_root)
        parts = relative.parts
        milestone_dir = (
            re.fullmatch(r"(M\d{4})(?:-[a-z0-9][a-z0-9-]*)?", parts[1])
            if len(parts) >= 2 and parts[0] == "plan"
            else None
        )
        if len(parts) == 3 and milestone_dir is not None:
            if parts[2] == "plan.md":
                return milestone_dir.group(1)
        if len(parts) == 4 and parts[0] == "plan" and parts[2] == "work":
            work_match = re.fullmatch(r"W(\d{2})-[^/]+\.md", parts[3])
            if milestone_dir is not None and work_match:
                return f"{milestone_dir.group(1)}-W{work_match.group(1)}"
        if len(parts) == 2 and parts[0] == "experience":
            match = re.fullmatch(r"(E\d{4})-[^/]+\.md", parts[1])
            return match.group(1) if match else None
        if len(parts) >= 4 and parts[0:2] == ("progress", "checkpoints"):
            match = re.fullmatch(r"(P\d{8}-\d{3})-[^/]+\.md", parts[-1])
            return match.group(1) if match else None
        return None

    def validate_agent_frontmatter_ids(self) -> None:
        if not self.agent_root.is_dir():
            return

        expected_paths: dict[Path, str] = {}
        for path in sorted(self.agent_root.rglob("*.md")):
            relative = path.relative_to(self.agent_root)
            if "templates" in relative.parts:
                continue
            expected_id = self.expected_instance_id(path)
            if expected_id is not None:
                expected_paths[path] = expected_id

        for path in sorted(self.agent_root.rglob("*.md")):
            relative = path.relative_to(self.agent_root)
            if "templates" in relative.parts:
                continue
            try:
                lines = path.read_text(encoding="utf-8").splitlines()
            except UnicodeDecodeError as exc:
                if path in expected_paths:
                    self.add_error(path, f"is not valid UTF-8: {exc}")
                continue
            if not lines or lines[0].strip() != "---":
                if path in expected_paths:
                    self.add_error(path, "is missing YAML frontmatter")
                continue

            record_id, error = self.read_frontmatter_id(path)
            if error is not None:
                if path in expected_paths:
                    self.add_error(path, error)
                continue
            assert record_id is not None
            if not STABLE_AGENT_ID_RE.fullmatch(record_id):
                if path in expected_paths:
                    self.add_error(path, f"frontmatter id is not a stable M/E/P id: {record_id!r}")
                continue
            expected_id = expected_paths.get(path)
            if expected_id is None:
                self.add_error(path, f"stable Agent id {record_id!r} is outside a recognized instance path")
                continue
            if record_id != expected_id:
                self.add_error(path, f"frontmatter id {record_id!r} does not match path id {expected_id!r}")
            previous = self.agent_record_ids.get(record_id)
            if previous is not None:
                self.add_error(path, f"duplicate Agent record id {record_id!r}; first used by {previous}")
            else:
                self.agent_record_ids[record_id] = path

    def run(self) -> int:
        self.validate_agent_frontmatter_ids()
        if not self.repo_root.is_dir():
            self.add_error(self.repo_root, "repository root is not a directory")
        elif not self.sessions_root.is_dir():
            self.add_error(self.sessions_root, "agent sessions directory is missing")
        else:
            session_files = sorted(
                path for path in self.sessions_root.rglob("session.json") if path.is_file() and not path.is_symlink()
            )
            if not session_files:
                self.add_error(self.sessions_root, "no session directories found")
            for session_file in session_files:
                self.validate_session(session_file.parent)

        self.validate_markdown_links()
        if self.errors:
            for error in self.errors:
                print(f"error: {error}", file=sys.stderr)
            print(f"agent record validation failed with {len(self.errors)} error(s)", file=sys.stderr)
            return 1

        print(
            "agent records: ok "
            f"({self.session_count} session(s), {self.event_count} event(s), "
            f"{self.markdown_count} Markdown file(s))"
        )
        return 0


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("repo_root", type=Path, help="path to the repository root")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    return Validator(args.repo_root.resolve()).run()


if __name__ == "__main__":
    raise SystemExit(main())
