#!/usr/bin/env python3
"""List, materialize, and verify exact MetaFlux reference gitlinks."""

from __future__ import annotations

import argparse
import configparser
import json
import os
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path, PurePosixPath
from typing import Any, Sequence


ENTRY_ID_RE = re.compile(r"[a-z0-9]+(?:[.-][a-z0-9]+)*")
REVISION_RE = re.compile(r"[0-9a-f]{40}")
MANIFEST_FIELDS = {
    "schema_version",
    "id",
    "kind",
    "upstream",
    "checkout",
    "license",
    "purpose",
    "consumers",
    "reference_paths",
}
FORBIDDEN_TRACKED_SUFFIXES = {
    ".a",
    ".bz2",
    ".cubin",
    ".deb",
    ".dll",
    ".dylib",
    ".gz",
    ".o",
    ".ptx",
    ".rpm",
    ".so",
    ".tar",
    ".whl",
    ".xz",
    ".zip",
}


class ReferenceError(ValueError):
    pass


@dataclass(frozen=True)
class Entry:
    identifier: str
    upstream_url: str
    revision: str
    tag_hint: str
    checkout_path: str
    license_path: str
    license_url: str
    purpose: str
    consumers: tuple[tuple[str, str], ...]
    reference_paths: tuple[str, ...]
    manifest_path: Path


def isolated_git_environment(root: Path) -> dict[str, str]:
    environment = dict(os.environ)
    result = subprocess.run(
        ["git", "rev-parse", "--local-env-vars"],
        cwd=root,
        env=environment,
        check=False,
        capture_output=True,
        text=True,
    )
    if result.returncode == 0:
        for variable in result.stdout.splitlines():
            environment.pop(variable, None)
    return environment


def run_git(
    root: Path,
    *arguments: str,
    allow_file_urls: bool = False,
    check: bool = True,
) -> subprocess.CompletedProcess[str]:
    environment = isolated_git_environment(root)
    if allow_file_urls:
        environment["GIT_ALLOW_PROTOCOL"] = "file:https:ssh:git"
    result = subprocess.run(
        ["git", *arguments],
        cwd=root,
        env=environment,
        check=False,
        capture_output=True,
        text=True,
    )
    if check and result.returncode != 0:
        detail = result.stderr.strip() or result.stdout.strip() or "Git command failed"
        raise ReferenceError(detail)
    return result


