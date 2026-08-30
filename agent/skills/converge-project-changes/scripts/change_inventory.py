#!/usr/bin/env python3
"""Inventory one stable Git change set without inferring ownership."""

from __future__ import annotations

import argparse
import errno
import hashlib
import json
import os
import re
import stat
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Callable


SESSION_ID_RE = re.compile(r"^S\d{4,}-\d{8}-\d{3}-[a-z0-9][a-z0-9-]*$")
REVISION_RE = re.compile(r"^[0-9a-f]{40}$")
TERMINAL_SESSION_STATUSES = {"complete", "blocked", "abandoned"}
DIFF_STATUSES = {"A", "B", "C", "D", "M", "R", "T", "U", "X"}
GIT_CONFIG_ARGUMENTS = ("-c", "core.fsmonitor=false")


class InventoryError(RuntimeError):
    """Base class for expected command failures."""


class ScopeError(InventoryError):
    """The requested repository, revision, or session scope is invalid."""


class OperationalError(InventoryError):
    """Git or filesystem inspection could not complete."""


class DriftError(InventoryError):
    """The repository changed while it was being inspected."""


@dataclass(frozen=True)
class Scope:
    source: str
    base_revision: str
    end_revision: str
    end_source: str
    session_id: str | None = None
    session_status: str | None = None

    def as_json(self) -> dict[str, str | None]:
        return {
            "base_revision": self.base_revision,
            "end_revision": self.end_revision,
            "end_source": self.end_source,
            "session_id": self.session_id,
            "session_status": self.session_status,
            "source": self.source,
        }


@dataclass(frozen=True)
class Fingerprint:
    head_revision: str
    index_sha256: str
    tracked_worktree_sha256: str
    untracked_sha256: str


def git_environment() -> dict[str, str]:
    environment = os.environ.copy()
    environment.update(
        {
            "GIT_OPTIONAL_LOCKS": "0",
            "GIT_NO_REPLACE_OBJECTS": "1",
            "GIT_PAGER": "cat",
            "LC_ALL": "C",
        }
    )
    return environment


def run_git(repo: Path, arguments: list[str], *, allowed: set[int] | None = None) -> bytes:
    try:
        result = subprocess.run(
            ["git", *GIT_CONFIG_ARGUMENTS, *arguments],
            cwd=repo,
            env=git_environment(),
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
        )
    except OSError as exc:
        raise OperationalError(f"cannot execute git: {exc}") from exc
    accepted = {0} if allowed is None else allowed
    if result.returncode not in accepted:
        diagnostic = result.stderr.decode("utf-8", "replace").strip()
        detail = diagnostic or f"git exited with {result.returncode}"
        raise OperationalError(f"git {' '.join(arguments)}: {detail}")
    return result.stdout


def resolve_repo(candidate: Path) -> Path:
    try:
        requested = candidate.resolve(strict=True)
    except OSError as exc:
        raise ScopeError(f"repository path is not readable: {candidate}: {exc}") from exc
    try:
        output = run_git(requested, ["rev-parse", "--show-toplevel"])
    except OperationalError as exc:
        raise ScopeError(f"not a Git worktree: {requested}") from exc
    root = Path(os.fsdecode(output.rstrip(b"\n"))).resolve()
    if not root.is_dir():
        raise ScopeError(f"Git top level is not a directory: {root}")
    return root


def resolve_commit(repo: Path, revision: str, field: str) -> str:
    if not revision:
        raise ScopeError(f"{field} is empty")
    try:
        output = run_git(
            repo,
            ["rev-parse", "--verify", "--end-of-options", f"{revision}^{{commit}}"],
        )
    except OperationalError as exc:
        raise ScopeError(f"{field} does not resolve to a commit: {revision!r}") from exc
    resolved = output.decode("ascii", "strict").strip()
    if REVISION_RE.fullmatch(resolved) is None:
        raise ScopeError(f"{field} did not resolve to a 40-hex commit")
    return resolved


def current_head(repo: Path) -> str:
    return resolve_commit(repo, "HEAD", "HEAD")


def require_ancestor(repo: Path, base: str, end: str) -> None:
    try:
        result = subprocess.run(
            [
                "git",
                *GIT_CONFIG_ARGUMENTS,
                "merge-base",
                "--is-ancestor",
                base,
                end,
            ],
            cwd=repo,
            env=git_environment(),
            stdout=subprocess.DEVNULL,
            stderr=subprocess.PIPE,
            check=False,
        )
    except OSError as exc:
        raise OperationalError(f"cannot execute git merge-base: {exc}") from exc
    if result.returncode == 1:
        raise ScopeError(f"base revision {base} is not an ancestor of end revision {end}")
    if result.returncode != 0:
        diagnostic = result.stderr.decode("utf-8", "replace").strip()
        raise OperationalError(diagnostic or "git merge-base failed")


