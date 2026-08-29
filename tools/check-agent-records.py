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
# Session summaries recorded from this date onward must carry a Distillation
# section; earlier sessions are grandfathered.
DISTILLATION_REQUIRED_FROM = "2026-08-28"
CLEANUP_REQUIRED_FROM = "2026-08-29"
STALENESS_WARNING_DAYS = 14
SESSION_ID_RE = re.compile(r"^S\d{8}-\d{3}-[a-z0-9][a-z0-9-]*$")
SESSION_PATH_RE = re.compile(
    r"^(?P<year>\d{4})/(?P<month>0[1-9]|1[0-2])/(?P<id>S\d{8}-\d{3}-[a-z0-9][a-z0-9-]*)$"
)
MILESTONE_ID_RE = re.compile(r"^M\d{4}$")
WORK_ITEM_ID_RE = re.compile(r"^M\d{4}-W\d{2}$")
EXPERIENCE_ID_RE = re.compile(r"^E\d{4}$")
DECISION_ID_RE = re.compile(r"^D\d{4}$")
DECISION_ID_SEARCH_RE = re.compile(r"\bD\d{4}\b")
CHECKPOINT_ID_RE = re.compile(r"^P\d{8}-\d{3}$")
STABLE_AGENT_ID_RE = re.compile(
    r"^(?:M\d{4}(?:-W\d{2})?|E\d{4}|P\d{8}-\d{3})$"
)
OUTPUT_REF_PATH_RE = re.compile(r"^outputs/\d{4}\.txt$")
# Unanchored companions of the stable-id patterns, used to extract ids from
# markdown link targets that carry directory prefixes.
SESSION_ID_SEARCH_RE = re.compile(r"S\d{8}-\d{3}-[a-z0-9][a-z0-9-]*")
MILESTONE_ID_SEARCH_RE = re.compile(r"M\d{4}")
EXPERIENCE_ID_SEARCH_RE = re.compile(r"E\d{4}")
SKILL_SLUG_RE = re.compile(r"^[a-z0-9]+(?:-[a-z0-9]+)*$")
SKILL_FRONTMATTER_KEYS = {
    "name",
    "description",
    "license",
    "allowed-tools",
    "metadata",
}
MAX_SKILL_NAME_LENGTH = 64
MAX_SKILL_DESCRIPTION_LENGTH = 1024
DECISION_IDENTITY_STOP_WORDS = {
    "a",
    "an",
    "and",
    "as",
    "at",
    "be",
    "before",
    "by",
    "each",
    "for",
    "from",
    "in",
    "including",
    "into",
    "is",
    "of",
    "on",
    "or",
    "per",
    "the",
    "this",
    "to",
    "with",
}
DECISION_TERM_ALIASES = {"marks": "mark"}
SKILL_LIFECYCLE_STATUSES = {"Draft", "Active", "Retired"}
SKILL_INDEX_COLUMNS = ("Skill", "Status", "Use when")
DOMAIN_SKILL_SLUGS = {
    "cpu-backend-performance",
    "cuda-driver-abi-compatibility",
    "device-lifecycle-resilience",
    "gpu-virtualization-vfio-user",
    "linux-device-driver-uapi",
    "mlir-compiler-engineering",
    "nvml-telemetry-compatibility",
    "pcie-vpci-device-model",
    "ptx-simt-semantics",
    "runtime-contracts-registry",
    "vulkan-spirv-compute",
}
DOMAIN_SKILL_SECTIONS = ("Inputs", "Routing", "Workflow", "Output", "Verification")
OPENAI_INTERFACE_FIELDS = {"display_name", "short_description", "default_prompt"}
MIN_OPENAI_SHORT_DESCRIPTION_LENGTH = 25
MAX_OPENAI_SHORT_DESCRIPTION_LENGTH = 64
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
        self.warnings: list[str] = []
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
        status = session.get("status")
        if status not in ALLOWED_STATUSES:
            self.add_error(session_file, "status has an unsupported value")
        elif status == "in_progress" and session.get("ended_at") is not None:
            self.add_error(session_file, "in_progress sessions must have ended_at set to null")
        elif status != "in_progress" and session.get("ended_at") is None:
            self.add_error(session_file, "terminal sessions must record ended_at")
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
        summary_path = self.validate_relative_file(
            session_dir, session.get("summary"), "summary"
        )
        self.validate_relative_file(session_dir, session.get("notes"), "notes")
        self.validate_distillation(summary_path, session)
        self.validate_cleanup(summary_path, session)

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

    def linked_ids(self, path: Path, id_pattern: re.Pattern) -> set[str] | None:
        """Collect stable IDs from markdown link targets in an index README."""
        if not path.is_file():
            self.add_error(path, "required index file is missing")
            return None
        try:
            text = path.read_text(encoding="utf-8")
        except UnicodeDecodeError as exc:
            self.add_error(path, f"is not valid UTF-8: {exc}")
            return None
        ids: set[str] = set()
        for match in MARKDOWN_LINK_RE.finditer(text):
            found = id_pattern.search(match.group(1))
            if found:
                ids.add(found.group(0))
        return ids

    def validate_index_completeness(self, actual_session_ids: set[str]) -> None:
        sessions_index = self.linked_ids(
            self.sessions_root / "README.md", SESSION_ID_SEARCH_RE
        )
        if sessions_index is not None and sessions_index != actual_session_ids:
            missing = sorted(actual_session_ids - sessions_index)
            stale = sorted(sessions_index - actual_session_ids)
            if missing:
                self.add_error(
                    self.sessions_root / "README.md",
                    f"sessions index is missing: {', '.join(missing)}",
                )
            if stale:
                self.add_error(
                    self.sessions_root / "README.md",
                    f"sessions index references nonexistent sessions: {', '.join(stale)}",
                )

        plan_root = self.agent_root / "plan"
        actual_milestones = {
            path.parent.name[:5]
            for path in plan_root.glob("M*/plan.md")
            if MILESTONE_ID_RE.fullmatch(path.parent.name[:5])
        }
        plans_index = self.linked_ids(plan_root / "README.md", MILESTONE_ID_SEARCH_RE)
        if plans_index is not None and plans_index != actual_milestones:
            missing = sorted(actual_milestones - plans_index)
            stale = sorted(plans_index - actual_milestones)
            if missing:
                self.add_error(
                    plan_root / "README.md",
                    f"milestone index is missing: {', '.join(missing)}",
                )
            if stale:
                self.add_error(
                    plan_root / "README.md",
                    f"milestone index references nonexistent milestones: {', '.join(stale)}",
                )

        experience_root = self.agent_root / "experience"
        actual_experience = {
            path.name[:5]
            for path in experience_root.glob("E*-*.md")
            if EXPERIENCE_ID_RE.fullmatch(path.name[:5])
        }
        experience_index = self.linked_ids(
            experience_root / "README.md", EXPERIENCE_ID_SEARCH_RE
        )
        if experience_index is not None and experience_index != actual_experience:
            missing = sorted(actual_experience - experience_index)
            stale = sorted(experience_index - actual_experience)
            if missing:
                self.add_error(
                    experience_root / "README.md",
                    f"experience index is missing: {', '.join(missing)}",
                )
            if stale:
                self.add_error(
                    experience_root / "README.md",
                    f"experience index references nonexistent records: {', '.join(stale)}",
                )

    @staticmethod
    def numbered_decisions(markdown: str) -> list[str]:
        """Return full numbered items from the Decisions to Close section."""

        match = re.search(r"(?m)^## Decisions to Close\s*$", markdown)
        if match is None:
            return []

        tail = markdown[match.end() :]
        next_heading = re.search(r"(?m)^##\s+", tail)
        body = tail[: next_heading.start()] if next_heading else tail
        decisions: list[str] = []
        current: list[str] | None = None
        for line in body.splitlines():
            item = re.match(r"^\d+\.\s+(.+?)\s*$", line)
            if item:
                if current is not None:
                    decisions.append(" ".join(current))
                current = [item.group(1)]
            elif current is not None and (line.startswith("   ") or line.startswith("\t")):
                current.append(line.strip())
            elif current is not None:
                decisions.append(" ".join(current))
                current = None
        if current is not None:
            decisions.append(" ".join(current))
        return decisions

    @staticmethod
    def normalized_decision_terms(decision: str) -> tuple[str, ...]:
        """Build an ordered Unicode-aware identity for one open decision."""

        decision = re.sub(r"\[([^]]+)\]\([^)]+\)", r"\1", decision)
        decision = decision.replace("`", "").casefold()
        terms: list[str] = []
        for token in re.findall(r"[^\W_]+", decision, flags=re.UNICODE):
            if token in DECISION_IDENTITY_STOP_WORDS:
                continue
            if DECISION_ID_RE.fullmatch(token.upper()):
                continue
            terms.append(DECISION_TERM_ALIASES.get(token, token))
        return tuple(terms)

    @classmethod
    def decisions_match(cls, plan_decision: str, ledger_decision: str) -> bool:
        """Match plan and ledger decisions by exact normalized identity."""

        plan_terms = cls.normalized_decision_terms(plan_decision)
        ledger_terms = cls.normalized_decision_terms(ledger_decision)
        return plan_terms == ledger_terms

    def validate_open_decisions(self) -> None:
        """Verify each milestone plan decision maps uniquely to its ledger row."""

        plan_root = self.agent_root / "plan"
        plan_decisions: dict[str, list[str]] = {}
        for plan_file in sorted(plan_root.glob("M*/plan.md")):
            milestone_id = plan_file.parent.name[:5]
            try:
                text = plan_file.read_text(encoding="utf-8")
            except UnicodeDecodeError:
                continue
            if "## Decisions to Close" not in text:
                continue
            decisions = self.numbered_decisions(text)
            if decisions:
                plan_decisions[milestone_id] = decisions

        ledger = self.agent_root / "memory" / "open-decisions.md"
        ledger_decisions: dict[str, list[str]] = {}
        if ledger.is_file():
            try:
                ledger_text = ledger.read_text(encoding="utf-8")
            except UnicodeDecodeError as exc:
                self.add_error(ledger, f"is not valid UTF-8: {exc}")
                ledger_text = ""
            for line in ledger_text.splitlines():
                cells = self.markdown_table_cells(line)
                if cells is not None and len(cells) >= 2:
                    if MILESTONE_ID_RE.fullmatch(cells[0]):
                        ledger_decisions.setdefault(cells[0], []).append(cells[1])
        elif plan_decisions:
            self.add_error(
                ledger,
                "open-decisions ledger is missing while plans declare "
                f"{sum(len(items) for items in plan_decisions.values())} "
                "open decision(s)",
            )
            return

        all_milestones = sorted(set(plan_decisions) | set(ledger_decisions))
        for milestone_id in all_milestones:
            plan_items = plan_decisions.get(milestone_id, [])
            ledger_items = ledger_decisions.get(milestone_id, [])
            if len(plan_items) != len(ledger_items):
                self.add_error(
                    ledger,
                    f"{milestone_id} declares {len(plan_items)} open decision(s) "
                    f"but the ledger lists {len(ledger_items)}",
                )

            for source_name, decisions in (
                ("plan", plan_items),
                ("ledger", ledger_items),
            ):
                seen: set[tuple[str, ...]] = set()
                for decision in decisions:
                    signature = self.normalized_decision_terms(decision)
                    if signature in seen:
                        self.add_error(
                            ledger,
                            f"duplicate {source_name} decision for {milestone_id}: "
                            f"{decision!r}",
                        )
                    seen.add(signature)

            matched_ledger: set[int] = set()
            for plan_item in plan_items:
                candidates = [
                    index
                    for index, ledger_item in enumerate(ledger_items)
                    if self.decisions_match(plan_item, ledger_item)
                ]
                if not candidates:
                    self.add_error(
                        ledger,
                        f"{milestone_id} plan decision has no matching ledger row: "
                        f"{plan_item!r}",
                    )
                elif len(candidates) > 1:
                    self.add_error(
                        ledger,
                        f"{milestone_id} plan decision matches multiple ledger rows: "
                        f"{plan_item!r}",
                    )
                else:
                    matched_ledger.add(candidates[0])

            for index, ledger_item in enumerate(ledger_items):
                if index not in matched_ledger:
                    self.add_error(
                        ledger,
                        f"{milestone_id} ledger decision has no matching plan item: "
                        f"{ledger_item!r}",
                    )

    def validate_decision_index(self) -> None:
        """Validate unique decision IDs and resolve all agent-record references."""

        index_path = self.agent_root / "memory" / "decisions-index.md"
        indexed: dict[str, int] = {}
        if index_path.is_file():
            try:
                index_text = index_path.read_text(encoding="utf-8")
            except UnicodeDecodeError as exc:
                self.add_error(index_path, f"is not valid UTF-8: {exc}")
                index_text = ""
            for line_no, line in enumerate(index_text.splitlines(), start=1):
                cells = self.markdown_table_cells(line)
                if cells is None or not cells or not DECISION_ID_RE.fullmatch(cells[0]):
                    continue
                decision_id = cells[0]
                if decision_id in indexed:
                    self.add_error(
                        index_path,
                        f"duplicate decision ID {decision_id} "
                        f"(rows {indexed[decision_id]} and {line_no})",
                    )
                else:
                    indexed[decision_id] = line_no

        references: dict[str, set[Path]] = {}
        if self.agent_root.is_dir():
            for path in sorted(self.agent_root.rglob("*.md")):
                if path == index_path:
                    continue
                try:
                    text = path.read_text(encoding="utf-8")
                except (OSError, UnicodeDecodeError):
                    continue
                for decision_id in DECISION_ID_SEARCH_RE.findall(text):
                    references.setdefault(decision_id, set()).add(path)

            for events_path in sorted(self.sessions_root.rglob("events.jsonl")):
                try:
                    event_lines = events_path.read_text(encoding="utf-8").splitlines()
                except (OSError, UnicodeDecodeError):
                    continue
                for line in event_lines:
                    try:
                        event = json.loads(line)
                    except json.JSONDecodeError:
                        continue
                    if not isinstance(event, dict) or event.get("type") != "decision":
                        continue
                    content = event.get("content")
                    if not isinstance(content, str):
                        continue
                    for decision_id in DECISION_ID_SEARCH_RE.findall(content):
                        references.setdefault(decision_id, set()).add(events_path)

        for decision_id, paths in sorted(references.items()):
            if decision_id in indexed:
                continue
            relative_paths = ", ".join(
                sorted(str(path.relative_to(self.repo_root)) for path in paths)
            )
            self.add_error(
                index_path,
                f"decision reference {decision_id} is absent from the index "
                f"(referenced by {relative_paths})",
            )

    def validate_distillation(self, summary_path: Path | None, session: dict) -> None:
        started_at = session.get("started_at")
        if not isinstance(started_at, str) or started_at < DISTILLATION_REQUIRED_FROM:
            return
        if summary_path is None:
            return
        try:
            text = summary_path.read_text(encoding="utf-8")
        except (OSError, UnicodeDecodeError):
            self.add_error(summary_path, "cannot read session summary for distillation")
            return
        parts = text.split("## Distillation", 1)
        if len(parts) != 2:
            self.add_error(
                summary_path,
                "sessions from "
                f"{DISTILLATION_REQUIRED_FROM} onward require a '## Distillation' "
                "section (use 'none' when nothing was promoted)",
            )
            return
        body = parts[1].split("\n## ", 1)[0].strip()
        if not body:
            self.add_error(summary_path, "Distillation section is empty")
        elif "none" not in body.lower() and "- " not in body:
            self.add_error(
                summary_path,
                "Distillation section must state 'none' or list promoted records",
            )

    def validate_cleanup(self, summary_path: Path | None, session: dict) -> None:
        started_at = session.get("started_at")
        if not isinstance(started_at, str) or started_at < CLEANUP_REQUIRED_FROM:
            return
        if summary_path is None:
            return
        try:
            text = summary_path.read_text(encoding="utf-8")
        except (OSError, UnicodeDecodeError):
            self.add_error(summary_path, "cannot read session summary for cleanup")
            return
        parts = text.split("## Cleanup", 1)
        if len(parts) != 2:
            self.add_error(
                summary_path,
                "sessions from "
                f"{CLEANUP_REQUIRED_FROM} onward require a '## Cleanup' "
                "section (use 'none' when no disposable work remains)",
            )
            return
        body = parts[1].split("\n## ", 1)[0].strip()
        if not body:
            self.add_error(summary_path, "Cleanup section is empty")
        elif "none" not in body.lower() and "- " not in body:
            self.add_error(
                summary_path,
                "Cleanup section must state 'none' or list removed/retained artifacts",
            )

    def validate_progress_health(self) -> None:
        checkpoint_dates: list[str] = []
        checkpoints_root = self.agent_root / "progress" / "checkpoints"
        for checkpoint_file in checkpoints_root.glob("*/*.md"):
            match = re.search(r"(?m)^captured:\s*(\d{4}-\d{2}-\d{2})", checkpoint_file.read_text(encoding="utf-8"))
            if match:
                checkpoint_dates.append(match.group(1))
        if not checkpoint_dates:
            return
        latest_checkpoint = max(checkpoint_dates)

        def staleness_cutoff(day: str) -> str:
            captured = dt.date.fromisoformat(day)
            return (captured - dt.timedelta(days=STALENESS_WARNING_DAYS)).isoformat()

        cutoff = staleness_cutoff(latest_checkpoint)
        plan_root = self.agent_root / "plan"
        plan_files = sorted(plan_root.glob("M*/plan.md")) + sorted(
            plan_root.glob("M*/work/W*-*.md")
        )
        for plan_file in plan_files:
            try:
                text = plan_file.read_text(encoding="utf-8")
            except UnicodeDecodeError:
                continue
            status = re.search(r"(?m)^status:\s*(\S+)", text)
            updated = re.search(r"(?m)^updated:\s*(\d{4}-\d{2}-\d{2})", text)
            if status and updated and status.group(1) == "Active":
                if updated.group(1) < cutoff:
                    self.warnings.append(
                        f"{plan_file.relative_to(self.repo_root)}: Active record not "
                        f"updated since {updated.group(1)} "
                        f"(latest checkpoint {latest_checkpoint})"
                    )

    def plan_statuses(self) -> dict[str, str]:
        """Map milestone and work-item ids to their plan frontmatter status."""
        statuses: dict[str, str] = {}
        plan_root = self.agent_root / "plan"
        candidates = sorted(plan_root.glob("M*/plan.md")) + sorted(
            plan_root.glob("M*/work/W*-*.md")
        )
        for plan_file in candidates:
            try:
                text = plan_file.read_text(encoding="utf-8")
            except UnicodeDecodeError:
                continue
            record_id = re.search(r"(?m)^id:\s*(\S+)", text)
            status = re.search(r"(?m)^status:\s*(\S+)", text)
            if record_id and status:
                statuses[record_id.group(1)] = status.group(1)
        return statuses

    def validate_current_progress(self) -> None:
        current = self.agent_root / "progress" / "current.md"
        if not current.is_file():
            return
        try:
            text = current.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            return
        referenced = re.search(r"(?m)^checkpoint:\s*(P\d{8}-\d{3})\b", text)
        if referenced is None:
            return
        known_checkpoints = sorted(
            record_id
            for record_id in self.agent_record_ids
            if CHECKPOINT_ID_RE.fullmatch(record_id)
        )
        if not known_checkpoints:
            return
        if referenced.group(1) not in known_checkpoints:
            self.add_error(
                current,
                f"references unknown checkpoint {referenced.group(1)}",
            )
        elif referenced.group(1) != known_checkpoints[-1]:
            self.add_error(
                current,
                f"references checkpoint {referenced.group(1)} but the latest "
                f"recorded checkpoint is {known_checkpoints[-1]}; refresh "
                "current progress",
            )

    def validate_status_consistency(self) -> None:
        """Warn when the latest complete session's plan statuses drifted.

        Only the newest complete session is compared: older sessions
        legitimately reflect the state of their time.
        """
        latest: tuple[str, str, Path] | None = None
        for session_file in sorted(self.sessions_root.rglob("session.json")):
            if session_file.is_symlink() or not session_file.is_file():
                continue
            try:
                document = json.loads(session_file.read_text(encoding="utf-8"))
            except (OSError, UnicodeDecodeError, json.JSONDecodeError):
                continue
            if not isinstance(document, dict) or document.get("status") != "complete":
                continue
            started_at = document.get("started_at")
            if not isinstance(started_at, str):
                continue
            key = (started_at, str(session_file))
            if latest is None or key > (latest[0], latest[1]):
                latest = (started_at, str(session_file), session_file)
        if latest is None:
            return
        try:
            document = json.loads(latest[2].read_text(encoding="utf-8"))
        except (OSError, UnicodeDecodeError, json.JSONDecodeError):
            return

        session_to_plan_status = {
            "queued": "Queued",
            "active": "Active",
            "in_progress": "Active",
            "blocked": "Blocked",
            "complete": "Complete",
        }
        plan_statuses = self.plan_statuses()
        for collection in ("milestones", "work_items"):
            records = document.get(collection)
            if not isinstance(records, list):
                continue
            for record in records:
                if not isinstance(record, dict):
                    continue
                record_id = record.get("id")
                session_status = record.get("status")
                mapped = session_to_plan_status.get(session_status)
                plan_status = plan_statuses.get(record_id) if isinstance(record_id, str) else None
                if mapped and plan_status and mapped != plan_status:
                    self.warnings.append(
                        f"{latest[2].relative_to(self.repo_root)}: latest complete "
                        f"session records {record_id} as '{session_status}' while "
                        f"the plan says '{plan_status}'; one of them is stale"
                    )

    @staticmethod
    def markdown_table_cells(line: str) -> tuple[str, ...] | None:
        stripped = line.strip()
        if not stripped.startswith("|") or not stripped.endswith("|"):
            return None
        return tuple(cell.strip() for cell in stripped[1:-1].split("|"))

    def parse_skills_index(self, path: Path, text: str) -> dict[str, str]:
        """Parse only the canonical three-column table in the README Index section."""
        lines = text.splitlines()
        index_headings = [
            index for index, line in enumerate(lines) if line.strip() == "## Index"
        ]
        if len(index_headings) != 1:
            self.add_error(path, "skills index requires exactly one '## Index' section")
            return {}

        section_start = index_headings[0] + 1
        section_end = next(
            (
                index
                for index in range(section_start, len(lines))
                if re.fullmatch(r"##[ \t]+.+", lines[index].strip())
            ),
            len(lines),
        )
        header_index = next(
            (
                index
                for index in range(section_start, section_end)
                if self.markdown_table_cells(lines[index]) == SKILL_INDEX_COLUMNS
            ),
            None,
        )
        if header_index is None:
            self.add_error(
                path,
                "skills Index section requires the table columns: Skill, Status, Use when",
            )
            return {}
        if header_index + 1 >= section_end:
            self.add_error(path, "skills index table is missing its separator row")
            return {}

        separator = self.markdown_table_cells(lines[header_index + 1])
        if separator is None or len(separator) != len(SKILL_INDEX_COLUMNS) or not all(
            re.fullmatch(r":?-{3,}:?", cell) for cell in separator
        ):
            self.add_error(path, "skills index table has an invalid separator row")
            return {}

        indexed: dict[str, str] = {}
        row_index = header_index + 2
        while row_index < section_end:
            line = lines[row_index]
            if not line.strip():
                break
            cells = self.markdown_table_cells(line)
            if cells is None:
                break
            if len(cells) != len(SKILL_INDEX_COLUMNS):
                self.add_error(
                    path,
                    f"skills index row {row_index + 1} must have exactly three columns",
                )
                row_index += 1
                continue

            skill_cell, status, use_when = cells
            link = re.fullmatch(r"\[([^]]+)\]\(([^)]+)\)", skill_cell)
            if link is None:
                self.add_error(
                    path,
                    f"skills index row {row_index + 1} has an invalid skill link",
                )
                row_index += 1
                continue
            label, target = link.groups()
            slug = label.strip()
            if not SKILL_SLUG_RE.fullmatch(slug):
                self.add_error(
                    path,
                    f"skills index row {row_index + 1} label is not a skill slug",
                )
                row_index += 1
                continue
            expected_target = f"{slug}/SKILL.md"
            if target != expected_target:
                self.add_error(
                    path,
                    f"skills index row {row_index + 1} must link to {expected_target!r}",
                )
            if status not in SKILL_LIFECYCLE_STATUSES:
                self.add_error(
                    path,
                    f"skills index row {row_index + 1} has unsupported status {status!r}",
                )
            if not use_when:
                self.add_error(
                    path,
                    f"skills index row {row_index + 1} requires a non-empty 'Use when'",
                )
            if slug in indexed:
                self.add_error(path, f"skills index repeats row for: {slug}")
            else:
                indexed[slug] = status
            row_index += 1

        if not indexed:
            self.add_error(path, "skills index table contains no skill rows")
        return indexed

    @staticmethod
    def markdown_h2_headings(text: str) -> list[tuple[int, str]]:
        headings: list[tuple[int, str]] = []
        fence_marker: str | None = None
        fence_length = 0
        for line_number, line in enumerate(text.splitlines(), start=1):
            fence = re.match(r"^[ \t]{0,3}(`{3,}|~{3,})(.*)$", line)
            if fence:
                marker = fence.group(1)
                if fence_marker is None:
                    fence_marker = marker[0]
                    fence_length = len(marker)
                elif (
                    marker[0] == fence_marker
                    and len(marker) >= fence_length
                    and not fence.group(2).strip()
                ):
                    fence_marker = None
                    fence_length = 0
                continue
            if fence_marker is not None:
                continue
            heading = re.fullmatch(r"##[ \t]+(.+?)[ \t]*", line)
            if heading:
                headings.append((line_number, heading.group(1)))
        return headings

    def validate_domain_skill_sections(self, path: Path, text: str) -> None:
        headings = self.markdown_h2_headings(text)
        positions: list[int] = []
        valid = True
        for required in DOMAIN_SKILL_SECTIONS:
            matches = [line_number for line_number, heading in headings if heading == required]
            if len(matches) != 1:
                self.add_error(
                    path,
                    f"domain SKILL.md requires exactly one '## {required}' section",
                )
                valid = False
            else:
                positions.append(matches[0])
        if valid and positions != sorted(positions):
            expected = " -> ".join(DOMAIN_SKILL_SECTIONS)
            self.add_error(path, f"domain SKILL.md sections must appear in order: {expected}")

    def parse_openai_yaml(
        self, path: Path, text: str, slug: str
    ) -> dict[str, str] | None:
        """Parse the repository's deliberately small agents/openai.yaml subset."""
        interface: dict[str, str] = {}
        saw_interface = False
        for line_number, line in enumerate(text.splitlines(), start=1):
            if not line.strip() or line.lstrip().startswith("#"):
                continue
            if "\t" in line:
                self.add_error(path, f"line {line_number}: tabs are not allowed")
                return None

            indentation = len(line) - len(line.lstrip(" "))
            if indentation == 0:
                if line != "interface:":
                    self.add_error(
                        path,
                        f"line {line_number}: only the top-level 'interface:' mapping is supported",
                    )
                    return None
                if saw_interface:
                    self.add_error(path, "agents/openai.yaml repeats 'interface'")
                    return None
                saw_interface = True
                continue

            if indentation != 2 or not saw_interface:
                self.add_error(
                    path,
                    f"line {line_number}: expected a two-space-indented interface field",
                )
                return None
            field_match = re.fullmatch(r"  ([a-z][a-z0-9_-]*):[ ]+(.+)", line)
            if field_match is None:
                self.add_error(path, f"line {line_number}: invalid interface field syntax")
                return None
            field, raw_value = field_match.groups()
            if field not in OPENAI_INTERFACE_FIELDS:
                self.add_error(path, f"line {line_number}: unsupported interface field {field!r}")
                return None
            if field in interface:
                self.add_error(path, f"agents/openai.yaml repeats interface field {field!r}")
                return None
            if not raw_value.startswith('"'):
                self.add_error(path, f"line {line_number}: interface strings must be double-quoted")
                return None
            try:
                value = json.loads(raw_value)
            except json.JSONDecodeError as exc:
                self.add_error(
                    path,
                    f"line {line_number}: invalid quoted string: {exc.msg}",
                )
                return None
            if not isinstance(value, str):
                self.add_error(path, f"line {line_number}: interface value must be a string")
                return None
            interface[field] = value

        if not saw_interface:
            self.add_error(path, "agents/openai.yaml requires an 'interface' mapping")
            return None
        missing = OPENAI_INTERFACE_FIELDS - interface.keys()
        if missing:
            self.add_error(
                path,
                "agents/openai.yaml interface is missing field(s): "
                + ", ".join(sorted(missing)),
            )
            return None
        if not interface["display_name"].strip():
            self.add_error(path, "interface.display_name must be non-empty")
        short_description = interface["short_description"].strip()
        if not (
            MIN_OPENAI_SHORT_DESCRIPTION_LENGTH
            <= len(short_description)
            <= MAX_OPENAI_SHORT_DESCRIPTION_LENGTH
        ):
            self.add_error(
                path,
                "interface.short_description must be "
                f"{MIN_OPENAI_SHORT_DESCRIPTION_LENGTH}-{MAX_OPENAI_SHORT_DESCRIPTION_LENGTH} characters",
            )
        default_prompt = interface["default_prompt"].strip()
        skill_token = f"${slug}"
        token_pattern = re.compile(
            rf"(?<![$A-Za-z0-9_-]){re.escape(skill_token)}(?![A-Za-z0-9_-])"
        )
        if len(token_pattern.findall(default_prompt)) != 1:
            self.add_error(
                path,
                f"interface.default_prompt must mention exact token {skill_token!r} once",
            )
        return interface

    def parse_skill_frontmatter(self, path: Path, text: str) -> dict[str, str] | None:
        """Parse the top-level scalar fields needed by the Codex skill contract.

        The record validator intentionally stays stdlib-only. It accepts nested
        YAML under optional fields and block scalars, while validating the two
        required scalar fields without pretending to be a general YAML parser.
        """
        lines = text.splitlines()
        if not lines or lines[0].strip() != "---":
            self.add_error(path, "SKILL.md is missing YAML frontmatter")
            return None
        try:
            closing = next(
                index
                for index, line in enumerate(lines[1:], start=1)
                if line.strip() == "---"
            )
        except StopIteration:
            self.add_error(path, "SKILL.md has unterminated YAML frontmatter")
            return None

        fields: dict[str, str] = {}
        index = 1
        while index < closing:
            line = lines[index]
            if not line.strip() or line.lstrip().startswith("#"):
                index += 1
                continue
            if line[0].isspace():
                self.add_error(
                    path,
                    f"SKILL.md frontmatter has unexpected indentation at line {index + 1}",
                )
                return None
            match = re.fullmatch(r"([A-Za-z][A-Za-z0-9_-]*):(?:[ \t]*(.*))?", line)
            if match is None:
                self.add_error(
                    path,
                    f"SKILL.md frontmatter has unsupported syntax at line {index + 1}",
                )
                return None
            key = match.group(1)
            if key in fields:
                self.add_error(path, f"SKILL.md frontmatter repeats '{key}'")
                return None
            raw_value = (match.group(2) or "").strip()

            if re.fullmatch(r"[>|][+-]?", raw_value):
                folded = raw_value.startswith(">")
                block: list[str] = []
                index += 1
                while index < closing:
                    nested = lines[index]
                    if nested and not nested[0].isspace():
                        break
                    block.append(nested.strip())
                    index += 1
                fields[key] = (" " if folded else "\n").join(block).strip()
                continue

            if raw_value.startswith('"'):
                try:
                    decoded = json.loads(raw_value)
                except json.JSONDecodeError as exc:
                    self.add_error(
                        path,
                        f"SKILL.md frontmatter field '{key}' has invalid "
                        f"quoting: {exc.msg}",
                    )
                    return None
                if not isinstance(decoded, str):
                    self.add_error(
                        path,
                        f"SKILL.md frontmatter field '{key}' must be a string",
                    )
                    return None
                fields[key] = decoded
            elif raw_value.startswith("'"):
                if len(raw_value) < 2 or not raw_value.endswith("'"):
                    self.add_error(
                        path,
                        f"SKILL.md frontmatter field '{key}' has invalid quoting",
                    )
                    return None
                fields[key] = raw_value[1:-1].replace("''", "'")
            else:
                fields[key] = raw_value

            index += 1
            while index < closing and (
                not lines[index].strip() or lines[index][0].isspace()
            ):
                index += 1

        unexpected = sorted(set(fields) - SKILL_FRONTMATTER_KEYS)
        if unexpected:
            self.add_error(
                path,
                "SKILL.md frontmatter has non-Codex field(s): " + ", ".join(unexpected),
            )
            return None
        return fields

    def validate_skill_package(self, skill_dir: Path) -> None:
        slug = skill_dir.name
        skill_file = skill_dir / "SKILL.md"
        if not skill_file.is_file():
            self.add_error(skill_file, "skill package requires a SKILL.md")
            return
        try:
            text = skill_file.read_text(encoding="utf-8")
        except UnicodeDecodeError as exc:
            self.add_error(skill_file, f"is not valid UTF-8: {exc}")
            return

        frontmatter = self.parse_skill_frontmatter(skill_file, text)
        if frontmatter is None:
            return
        for field in ("name", "description"):
            if not frontmatter.get(field, "").strip():
                self.add_error(
                    skill_file,
                    f"SKILL.md frontmatter requires a non-empty '{field}'",
                )

        name = frontmatter.get("name", "").strip()
        if name:
            if not SKILL_SLUG_RE.fullmatch(name):
                self.add_error(
                    skill_file,
                    "SKILL.md name must be a lowercase-hyphenated slug",
                )
            if len(name) > MAX_SKILL_NAME_LENGTH:
                self.add_error(
                    skill_file,
                    f"SKILL.md name exceeds {MAX_SKILL_NAME_LENGTH} characters",
                )
            if name != slug:
                self.add_error(
                    skill_file,
                    f"SKILL.md name {name!r} must match directory slug {slug!r}",
                )

        description = frontmatter.get("description", "").strip()
        if description:
            if description.startswith("[TODO:"):
                self.add_error(
                    skill_file,
                    "SKILL.md description contains an unfinished TODO",
                )
            if "<" in description or ">" in description:
                self.add_error(
                    skill_file,
                    "SKILL.md description must not contain angle brackets",
                )
            if len(description) > MAX_SKILL_DESCRIPTION_LENGTH:
                self.add_error(
                    skill_file,
                    f"SKILL.md description exceeds {MAX_SKILL_DESCRIPTION_LENGTH} characters",
                )

        if slug in DOMAIN_SKILL_SLUGS:
            self.validate_domain_skill_sections(skill_file, text)

        openai_yaml = skill_dir / "agents" / "openai.yaml"
        if slug in DOMAIN_SKILL_SLUGS and not openai_yaml.is_file():
            self.add_error(
                openai_yaml,
                "domain skill package requires agents/openai.yaml",
            )
        if openai_yaml.exists():
            if not openai_yaml.is_file():
                self.add_error(openai_yaml, "agents/openai.yaml must be a regular file")
            else:
                try:
                    openai_text = openai_yaml.read_text(encoding="utf-8")
                except UnicodeDecodeError as exc:
                    self.add_error(openai_yaml, f"is not valid UTF-8: {exc}")
                else:
                    self.parse_openai_yaml(openai_yaml, openai_text, slug)

    def validate_skills(self) -> None:
        """Enforce Codex-compatible packages and the repository catalog."""
        skills_root = self.agent_root / "skills"
        if not skills_root.is_dir():
            return

        codex_entry = self.repo_root / ".agents" / "skills"
        if not codex_entry.is_symlink():
            self.add_error(
                codex_entry,
                "Codex repository discovery requires a symlink to ../agent/skills",
            )
        else:
            target = codex_entry.readlink().as_posix()
            if target != "../agent/skills":
                self.add_error(
                    codex_entry,
                    f"Codex skill symlink must target '../agent/skills', found {target!r}",
                )
            try:
                resolved_entry = codex_entry.resolve(strict=True)
            except FileNotFoundError:
                self.add_error(codex_entry, "Codex skill symlink target does not exist")
            else:
                if resolved_entry != skills_root.resolve():
                    self.add_error(
                        codex_entry,
                        "Codex skill symlink resolves outside agent/skills",
                    )

        actual: set[str] = set()
        for entry in sorted(skills_root.iterdir()):
            if not entry.is_dir():
                continue
            if SKILL_SLUG_RE.fullmatch(entry.name):
                actual.add(entry.name)
            else:
                self.add_error(
                    entry,
                    "skill directory name must be a lowercase-hyphenated slug",
                )

        index_path = skills_root / "README.md"
        if not index_path.is_file():
            self.add_error(index_path, "required skills index is missing")
            return
        try:
            index_text = index_path.read_text(encoding="utf-8")
        except UnicodeDecodeError as exc:
            self.add_error(index_path, f"is not valid UTF-8: {exc}")
            return
        indexed = set(self.parse_skills_index(index_path, index_text))

        for missing in sorted(actual - indexed):
            self.add_error(index_path, f"skills index is missing: {missing}")
        for stale in sorted(indexed - actual):
            self.add_error(
                index_path, f"skills index references nonexistent skill: {stale}"
            )

        for slug in sorted(actual):
            self.validate_skill_package(skills_root / slug)

    def validate_entry_points(self) -> None:
        """Keep tool entry-point bridges single-sourced and well-formed.

        Inert when the bridges are absent: these files are conveniences for
        specific agent CLIs, not requirements of the record system.
        """
        claude_md = self.repo_root / "CLAUDE.md"
        if claude_md.is_file():
            try:
                text = claude_md.read_text(encoding="utf-8")
            except UnicodeDecodeError as exc:
                self.add_error(claude_md, f"is not valid UTF-8: {exc}")
            else:
                if "@AGENTS.md" not in text:
                    self.add_error(
                        claude_md,
                        "must import the canonical rules with an "
                        "'@AGENTS.md' line instead of duplicating them",
                    )

        settings = self.repo_root / ".claude" / "settings.json"
        if settings.is_file():
            try:
                json.loads(settings.read_text(encoding="utf-8"))
            except (UnicodeDecodeError, json.JSONDecodeError) as exc:
                self.add_error(settings, f"is not valid JSON: {exc}")

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
        actual_session_ids: set[str] = set()
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
                if SESSION_ID_RE.fullmatch(session_file.parent.name):
                    actual_session_ids.add(session_file.parent.name)

        self.validate_index_completeness(actual_session_ids)
        self.validate_open_decisions()
        self.validate_decision_index()
        self.validate_progress_health()
        self.validate_current_progress()
        self.validate_status_consistency()
        self.validate_skills()
        self.validate_entry_points()
        self.validate_markdown_links()
        if self.errors:
            for error in self.errors:
                print(f"error: {error}", file=sys.stderr)
            print(f"agent record validation failed with {len(self.errors)} error(s)", file=sys.stderr)
            return 1

        for warning in self.warnings:
            print(f"warning: {warning}", file=sys.stderr)
        print(
            "agent records: ok "
            f"({self.session_count} session(s), {self.event_count} event(s), "
            f"{self.markdown_count} Markdown file(s)"
            + (f", {len(self.warnings)} warning(s)" if self.warnings else "")
            + ")"
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
