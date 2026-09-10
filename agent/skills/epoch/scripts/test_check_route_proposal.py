#!/usr/bin/env python3
"""Behavior tests for the transient epoch proposal guard."""

from __future__ import annotations

import importlib.util
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path


SCRIPT = Path(__file__).with_name("check_route_proposal.py")


def load_module():
    spec = importlib.util.spec_from_file_location("metaflux_route_proposal", SCRIPT)
    if spec is None or spec.loader is None:
        raise AssertionError("proposal guard cannot be loaded")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


GUARD = load_module()


def git(root: Path, *arguments: str) -> str:
    environment = dict(os.environ)
    for variable in subprocess.run(
        ["git", "rev-parse", "--local-env-vars"],
        check=True,
        capture_output=True,
        text=True,
    ).stdout.splitlines():
        environment.pop(variable, None)
    result = subprocess.run(
        ["git", *arguments],
        cwd=root,
        env=environment,
        check=True,
        capture_output=True,
        text=True,
    )
    return result.stdout.strip()


def write(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def repository(root: Path) -> str:
    root.mkdir()
    git(root, "init", "-q")
    git(root, "config", "user.name", "Fixture")
    git(root, "config", "user.email", "fixture@example.invalid")
    write(root / "agent/goal.json", '{"epoch":"epoch-0011"}\n')
    git(root, "add", ".")
    git(root, "commit", "-q", "-m", "baseline")
    return git(root, "rev-parse", "HEAD")


def route() -> dict:
    return {
        "objective": "Run a stock client through the daemon.",
        "observable_success": "The real-client gate completes.",
        "nodes": [
            {
                "id": "reference.pytorch",
                "disposition": "keep",
                "depends_on": [],
            },
            {
                "id": "decision.surface",
                "disposition": "rewrite",
                "depends_on": ["reference.pytorch"],
            },
            {
                "id": "lane.baseline",
                "disposition": "reorder",
                "depends_on": ["decision.surface"],
            },
        ],
    }


def proposal(revision: str, *, semantic_change: bool = True) -> dict:
    return {
        "schema_version": 1,
        "focus": "Stock client compatibility",
        "candidates": [],
        "baseline_revision": revision,
        "baseline_epoch": "epoch-0011",
        "proposed_epoch": "epoch-0012" if semantic_change else "epoch-0011",
        "semantic_change": semantic_change,
        "route": route(),
    }


def expect_error(document: dict, root: Path, fragment: str, *, confirmed: bool) -> None:
    try:
        GUARD.evaluate(document, root, confirmed=confirmed)
    except GUARD.ProposalError as error:
        assert fragment in str(error), error
    else:
        raise AssertionError(f"expected error containing {fragment!r}")


def main() -> int:
    candidates = {
        "schema_version": 1,
        "focus": None,
        "candidates": [
            {
                "objective": "Candidate A",
                "observable_success": "Signal A",
            },
            {
                "objective": "Candidate B",
                "observable_success": "Signal B",
            },
        ],
        "route": None,
    }
    assert GUARD.evaluate(candidates, Path("."), confirmed=False) == "select-target"

    with tempfile.TemporaryDirectory(prefix="metaflux-replan-") as temporary:
        root = Path(temporary) / "repo"
        revision = repository(root)
        document = proposal(revision)

        before = git(root, "rev-parse", "HEAD")
        assert GUARD.evaluate(document, root, confirmed=False) == "await-confirmation"
        assert git(root, "rev-parse", "HEAD") == before
        assert not git(root, "status", "--porcelain", "--untracked-files=all")

        no_op = proposal(revision, semantic_change=False)
        assert GUARD.evaluate(no_op, root, confirmed=True) == "no-op"

        assert GUARD.evaluate(document, root, confirmed=True) == "ready-to-govern"

        cyclic = proposal(revision)
        cyclic["route"]["nodes"][0]["depends_on"] = ["lane.baseline"]
        expect_error(cyclic, root, "contains a cycle", confirmed=True)

        write(root / "changed.txt", "changed\n")
        git(root, "add", "changed.txt")
        git(root, "commit", "-q", "-m", "changed baseline")
        expect_error(document, root, "baseline changed", confirmed=True)

    print("replan roadmap proposal self-tests: 6 groups passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