def load_session(repo: Path, session_id: str) -> tuple[dict[str, object], Path]:
    if SESSION_ID_RE.fullmatch(session_id) is None:
        raise ScopeError(f"session id has invalid shape: {session_id!r}")
    sessions_root = (repo / "agent" / "sessions").resolve()
    matches = sorted(
        repo.glob(f"agent/sessions/*/*/{session_id}/session.json"),
        key=lambda path: os.fsencode(path.as_posix()),
    )
    valid: list[Path] = []
    for path in matches:
        if path.is_symlink() or not path.is_file():
            continue
        try:
            resolved = path.resolve(strict=True)
        except OSError:
            continue
        if resolved.is_relative_to(sessions_root):
            valid.append(path)
    if len(valid) != 1:
        raise ScopeError(f"session id must resolve to one regular record: {session_id}")
    path = valid[0]
    try:
        document = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise ScopeError(f"cannot read session record {path.relative_to(repo)}: {exc}") from exc
    if not isinstance(document, dict) or document.get("id") != session_id:
        raise ScopeError(f"session record identity does not match {session_id}")
    return document, path


def resolve_scope(repo: Path, base_argument: str | None, session_id: str | None) -> Scope:
    workspace_head = current_head(repo)
    if session_id is not None:
        session, _ = load_session(repo, session_id)
        status = session.get("status")
        if status != "in_progress" and status not in TERMINAL_SESSION_STATUSES:
            raise ScopeError(f"session {session_id} has invalid status: {status!r}")
        base_value = session.get("base_revision")
        if not isinstance(base_value, str):
            raise ScopeError(f"session {session_id} has no base_revision")
        base = resolve_commit(repo, base_value, f"{session_id}.base_revision")
        final_value = session.get("final_revision")
        if status == "in_progress":
            if final_value is not None:
                raise ScopeError(f"active session {session_id} must not have final_revision")
            end = workspace_head
            end_source = "workspace-head"
        else:
            if not isinstance(final_value, str):
                raise ScopeError(f"terminal session {session_id} has no final_revision")
            end = resolve_commit(repo, final_value, f"{session_id}.final_revision")
            end_source = "session-final"
        require_ancestor(repo, base, end)
        return Scope("session", base, end, end_source, session_id, str(status))

    if base_argument is not None:
        base = resolve_commit(repo, base_argument, "base revision")
        require_ancestor(repo, base, workspace_head)
        return Scope("explicit-base", base, workspace_head, "workspace-head")

    return Scope("workspace", workspace_head, workspace_head, "workspace-head")


def split_nul(data: bytes, label: str) -> list[bytes]:
    if not data:
        return []
    if not data.endswith(b"\0"):
        raise OperationalError(f"{label} produced a truncated non-NUL-terminated stream")
    tokens = data[:-1].split(b"\0")
    if any(token == b"" for token in tokens):
        raise OperationalError(f"{label} produced an empty field")
    return tokens


def decode_path(path: bytes) -> str:
    return os.fsdecode(path)


def path_sort_key(entry: dict[str, object]) -> tuple[bytes, bytes, str, int]:
    path = os.fsencode(str(entry["path"]))
    old_path = os.fsencode(str(entry.get("old_path", "")))
    score = entry.get("score")
    return path, old_path, str(entry["status"]), -1 if score is None else int(score)


def parse_name_status(data: bytes, label: str) -> list[dict[str, object]]:
    tokens = split_nul(data, label)
    entries: list[dict[str, object]] = []
    index = 0
    while index < len(tokens):
        status_token = tokens[index]
        index += 1
        first_path: bytes | None = None
        if b"\t" in status_token:
            status_token, first_path = status_token.split(b"\t", 1)
        try:
            status_text = status_token.decode("ascii", "strict")
        except UnicodeDecodeError as exc:
            raise OperationalError(f"{label} produced a non-ASCII status") from exc
        if not status_text or status_text[0] not in DIFF_STATUSES:
            raise OperationalError(f"{label} produced invalid status {status_text!r}")
        status = status_text[0]
        score_text = status_text[1:]
        score: int | None = None
        if score_text:
            if status not in {"R", "C"} or not score_text.isdigit():
                raise OperationalError(f"{label} produced invalid status {status_text!r}")
            score = int(score_text)
            if score < 0 or score > 100:
                raise OperationalError(f"{label} produced invalid similarity score {score}")

        if status in {"R", "C"}:
            if first_path is None:
                if index + 1 >= len(tokens):
                    raise OperationalError(f"{label} truncated a rename/copy entry")
                old_path = tokens[index]
                path = tokens[index + 1]
                index += 2
            else:
                if index >= len(tokens):
                    raise OperationalError(f"{label} truncated a rename/copy entry")
                old_path = first_path
                path = tokens[index]
                index += 1
            entry: dict[str, object] = {
                "old_path": decode_path(old_path),
                "path": decode_path(path),
                "score": score,
                "status": status,
            }
        else:
            if first_path is None:
                if index >= len(tokens):
                    raise OperationalError(f"{label} truncated a path entry")
                path = tokens[index]
                index += 1
            else:
                path = first_path
            entry = {"path": decode_path(path), "status": status}
        entries.append(entry)
    return sorted(entries, key=path_sort_key)


