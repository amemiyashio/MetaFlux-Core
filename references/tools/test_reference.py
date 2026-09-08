#!/usr/bin/env python3
"""Behavior tests for the MetaFlux reference catalog."""

from __future__ import annotations

import importlib.util
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path


SCRIPT = Path(__file__).with_name("reference.py").resolve()
SPEC = importlib.util.spec_from_file_location("metaflux_reference", SCRIPT)
if SPEC is None or SPEC.loader is None:
    raise AssertionError("reference module cannot be loaded")
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


def isolated_environment(root: Path) -> dict[str, str]:
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
    environment["GIT_ALLOW_PROTOCOL"] = "file:https:ssh:git"
    return environment


def git(root: Path, *arguments: str) -> str:
    result = subprocess.run(
        ["git", *arguments],
        cwd=root,
        env=isolated_environment(root),
        check=False,
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        raise AssertionError(f"git {' '.join(arguments)} failed: {result.stderr}")
    return result.stdout.strip()


def write(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def expect_error(action, fragment: str) -> None:
    try:
        action()
    except MODULE.ReferenceError as error:
        assert fragment in str(error), error
    else:
        raise AssertionError(f"expected ReferenceError containing {fragment!r}")


def create_upstream(root: Path) -> tuple[Path, str]:
    upstream = root / "upstream"
    upstream.mkdir()
    git(upstream, "init", "-q", "-b", "main")
    git(upstream, "config", "user.name", "Fixture")
    git(upstream, "config", "user.email", "fixture@example.invalid")
    write(upstream / "src/value.txt", "reference\n")
    write(upstream / "LICENSE", "fixture license\n")
    git(upstream, "add", ".")
    git(upstream, "commit", "-q", "-m", "fixture upstream")
    return upstream, git(upstream, "rev-parse", "HEAD")


def create_catalog(root: Path, upstream: Path, revision: str) -> Path:
    catalog = root / "catalog-repo"
    catalog.mkdir()
    git(catalog, "init", "-q", "-b", "main")
    git(catalog, "config", "user.name", "Fixture")
    git(catalog, "config", "user.email", "fixture@example.invalid")
    upstream_url = upstream.as_uri()
    write(
        catalog / ".gitmodules",
        '[submodule "fixture-v1"]\n'
        "\tpath = references/sources/fixture/v1\n"
        f"\turl = {upstream_url}\n",
    )
    document = {
        "schema_version": 1,
        "id": "fixture-v1",
        "kind": "git-submodule",
        "upstream": {
            "url": upstream_url,
            "revision": revision,
            "tag_hint": "v1",
        },
        "checkout": {"path": "references/sources/fixture/v1"},
        "license": {
            "source_path": "LICENSE",
            "url": f"{upstream_url}/LICENSE",
        },
        "purpose": "Exercise the catalog.",
        "consumers": [
            {"repository": "Fixture", "work_item": "work-item-0.0.0.1"}
        ],
        "reference_paths": ["src"],
    }
    write(
        catalog / "references/catalog/fixture/fixture-v1.json",
        json.dumps(document, indent=2) + "\n",
    )
    git(catalog, "add", ".gitmodules", "references/catalog")
    git(
        catalog,
        "update-index",
        "--add",
        "--cacheinfo",
        f"160000,{revision},references/sources/fixture/v1",
    )
    git(catalog, "commit", "-q", "-m", "fixture catalog")
    return catalog


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="metaflux-reference-") as temporary:
        root = Path(temporary)
        upstream, revision = create_upstream(root)
        catalog_root = create_catalog(root, upstream, revision)
        catalog = MODULE.ReferenceCatalog(catalog_root, allow_file_urls=True)

        entries = catalog.verify()
        assert [entry.identifier for entry in entries] == ["fixture-v1"]
        entry = catalog.entry("fixture-v1")
        assert not catalog.is_materialized(entry)
        tree = git(
            catalog_root,
            "ls-tree",
            "HEAD",
            "references/sources/fixture/v1",
        )
        assert tree.startswith("160000 commit")

        clone = root / "plain-clone"
        git(root, "clone", "-q", catalog_root.as_uri(), str(clone))
        assert not (clone / "references/sources/fixture/v1/.git").exists()

        catalog.materialize("fixture-v1")
        assert catalog.is_materialized(entry)
        catalog.materialize("fixture-v1")
        catalog.verify("fixture-v1")

        write(catalog_root / "references/sources/fixture/v1/dirty.txt", "dirty\n")
        expect_error(
            lambda: catalog.verify("fixture-v1"), "materialized source is dirty"
        )
        git(catalog_root / "references/sources/fixture/v1", "clean", "-q", "-f")

        manifest = catalog_root / "references/catalog/fixture/fixture-v1.json"
        original_manifest = manifest.read_text(encoding="utf-8")
        document = json.loads(original_manifest)
        document["upstream"]["revision"] = "0" * 40
        write(manifest, json.dumps(document))
        expect_error(lambda: catalog.verify(), "gitlink revision mismatch")
        write(manifest, original_manifest)

        modules = catalog_root / ".gitmodules"
        original_modules = modules.read_text(encoding="utf-8")
        write(modules, original_modules.replace(upstream.as_uri(), "file:///wrong"))
        expect_error(lambda: catalog.verify(), ".gitmodules URL mismatch")
        write(modules, original_modules)

        attributes = catalog_root / ".gitattributes"
        write(attributes, "references/catalog/** filter=lfs\n")
        expect_error(lambda: catalog.verify(), "Git LFS is prohibited")
        attributes.unlink()

        expect_error(lambda: catalog.entry("missing"), "unknown catalog entry")
        catalog.verify()

    print("reference catalog self-tests: 8 groups passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