def require_string(value: Any, where: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise ReferenceError(f"{where} must be a non-empty string")
    return value


def relative_path(value: Any, where: str, *, prefix: str | None = None) -> str:
    text = require_string(value, where)
    path = PurePosixPath(text)
    if path.is_absolute() or ".." in path.parts or "." in path.parts:
        raise ReferenceError(f"{where} must be a normalized relative path")
    if prefix is not None:
        prefix_parts = PurePosixPath(prefix).parts
        if (
            tuple(path.parts[: len(prefix_parts)]) != tuple(prefix_parts)
            or len(path.parts) == len(prefix_parts)
        ):
            raise ReferenceError(f"{where} must remain under {prefix}/")
    return text


class ReferenceCatalog:
    def __init__(self, root: Path, *, allow_file_urls: bool = False):
        self.root = root.resolve()
        self.allow_file_urls = allow_file_urls

    def manifest_paths(self) -> list[Path]:
        return sorted(
            path
            for path in (self.root / "references/catalog").glob("**/*.json")
            if path.name != "schema-v1.json"
        )

    def load_entries(self) -> dict[str, Entry]:
        entries: dict[str, Entry] = {}
        for path in self.manifest_paths():
            try:
                document = json.loads(path.read_text(encoding="utf-8"))
            except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
                raise ReferenceError(f"{path}: cannot read manifest: {error}") from error
            entry = self.validate_manifest(document, path)
            if entry.identifier in entries:
                raise ReferenceError(f"duplicate catalog id: {entry.identifier}")
            entries[entry.identifier] = entry
        if not entries:
            raise ReferenceError("reference catalog is empty")
        return entries

    def validate_manifest(self, document: Any, path: Path) -> Entry:
        if not isinstance(document, dict) or set(document) != MANIFEST_FIELDS:
            raise ReferenceError(f"{path}: manifest fields differ from schema v1")
        if document.get("schema_version") != 1:
            raise ReferenceError(f"{path}: schema_version must be 1")
        identifier = require_string(document.get("id"), f"{path}: id")
        if ENTRY_ID_RE.fullmatch(identifier) is None:
            raise ReferenceError(f"{path}: invalid catalog id")
        if document.get("kind") != "git-submodule":
            raise ReferenceError(f"{path}: kind must be git-submodule")

        upstream = document.get("upstream")
        if not isinstance(upstream, dict) or set(upstream) != {"url", "revision", "tag_hint"}:
            raise ReferenceError(f"{path}: upstream fields differ from schema v1")
        upstream_url = require_string(upstream.get("url"), f"{path}: upstream.url")
        if not upstream_url.startswith("https://") and not (
            self.allow_file_urls and upstream_url.startswith("file://")
        ):
            raise ReferenceError(f"{path}: upstream.url must use HTTPS")
        revision = require_string(upstream.get("revision"), f"{path}: upstream.revision")
        if REVISION_RE.fullmatch(revision) is None:
            raise ReferenceError(f"{path}: upstream.revision must be a full SHA-1 commit")
        tag_hint = require_string(upstream.get("tag_hint"), f"{path}: upstream.tag_hint")

        checkout = document.get("checkout")
        if not isinstance(checkout, dict) or set(checkout) != {"path"}:
            raise ReferenceError(f"{path}: checkout must contain only path")
        checkout_path = relative_path(
            checkout.get("path"),
            f"{path}: checkout.path",
            prefix="references/sources",
        )

        license_value = document.get("license")
        if not isinstance(license_value, dict) or set(license_value) != {"source_path", "url"}:
            raise ReferenceError(f"{path}: license fields differ from schema v1")
        license_path = relative_path(
            license_value.get("source_path"), f"{path}: license.source_path"
        )
        license_url = require_string(license_value.get("url"), f"{path}: license.url")
        if not license_url.startswith("https://") and not (
            self.allow_file_urls and license_url.startswith("file://")
        ):
            raise ReferenceError(f"{path}: license.url must use HTTPS")
        if not self.allow_file_urls and revision not in license_url:
            raise ReferenceError(f"{path}: license.url must be pinned to upstream.revision")

        purpose = require_string(document.get("purpose"), f"{path}: purpose")
        consumers_value = document.get("consumers")
        if not isinstance(consumers_value, list) or not consumers_value:
            raise ReferenceError(f"{path}: consumers must be a non-empty list")
        consumers: list[tuple[str, str]] = []
        for index, consumer in enumerate(consumers_value):
            if not isinstance(consumer, dict) or set(consumer) != {"repository", "work_item"}:
                raise ReferenceError(f"{path}: consumers[{index}] has invalid fields")
            consumers.append(
                (
                    require_string(
                        consumer.get("repository"),
                        f"{path}: consumers[{index}].repository",
                    ),
                    require_string(
                        consumer.get("work_item"),
                        f"{path}: consumers[{index}].work_item",
                    ),
                )
            )

        reference_paths_value = document.get("reference_paths")
        if not isinstance(reference_paths_value, list) or not reference_paths_value:
            raise ReferenceError(f"{path}: reference_paths must be a non-empty list")
        reference_paths = tuple(
            relative_path(value, f"{path}: reference_paths[{index}]")
            for index, value in enumerate(reference_paths_value)
        )
        if len(reference_paths) != len(set(reference_paths)):
            raise ReferenceError(f"{path}: reference_paths contains duplicates")

        return Entry(
            identifier=identifier,
            upstream_url=upstream_url,
            revision=revision,
            tag_hint=tag_hint,
            checkout_path=checkout_path,
            license_path=license_path,
            license_url=license_url,
            purpose=purpose,
            consumers=tuple(consumers),
            reference_paths=reference_paths,
            manifest_path=path,
        )

    def entry(self, identifier: str) -> Entry:
        entries = self.load_entries()
        try:
            return entries[identifier]
        except KeyError as error:
            raise ReferenceError(f"unknown catalog entry: {identifier}") from error

    def gitmodules(self) -> dict[str, tuple[str, str]]:
        parser = configparser.ConfigParser(interpolation=None)
        parser.read(self.root / ".gitmodules", encoding="utf-8")
        modules: dict[str, tuple[str, str]] = {}
        for section in parser.sections():
            if not section.startswith('submodule "') or not section.endswith('"'):
                raise ReferenceError(f"invalid .gitmodules section: {section}")
            name = section[len('submodule "') : -1]
            path = parser.get(section, "path", fallback="").strip()
            url = parser.get(section, "url", fallback="").strip()
            if not path or not url or path in modules:
                raise ReferenceError(f"invalid .gitmodules entry: {name}")
            if path.startswith("references/sources/"):
                modules[path] = (name, url)
        return modules

    def gitlinks(self) -> dict[str, str]:
        result = run_git(
            self.root, "ls-files", "--stage", "--", "references/sources"
        )
        gitlinks: dict[str, str] = {}
        for line in result.stdout.splitlines():
            metadata, path = line.split("\t", 1)
            mode, object_id, stage = metadata.split()
            if mode != "160000" or stage != "0":
                raise ReferenceError(f"tracked reference source is not a gitlink: {path}")
            gitlinks[path] = object_id
        return gitlinks

    def verify_static(self) -> dict[str, Entry]:
        entries = self.load_entries()
        modules = self.gitmodules()
        gitlinks = self.gitlinks()
        entry_paths = {entry.checkout_path for entry in entries.values()}
        if set(modules) != entry_paths:
            raise ReferenceError(".gitmodules paths differ from catalog entries")
        if set(gitlinks) != entry_paths:
            raise ReferenceError("tracked gitlinks differ from catalog entries")
        for entry in entries.values():
            _, module_url = modules[entry.checkout_path]
            if module_url != entry.upstream_url:
                raise ReferenceError(f"{entry.identifier}: .gitmodules URL mismatch")
            if gitlinks[entry.checkout_path] != entry.revision:
                raise ReferenceError(f"{entry.identifier}: gitlink revision mismatch")

        tracked = run_git(self.root, "ls-files", "--", "references").stdout.splitlines()
        for name in tracked:
            if Path(name).suffix.lower() in FORBIDDEN_TRACKED_SUFFIXES:
                raise ReferenceError(f"forbidden tracked reference payload: {name}")
        if tracked:
            attributes = run_git(
                self.root, "check-attr", "filter", "--", *tracked
            ).stdout.splitlines()
            if any(line.endswith(": filter: lfs") for line in attributes):
                raise ReferenceError("Git LFS is prohibited for reference sources")
        return entries

    def is_materialized(self, entry: Entry) -> bool:
        checkout = self.root / entry.checkout_path
        return checkout.is_dir() and (checkout / ".git").exists()

    def verify_materialized(self, entry: Entry) -> None:
        checkout = self.root / entry.checkout_path
        if not self.is_materialized(entry):
            return
        head = run_git(checkout, "rev-parse", "HEAD").stdout.strip()
        if head != entry.revision:
            raise ReferenceError(f"{entry.identifier}: materialized revision mismatch")
        origin = run_git(
            checkout, "config", "--get", "remote.origin.url"
        ).stdout.strip()
        if origin != entry.upstream_url:
            raise ReferenceError(f"{entry.identifier}: materialized origin mismatch")
        dirty = run_git(
            checkout, "status", "--porcelain", "--untracked-files=all"
        ).stdout
        if dirty:
            raise ReferenceError(f"{entry.identifier}: materialized source is dirty")
        branch = run_git(checkout, "symbolic-ref", "-q", "HEAD", check=False)
        if branch.returncode == 0:
            raise ReferenceError(f"{entry.identifier}: materialized source must be detached")
        for path in entry.reference_paths:
            if not (checkout / path).exists():
                raise ReferenceError(
                    f"{entry.identifier}: reference path is absent: {path}"
                )
        if not (checkout / entry.license_path).is_file():
            raise ReferenceError(f"{entry.identifier}: license source path is absent")

    def verify(self, identifier: str | None = None) -> list[Entry]:
        entries = self.verify_static()
        if identifier is None:
            selected = list(entries.values())
        else:
            try:
                selected = [entries[identifier]]
            except KeyError as error:
                raise ReferenceError(f"unknown catalog entry: {identifier}") from error
        for entry in selected:
            self.verify_materialized(entry)
        return selected

    def materialize(self, identifier: str) -> Entry:
        entries = self.verify_static()
        try:
            entry = entries[identifier]
        except KeyError as error:
            raise ReferenceError(f"unknown catalog entry: {identifier}") from error
        if self.is_materialized(entry):
            self.verify_materialized(entry)
            return entry
        run_git(
            self.root,
            "submodule",
            "update",
            "--init",
            "--filter=blob:none",
            "--depth",
            "1",
            "--",
            entry.checkout_path,
            allow_file_urls=self.allow_file_urls,
        )
        self.verify_materialized(entry)
        return entry


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(description=__doc__)
    result.add_argument(
        "--root", type=Path, default=Path(__file__).resolve().parents[2]
    )
    subcommands = result.add_subparsers(dest="command", required=True)
    subcommands.add_parser("list", help="List catalog entries")
    status = subcommands.add_parser(
        "status", help="Show local materialization status"
    )
    status.add_argument("entry")
    materialize = subcommands.add_parser(
        "materialize", help="Materialize one exact gitlink"
    )
    materialize.add_argument("entry")
    verify = subcommands.add_parser(
        "verify", help="Verify one entry or the complete catalog"
    )
    verify.add_argument("entry", nargs="?")
    return result


def main(arguments: Sequence[str] | None = None) -> int:
    options = parser().parse_args(arguments)
    catalog = ReferenceCatalog(options.root)
    try:
        if options.command == "list":
            for entry in catalog.load_entries().values():
                print(
                    f"{entry.identifier}\t{entry.revision}\t{entry.upstream_url}"
                )
        elif options.command == "status":
            entry = catalog.entry(options.entry)
            catalog.verify_static()
            catalog.verify_materialized(entry)
            state = (
                "materialized" if catalog.is_materialized(entry) else "not-materialized"
            )
            print(
                f"reference status: {entry.identifier} {state} "
                f"revision={entry.revision}"
            )
        elif options.command == "materialize":
            entry = catalog.materialize(options.entry)
            print(
                f"reference materialized: {entry.identifier} "
                f"revision={entry.revision} path={entry.checkout_path}"
            )
        else:
            selected = catalog.verify(options.entry)
            print(f"reference catalog: ok ({len(selected)} entries)")
    except ReferenceError as error:
        print(f"reference catalog: invalid: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