def inventory_diff(repo: Path, arguments: list[str], label: str) -> list[dict[str, object]]:
    output = run_git(
        repo,
        [
            "diff",
            "--no-ext-diff",
            "--no-textconv",
            "--name-status",
            "-z",
            "--find-renames=50%",
            "--find-copies=50%",
            "--find-copies-harder",
            "--ignore-submodules=none",
            *arguments,
            "--",
        ],
    )
    return parse_name_status(output, label)


def inventory_untracked(repo: Path) -> list[dict[str, object]]:
    output = run_git(repo, ["ls-files", "--others", "--exclude-standard", "-z"])
    entries = [
        {"path": decode_path(path), "status": "?"}
        for path in split_nul(output, "untracked inventory")
    ]
    return sorted(entries, key=path_sort_key)


def collect_layers(repo: Path, scope: Scope, workspace_head: str) -> dict[str, object]:
    return {
        "committed": inventory_diff(
            repo,
            [scope.base_revision, scope.end_revision],
            "committed inventory",
        ),
        "staged": inventory_diff(
            repo,
            ["--cached", workspace_head],
            "staged inventory",
        ),
        "unstaged": inventory_diff(repo, [], "unstaged inventory"),
        "untracked": inventory_untracked(repo),
    }


def hash_git_stream(repo: Path, arguments: list[str], label: str) -> str:
    try:
        process = subprocess.Popen(
            ["git", *GIT_CONFIG_ARGUMENTS, *arguments],
            cwd=repo,
            env=git_environment(),
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
    except OSError as exc:
        raise OperationalError(f"cannot execute git for {label}: {exc}") from exc
    assert process.stdout is not None and process.stderr is not None
    try:
        digest = hashlib.sha256()
        while True:
            chunk = process.stdout.read(1024 * 1024)
            if not chunk:
                break
            digest.update(chunk)
        diagnostic = process.stderr.read()
        returncode = process.wait()
    finally:
        process.stdout.close()
        process.stderr.close()
        if process.poll() is None:
            process.kill()
            process.wait()
    if returncode != 0:
        detail = diagnostic.decode("utf-8", "replace").strip()
        raise OperationalError(f"{label}: {detail or f'git exited with {returncode}'}")
    return digest.hexdigest()


def hash_field(digest: "hashlib._Hash", value: bytes) -> None:
    digest.update(len(value).to_bytes(8, "big"))
    digest.update(value)


def stable_stat(path: Path) -> os.stat_result:
    try:
        return path.lstat()
    except FileNotFoundError as exc:
        raise DriftError(f"untracked path disappeared during scan: {path.name}") from exc
    except OSError as exc:
        raise OperationalError(f"cannot inspect untracked path {path.name}: {exc}") from exc


def metadata_key(value: os.stat_result) -> tuple[int, int, int, int, int, int, int]:
    return (
        value.st_mode,
        value.st_dev,
        value.st_ino,
        value.st_size,
        value.st_mtime_ns,
        value.st_ctime_ns,
        value.st_nlink,
    )


def hash_untracked(repo: Path) -> str:
    paths = split_nul(
        run_git(repo, ["ls-files", "--others", "--exclude-standard", "-z"]),
        "untracked fingerprint",
    )
    digest = hashlib.sha256()
    for raw_path in sorted(paths):
        hash_field(digest, raw_path)
        path = repo / decode_path(raw_path)
        before = stable_stat(path)
        hash_field(digest, repr(metadata_key(before)).encode("ascii"))
        kind = stat.S_IFMT(before.st_mode)
        if stat.S_ISREG(before.st_mode):
            no_follow = getattr(os, "O_NOFOLLOW", None)
            if no_follow is None:
                raise OperationalError("platform does not provide O_NOFOLLOW")
            flags = os.O_RDONLY | no_follow
            flags |= getattr(os, "O_CLOEXEC", 0) | getattr(os, "O_NONBLOCK", 0)
            try:
                descriptor = os.open(path, flags)
            except OSError as exc:
                if exc.errno in {errno.ELOOP, errno.ENOENT, errno.ENOTDIR, errno.ENXIO}:
                    raise DriftError(
                        f"untracked path changed during scan: {decode_path(raw_path)}"
                    ) from exc
                raise OperationalError(
                    f"cannot read untracked path {decode_path(raw_path)}: {exc}"
                ) from exc
            try:
                opened = os.fstat(descriptor)
                if not stat.S_ISREG(opened.st_mode) or metadata_key(opened) != metadata_key(
                    before
                ):
                    raise DriftError(
                        f"untracked path changed during scan: {decode_path(raw_path)}"
                    )
                while True:
                    chunk = os.read(descriptor, 1024 * 1024)
                    if not chunk:
                        break
                    digest.update(chunk)
                if metadata_key(os.fstat(descriptor)) != metadata_key(before):
                    raise DriftError(
                        f"untracked path changed during scan: {decode_path(raw_path)}"
                    )
            except OSError as exc:
                raise OperationalError(
                    f"cannot read untracked path {decode_path(raw_path)}: {exc}"
                ) from exc
            finally:
                os.close(descriptor)
        elif stat.S_ISLNK(before.st_mode):
            try:
                target = os.readlink(os.fsencode(path))
            except FileNotFoundError as exc:
                raise DriftError(
                    f"untracked symlink disappeared during scan: {decode_path(raw_path)}"
                ) from exc
            except OSError as exc:
                raise DriftError(
                    f"untracked symlink changed during scan: {decode_path(raw_path)}"
                ) from exc
            hash_field(digest, target)
        elif stat.S_ISDIR(before.st_mode):
            raise OperationalError(
                "cannot establish a content fingerprint for opaque untracked "
                f"directory: {decode_path(raw_path)}"
            )
        else:
            raise OperationalError(
                "cannot establish a content fingerprint for special untracked "
                f"path: {decode_path(raw_path)} (mode {kind})"
            )
        after = stable_stat(path)
        if metadata_key(before) != metadata_key(after):
            raise DriftError(f"untracked path changed during scan: {decode_path(raw_path)}")
    return digest.hexdigest()


def fingerprint(repo: Path) -> Fingerprint:
    head = current_head(repo)
    return Fingerprint(
        head_revision=head,
        index_sha256=hash_git_stream(
            repo,
            ["ls-files", "--stage", "-v", "-z"],
            "index fingerprint",
        ),
        tracked_worktree_sha256=hash_git_stream(
            repo,
            [
                "diff",
                "--binary",
                "--full-index",
                "--no-ext-diff",
                "--no-textconv",
                "--ignore-submodules=none",
                "--",
            ],
            "tracked worktree fingerprint",
        ),
        untracked_sha256=hash_untracked(repo),
    )


LayerCollector = Callable[[Path, Scope, str], dict[str, object]]
ScopeResolver = Callable[[Path, str | None, str | None], Scope]


def collect_stable_inventory(
    repo: Path,
    *,
    base_argument: str | None = None,
    session_id: str | None = None,
    collector: LayerCollector = collect_layers,
    resolver: ScopeResolver = resolve_scope,
) -> dict[str, object]:
    before = fingerprint(repo)
    scope = resolver(repo, base_argument, session_id)
    if scope.end_source == "workspace-head" and scope.end_revision != before.head_revision:
        raise DriftError("HEAD changed while the change-set endpoint was resolved")
    layers = collector(repo, scope, before.head_revision)
    try:
        closing_scope = resolve_scope(repo, base_argument, session_id)
    except ScopeError as exc:
        raise DriftError(
            "change-set scope became invalid while the inventory was being collected"
        ) from exc
    closing_layers = collect_layers(repo, closing_scope, before.head_revision)
    after = fingerprint(repo)
    if before != after:
        raise DriftError("repository changed while the inventory was being collected")
    if scope != closing_scope:
        raise DriftError("change-set scope changed while the inventory was being collected")
    if layers != closing_layers:
        raise DriftError("change layers changed while the inventory was being collected")
    return {
        "changes": layers,
        "schema_version": 1,
        "scope": scope.as_json(),
        "stable": True,
        "workspace_revision": before.head_revision,
    }


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=Path.cwd(),
        help="repository root or a path inside it (default: current directory)",
    )
    scope = parser.add_mutually_exclusive_group()
    scope.add_argument("--base", help="ancestor revision compared to current HEAD")
    scope.add_argument("--session", help="exact MetaFlux session id")
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    arguments = parse_args(sys.argv[1:] if argv is None else argv)
    try:
        repo = resolve_repo(arguments.repo_root)
        document = collect_stable_inventory(
            repo,
            base_argument=arguments.base,
            session_id=arguments.session,
        )
    except ScopeError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 3
    except DriftError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 5
    except OperationalError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 4
    json.dump(document, sys.stdout, ensure_ascii=True, indent=2, sort_keys=True)
    print()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
