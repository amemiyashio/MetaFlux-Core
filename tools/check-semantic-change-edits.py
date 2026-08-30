#!/usr/bin/env python3
"""Authorize edits to protected Agent history from committed Active SC records.

The hard gate reads permits only from HEAD. A staged SC cannot authorize history
changes in the same commit. Existing checkpoints and files below terminal
sessions are protected; a newly added checkpoint and an in-progress session are
ordinary record writes.
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path, PurePosixPath


SESSION_ID_RE = re.compile(
    r"^S\d{4,}-\d{8}-\d{3}-[a-z0-9][a-z0-9-]*$"
)
SC_FILE_RE = re.compile(
    r"^agent/semantic-changes/(?P<id>SC\d{4})-[a-z0-9][a-z0-9-]*\.md$"
)
DECISION_ID_RE = re.compile(r"^D\d{4}$")
SCOPE_RE = re.compile(r"^[a-z0-9]+(?:-[a-z0-9]+)*$")
MIGRATION_COLUMNS = ("Surface", "Class", "Disposition", "Evidence")
SC_INDEX_COLUMNS = ("ID", "Status", "Decision", "Scope", "Updated")
SC_SECTIONS = (
    "Semantic replacement",
    "Migration inventory",
    "Active-session handoff",
    "Evidence preservation",
    "Future-agent reminder",
    "Verification",
)
SC_REQUIRED_FIELDS = {
    "id",
    "status",
    "created",
    "updated",
    "decision",
    "session",
    "scope",
    "history_sync",
    "effective_revision",
    "superseded_by",
}
MIGRATION_CLASSES = {"Current", "Historical", "Tooling", "Active session"}
MIGRATION_DISPOSITIONS = {
    "Pending",
    "Migrated",
    "Removed",
    "Retained evidence",
}


def git_bytes(repo_root: Path, *arguments: str) -> subprocess.CompletedProcess[bytes]:
    return subprocess.run(
        ["git", *arguments],
        cwd=repo_root,
        check=False,
        capture_output=True,
    )


def head_blob(repo_root: Path, relative_path: str) -> bytes | None:
    result = git_bytes(repo_root, "show", f"HEAD:{relative_path}")
    return result.stdout if result.returncode == 0 else None


def index_blob(repo_root: Path, relative_path: str) -> bytes | None:
    result = git_bytes(repo_root, "show", f":{relative_path}")
    return result.stdout if result.returncode == 0 else None


def head_has_path(repo_root: Path, relative_path: str) -> bool:
    return git_bytes(repo_root, "cat-file", "-e", f"HEAD:{relative_path}").returncode == 0


def simple_frontmatter(text: str) -> dict[str, list[str]]:
    lines = text.splitlines()
    if not lines or lines[0].strip() != "---":
        return {}
    try:
        closing = next(
            index
            for index, line in enumerate(lines[1:], start=1)
            if line.strip() == "---"
        )
    except StopIteration:
        return {}
    fields: dict[str, list[str]] = {}
    for line in lines[1:closing]:
        match = re.fullmatch(r"([a-z][a-z0-9_-]*):\s*(.*?)\s*", line)
        if match is None:
            continue
        fields.setdefault(match.group(1), []).append(match.group(2).strip("'\""))
    return fields


def table_cells(line: str) -> tuple[str, ...] | None:
    stripped = line.strip()
    if not stripped.startswith("|") or not stripped.endswith("|"):
        return None
    return tuple(cell.strip() for cell in stripped[1:-1].split("|"))


def migration_surfaces(
    text: str,
) -> tuple[dict[str, tuple[str, str]], list[str]]:
    lines = text.splitlines()
    headings = [
        (index, match.group(1))
        for index, line in enumerate(lines)
        if (match := re.fullmatch(r"##\s+(.+?)\s*", line)) is not None
    ]
    if tuple(name for _, name in headings) != SC_SECTIONS:
        return {}, ["Active SC requires the exact canonical section sequence"]
    migration_heading = next(index for index, name in headings if name == "Migration inventory")
    start = migration_heading + 1
    end = next(
        (index for index in range(start, len(lines)) if re.fullmatch(r"##\s+.+", lines[index].strip())),
        len(lines),
    )
    headers = [
        index
        for index in range(start, end)
        if table_cells(lines[index]) == MIGRATION_COLUMNS
    ]
    if len(headers) != 1:
        return {}, ["Active SC requires the canonical Migration inventory table"]
    separator_index = headers[0] + 1
    separator = table_cells(lines[separator_index]) if separator_index < end else None
    if separator is None or len(separator) != len(MIGRATION_COLUMNS) or not all(
        re.fullmatch(r":?-{3,}:?", cell) for cell in separator
    ):
        return {}, ["Active SC Migration inventory has an invalid separator row"]
    surfaces: dict[str, tuple[str, str]] = {}
    errors: list[str] = []
    for line in lines[headers[0] + 2 : end]:
        cells = table_cells(line)
        if cells is None:
            if surfaces or line.strip():
                break
            continue
        if len(cells) != len(MIGRATION_COLUMNS):
            errors.append("Active SC Migration inventory has a malformed row")
            continue
        surface_cell, record_class, disposition, evidence = cells
        match = re.fullmatch(r"`([^`]+)`", surface_cell)
        if match is None:
            errors.append("Historical SC Surface must be one exact backticked path")
            continue
        surface = match.group(1)
        pure = PurePosixPath(surface)
        if pure.is_absolute() or ".." in pure.parts or re.search(r"[*?\[]", surface):
            errors.append(f"Historical SC Surface is not exact: {surface!r}")
            continue
        if surface in surfaces:
            errors.append(f"Active SC Migration inventory repeats {surface!r}")
        if record_class not in MIGRATION_CLASSES:
            errors.append(f"Active SC Migration inventory has invalid class {record_class!r}")
        if disposition not in MIGRATION_DISPOSITIONS:
            errors.append(
                f"Active SC Migration inventory has invalid disposition {disposition!r}"
            )
        if not evidence or re.search(r"\bTODO\b", evidence, re.IGNORECASE):
            errors.append(f"Active SC Migration inventory lacks evidence for {surface!r}")
        surfaces[surface] = (record_class, disposition)
    return surfaces, errors


def head_decision_resolves(repo_root: Path, decision: str) -> bool:
    blob = head_blob(repo_root, "agent/memory/decisions-index.md")
    if blob is None:
        return False
    try:
        lines = blob.decode("utf-8").splitlines()
    except UnicodeDecodeError:
        return False
    matches = [
        cells
        for line in lines
        if (cells := table_cells(line)) is not None
        and len(cells) == 4
        and cells[0] == decision
    ]
    return len(matches) == 1


def head_index_matches(
    repo_root: Path,
    sc_id: str,
    filename: str,
    fields: dict[str, list[str]],
) -> bool:
    blob = head_blob(repo_root, "agent/semantic-changes/README.md")
    if blob is None:
        return False
    try:
        lines = blob.decode("utf-8").splitlines()
    except UnicodeDecodeError:
        return False
    expected_link = f"[{sc_id}]({filename})"
    identity_rows = [
        cells
        for line in lines
        if (cells := table_cells(line)) is not None
        and len(cells) == len(SC_INDEX_COLUMNS)
        and re.match(rf"\[{re.escape(sc_id)}\]\(", cells[0])
    ]
    if len(identity_rows) != 1 or identity_rows[0][0] != expected_link:
        return False
    row = identity_rows[0]
    return row[1:] == (
        fields["status"][0],
        fields["decision"][0],
        fields["scope"][0],
        fields["updated"][0],
    )


def head_session_status(repo_root: Path, session_id: str) -> str | None:
    listing = git_bytes(
        repo_root,
        "ls-tree",
        "-r",
        "--name-only",
        "HEAD",
        "--",
        "agent/sessions",
    )
    if listing.returncode != 0:
        return None
    suffix = f"/{session_id}/session.json"
    matches = [
        line.decode("utf-8")
        for line in listing.stdout.splitlines()
        if line.decode("utf-8").endswith(suffix)
    ]
    if len(matches) != 1:
        return None
    blob = head_blob(repo_root, matches[0])
    if blob is None:
        return None
    try:
        document = json.loads(blob.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError):
        return None
    status = document.get("status") if isinstance(document, dict) else None
    return status if isinstance(status, str) else None


def committed_authorizations(
    repo_root: Path,
) -> tuple[dict[str, str], dict[str, str], list[str]]:
    listing = git_bytes(
        repo_root,
        "ls-tree",
        "-r",
        "--name-only",
        "HEAD",
        "--",
        "agent/semantic-changes",
    )
    if listing.returncode != 0:
        return {}, {}, []
    historical_owners: dict[str, str] = {}
    inventory_owners: dict[str, str] = {}
    errors: list[str] = []
    for raw_path in listing.stdout.splitlines():
        path = raw_path.decode("utf-8")
        path_match = SC_FILE_RE.fullmatch(path)
        if path_match is None:
            continue
        blob = head_blob(repo_root, path)
        if blob is None:
            continue
        try:
            text = blob.decode("utf-8")
        except UnicodeDecodeError:
            errors.append(f"{path}: Active SC is not UTF-8")
            continue
        fields = simple_frontmatter(text)
        if fields.get("status") != ["Active"]:
            continue
        missing = sorted(SC_REQUIRED_FIELDS - set(fields))
        repeated = sorted(field for field in SC_REQUIRED_FIELDS if len(fields.get(field, [])) != 1)
        if missing or repeated:
            detail = []
            if missing:
                detail.append("missing " + ", ".join(missing))
            if repeated:
                detail.append("non-scalar " + ", ".join(repeated))
            errors.append(f"{path}: Active SC frontmatter is invalid: {'; '.join(detail)}")
            continue
        sc_id = path_match.group("id")
        if fields["id"] != [sc_id]:
            errors.append(f"{path}: Active SC id does not match its path")
        decision = fields["decision"][0]
        if not DECISION_ID_RE.fullmatch(decision) or not head_decision_resolves(
            repo_root, decision
        ):
            errors.append(f"{path}: Active SC decision does not resolve in HEAD")
        if not SCOPE_RE.fullmatch(fields["scope"][0]):
            errors.append(f"{path}: Active SC scope is invalid")
        if fields["history_sync"] != ["automatic"]:
            errors.append(f"{path}: Active SC history_sync is not automatic")
        if fields["effective_revision"] != ["null"]:
            errors.append(f"{path}: Active SC effective_revision is not null")
        if fields["superseded_by"] != ["null"]:
            errors.append(f"{path}: Active SC superseded_by is not null")
        if not all(
            re.fullmatch(r"\d{4}-\d{2}-\d{2}", fields[field][0])
            for field in ("created", "updated")
        ):
            errors.append(f"{path}: Active SC dates are invalid")
        filename = PurePosixPath(path).name
        if not head_index_matches(repo_root, sc_id, filename, fields):
            errors.append(f"{path}: Active SC index row does not match in HEAD")
        sessions = fields.get("session", [])
        if len(sessions) != 1 or not SESSION_ID_RE.fullmatch(sessions[0]):
            errors.append(f"{path}: Active SC has no canonical migration session")
            continue
        if head_session_status(repo_root, sessions[0]) != "in_progress":
            errors.append(f"{path}: Active SC migration session is not in_progress in HEAD")
            continue
        surfaces, surface_errors = migration_surfaces(text)
        errors.extend(f"{path}: {error}" for error in surface_errors)
        for surface, (record_class, disposition) in surfaces.items():
            previous = inventory_owners.get(surface)
            if previous is not None:
                errors.append(f"{surface}: overlapping Active SC permits in {previous} and {path}")
            else:
                inventory_owners[surface] = path
            if record_class == "Historical" and disposition == "Pending":
                historical_owners[surface] = path
    return historical_owners, inventory_owners, errors


def terminal_session_for_path(repo_root: Path, relative_path: str) -> bool:
    parts = PurePosixPath(relative_path).parts
    if len(parts) < 6 or parts[:2] != ("agent", "sessions"):
        return False
    session_id = parts[4]
    if not SESSION_ID_RE.fullmatch(session_id):
        return False
    status = head_session_status(repo_root, session_id)
    return status is not None and status != "in_progress"


def protected_in_head(repo_root: Path, relative_path: str) -> bool:
    parts = PurePosixPath(relative_path).parts
    if parts[:3] == ("agent", "progress", "checkpoints"):
        return head_has_path(repo_root, relative_path)
    return terminal_session_for_path(repo_root, relative_path)


def normalize_relative_path(raw_path: str) -> str | None:
    path = PurePosixPath(raw_path.replace("\\", "/"))
    if path.is_absolute() or ".." in path.parts or not path.parts:
        return None
    return path.as_posix()


def cached_changes(repo_root: Path) -> tuple[list[tuple[str, list[str]]], list[str]]:
    result = git_bytes(
        repo_root,
        "diff",
        "--cached",
        "--name-status",
        "-z",
        "--find-renames=1%",
        "--find-copies",
        "--diff-filter=ACDMRT",
    )
    if result.returncode != 0:
        return [], [result.stderr.decode("utf-8", errors="replace").strip()]
    tokens = result.stdout.split(b"\0")
    if tokens and not tokens[-1]:
        tokens.pop()
    changes: list[tuple[str, list[str]]] = []
    index = 0
    while index < len(tokens):
        status = tokens[index].decode("utf-8")
        index += 1
        count = 2 if status[:1] in {"R", "C"} else 1
        if index + count > len(tokens):
            return [], ["malformed staged name-status stream"]
        paths = [tokens[index + offset].decode("utf-8") for offset in range(count)]
        index += count
        changes.append((status, paths))
    return changes, []


def revision_is_head_ancestor(repo_root: Path, revision: str) -> bool:
    if not re.fullmatch(r"[0-9a-f]{40}", revision):
        return False
    resolved = git_bytes(repo_root, "rev-parse", "--verify", "--quiet", f"{revision}^{{commit}}")
    if resolved.returncode != 0:
        return False
    return git_bytes(repo_root, "merge-base", "--is-ancestor", revision, "HEAD").returncode == 0


def check_sc_transitions(
    repo_root: Path, changes: list[tuple[str, list[str]]]
) -> list[str]:
    """Enforce monotonic SC lifecycle and real content-revision binding."""

    errors: list[str] = []
    frozen_fields = {"id", "created", "decision", "session", "scope", "history_sync"}
    allowed_transitions = {
        "Active": {"Active", "Applied"},
        "Applied": {"Superseded"},
        "Superseded": set(),
    }
    for change_status, paths in changes:
        sc_paths = [path for path in paths if SC_FILE_RE.fullmatch(path)]
        if not sc_paths:
            continue
        if change_status[:1] in {"C", "D", "R", "T"}:
            errors.append(
                "semantic-change records cannot be copied, deleted, renamed, or type-changed: "
                + ", ".join(sc_paths)
            )
            continue
        path = sc_paths[0]
        staged_blob = index_blob(repo_root, path)
        if staged_blob is None:
            errors.append(f"{path}: staged semantic-change record is unreadable")
            continue
        try:
            staged_text = staged_blob.decode("utf-8")
        except UnicodeDecodeError:
            errors.append(f"{path}: staged semantic-change record is not UTF-8")
            continue
        after = simple_frontmatter(staged_text)
        if any(len(after.get(field, [])) != 1 for field in SC_REQUIRED_FIELDS):
            errors.append(f"{path}: staged semantic-change frontmatter is incomplete")
            continue
        after_status = after["status"][0]
        before_blob = head_blob(repo_root, path)
        if before_blob is None:
            if after_status != "Active":
                errors.append(f"{path}: a new semantic change must start as Active")
            continue
        try:
            before = simple_frontmatter(before_blob.decode("utf-8"))
        except UnicodeDecodeError:
            errors.append(f"{path}: HEAD semantic-change record is not UTF-8")
            continue
        if any(len(before.get(field, [])) != 1 for field in SC_REQUIRED_FIELDS):
            errors.append(f"{path}: HEAD semantic-change frontmatter is incomplete")
            continue
        before_status = before["status"][0]
        for field in sorted(frozen_fields):
            if before[field] != after[field]:
                errors.append(f"{path}: semantic-change field {field} is immutable")
        if after_status not in allowed_transitions.get(before_status, set()):
            errors.append(
                f"{path}: semantic-change lifecycle cannot move "
                f"{before_status} -> {after_status}"
            )
        if before_status in {"Applied", "Superseded"} and after_status == before_status:
            errors.append(f"{path}: terminal semantic-change records are immutable")
        if before_status == "Applied" and after_status == "Superseded":
            if before["effective_revision"] != after["effective_revision"]:
                errors.append(f"{path}: effective_revision is immutable after application")
            successor = after["superseded_by"][0]
            current_id = after["id"][0]
            if (
                not re.fullmatch(r"SC\d{4}", successor)
                or not re.fullmatch(r"SC\d{4}", current_id)
                or int(successor[2:]) <= int(current_id[2:])
            ):
                errors.append(f"{path}: superseded_by must name a later SC identity")
        if after_status in {"Applied", "Superseded"}:
            revision = after["effective_revision"][0]
            if not revision_is_head_ancestor(repo_root, revision):
                errors.append(
                    f"{path}: effective_revision must be a full commit in the HEAD ancestry"
                )
    return errors


def check_cached(repo_root: Path) -> list[str]:
    authorized, inventory_owners, errors = committed_authorizations(repo_root)
    changes, change_errors = cached_changes(repo_root)
    errors.extend(change_errors)
    errors.extend(check_sc_transitions(repo_root, changes))
    for status, raw_paths in changes:
        paths = [normalize_relative_path(path) for path in raw_paths]
        if any(path is None for path in paths):
            errors.append(f"staged change has an invalid path: {raw_paths!r}")
            continue
        normalized = [path for path in paths if path is not None]
        protected = [path for path in normalized if protected_in_head(repo_root, path)]
        if not protected:
            continue
        if status.startswith("T"):
            errors.append(f"protected history cannot change file type: {protected[0]}")
            continue
        if status.startswith("C"):
            errors.append(f"protected history cannot be copied: {', '.join(protected)}")
            continue
        if status.startswith("R"):
            protected_missing = [path for path in protected if path not in authorized]
            owners = [inventory_owners.get(path) for path in normalized]
            if protected_missing:
                errors.append(
                    "protected history edit lacks a committed Active SC row: "
                    + ", ".join(protected_missing)
                )
            elif any(owner is None for owner in owners):
                missing = [
                    path for path, owner in zip(normalized, owners) if owner is None
                ]
                errors.append(
                    "protected history rename lacks an inventory row: "
                    + ", ".join(missing)
                )
            elif len(set(owners)) != 1:
                errors.append(
                    "protected history rename paths must belong to the same Active SC: "
                    + ", ".join(normalized)
                )
            continue
        missing = [path for path in protected if path not in authorized]
        if missing:
            errors.append(
                "protected history edit lacks a committed Active SC row: "
                + ", ".join(missing)
            )
    return errors


def check_path(repo_root: Path, raw_path: str) -> list[str]:
    relative_path = normalize_relative_path(raw_path)
    if relative_path is None:
        return [f"invalid repository-relative edit path: {raw_path!r}"]
    if not protected_in_head(repo_root, relative_path):
        return []
    authorized, _, errors = committed_authorizations(repo_root)
    if relative_path not in authorized:
        errors.append(
            f"protected history edit lacks a committed Active SC row: {relative_path}"
        )
    return errors


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("repo_root", type=Path)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--cached", action="store_true")
    mode.add_argument("--path")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    repo_root = args.repo_root.resolve()
    errors = check_cached(repo_root) if args.cached else check_path(repo_root, args.path)
    if errors:
        for error in errors:
            print(f"error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
