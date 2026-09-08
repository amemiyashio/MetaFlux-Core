#!/usr/bin/env python3
"""Behavior tests for the MetaFlux automatic acceptance controller."""

from __future__ import annotations

import copy
import importlib.util
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path


SCRIPT = Path(__file__).with_name("accept_and_advance.py").resolve()


def load_module():
    spec = importlib.util.spec_from_file_location("metaflux_accept_and_advance", SCRIPT)
    if spec is None or spec.loader is None:
        raise AssertionError("acceptance controller cannot be loaded")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


CONTROLLER = load_module()


def isolated_environment() -> dict[str, str]:
    environment = dict(os.environ)
    result = subprocess.run(
        ["git", "rev-parse", "--local-env-vars"],
        check=False,
        capture_output=True,
        text=True,
    )
    if result.returncode == 0:
        for variable in result.stdout.splitlines():
            environment.pop(variable, None)
    return environment


def git(root: Path, *arguments: str) -> str:
    result = subprocess.run(
        ["git", *arguments],
        cwd=root,
        env=isolated_environment(),
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


def work_item(identifier: str, status: str) -> str:
    return f"""---
id: {identifier}
delivery: {identifier.removeprefix('work-item-')}
milestone: milestone-0.2.0.0
status: {status}
depends_on: []
updated: 2026-09-08
---

# Fixture

## Exit Gate

Pass.
"""


def goal() -> dict:
    return {
        "schema_version": 3,
        "epoch": "epoch-0015",
        "batch": {"id": "batch-0001", "status": "open"},
        "target": {
            "milestone": "milestone-0.2.0.0",
            "work_item": "work-item-0.2.0.1",
        },
        "objective": "Accept one candidate and advance.",
        "references": [],
        "lanes": [
            {
                "id": "lane-one",
                "work_item": "work-item-0.2.0.1",
                "iteration": "iteration-0001",
                "outcome": "Deliver one candidate.",
                "status": "planned",
                "depends_on": [],
                "acceptance": ["Pass."],
            },
            {
                "id": "lane-two",
                "work_item": "work-item-0.2.0.2",
                "iteration": "iteration-0002",
                "outcome": "Deliver the dependent candidate.",
                "status": "planned",
                "depends_on": ["lane-one"],
                "acceptance": ["Pass."],
            },
        ],
    }


def repository(root: Path) -> str:
    root.mkdir()
    git(root, "init", "-q", "-b", "main")
    git(root, "config", "user.name", "Fixture")
    git(root, "config", "user.email", "fixture@example.invalid")
    write(root / "agent/goal.json", json.dumps(goal(), indent=2) + "\n")
    base = root / "agent/plan/milestone-0.2.0.0-fixture/work"
    write(base / "work-item-0.2.0.1-one.md", work_item("work-item-0.2.0.1", "Active"))
    write(base / "work-item-0.2.0.2-two.md", work_item("work-item-0.2.0.2", "Queued"))
    write(root / "product.txt", "baseline\n")
    git(root, "add", ".")
    git(root, "commit", "-q", "-m", "activate epoch")
    return git(root, "rev-parse", "HEAD")


def delivery(base: str, tip: str) -> dict:
    return {
        "schema_version": 1,
        "epoch": "epoch-0015",
        "batch": "batch-0001",
        "iteration": "iteration-0001",
        "lane": "lane-one",
        "base_revision": base,
        "tip_revision": tip,
        "tests": [{"command": "fixture-test", "status": "passed"}],
        "blockers": [],
        "roast_candidates": [],
    }


def candidate(root: Path, base: str) -> str:
    assert git(root, "rev-parse", "HEAD") == base
    write(root / "product.txt", "candidate\n")
    git(root, "add", "product.txt")
    git(root, "commit", "-q", "-m", "candidate")
    return git(root, "rev-parse", "HEAD")


def expect_error(document: dict, root: Path, code: str) -> None:
    try:
        CONTROLLER.check_delivery(document, root)
    except CONTROLLER.DiagnosticError as error:
        assert error.diagnostic.code == code, error.diagnostic
    else:
        raise AssertionError(f"expected {code}")


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="metaflux-accept-") as temporary:
        root = Path(temporary) / "in-place"
        base = repository(root)
        tip = candidate(root, base)
        document = delivery(base, tip)

        checked = CONTROLLER.check_delivery(document, root)
        assert checked.action == "in-place"
        assert checked.changed_paths == ("product.txt",)
        assert checked.next_lane == "lane-two"

        empty = copy.deepcopy(document)
        empty["base_revision"] = tip
        expect_error(empty, root, "acceptance.candidate-empty")

        failed = copy.deepcopy(document)
        failed["tests"][0]["status"] = "failed"
        expect_error(failed, root, "acceptance.not-qualified")

        blocked = copy.deepcopy(document)
        blocked["blockers"] = ["fixture blocker"]
        expect_error(blocked, root, "acceptance.not-qualified")

        before_goal = (root / "agent/goal.json").read_text(encoding="utf-8")
        before_work = next((root / "agent/plan").glob("**/work-item-0.2.0.1-*.md")).read_text(encoding="utf-8")
        try:
            CONTROLLER.advance_delivery(
                document,
                root,
                state_validator=lambda _: (_ for _ in ()).throw(RuntimeError("gate")),
            )
        except RuntimeError as error:
            assert str(error) == "gate"
        else:
            raise AssertionError("expected rollback validator failure")
        assert (root / "agent/goal.json").read_text(encoding="utf-8") == before_goal
        assert next((root / "agent/plan").glob("**/work-item-0.2.0.1-*.md")).read_text(encoding="utf-8") == before_work

        advanced = CONTROLLER.advance_delivery(document, root, state_validator=lambda _: None)
        assert advanced.action == "advanced"
        advanced_goal = json.loads((root / "agent/goal.json").read_text(encoding="utf-8"))
        assert advanced_goal["lanes"][0]["status"] == "integrated"
        assert advanced_goal["target"]["work_item"] == "work-item-0.2.0.2"
        current = next((root / "agent/plan").glob("**/work-item-0.2.0.1-*.md")).read_text(encoding="utf-8")
        following = next((root / "agent/plan").glob("**/work-item-0.2.0.2-*.md")).read_text(encoding="utf-8")
        assert "status: Complete" in current
        assert "status: Active" in following

        git(root, "add", "agent")
        git(root, "commit", "-q", "-m", "accept candidate")
        repeated = CONTROLLER.advance_delivery(document, root, state_validator=lambda _: None)
        assert repeated.action == "no-op"

        second_base = git(root, "rev-parse", "HEAD")
        write(root / "product-two.txt", "second candidate\n")
        git(root, "add", "product-two.txt")
        git(root, "commit", "-q", "-m", "second candidate")
        second_tip = git(root, "rev-parse", "HEAD")
        second = delivery(second_base, second_tip)
        second["iteration"] = "iteration-0002"
        second["lane"] = "lane-two"
        closed = CONTROLLER.advance_delivery(second, root, state_validator=lambda _: None)
        assert closed.action == "advanced"
        assert closed.next_lane is None
        closed_goal = json.loads((root / "agent/goal.json").read_text(encoding="utf-8"))
        assert closed_goal["batch"]["status"] == "integrated"
        assert all(lane["status"] == "integrated" for lane in closed_goal["lanes"])

    with tempfile.TemporaryDirectory(prefix="metaflux-accept-") as temporary:
        root = Path(temporary) / "divergent"
        base = repository(root)
        git(root, "switch", "-q", "-c", "candidate")
        tip = candidate(root, base)
        git(root, "switch", "-q", "main")
        write(root / "main.txt", "main\n")
        git(root, "add", "main.txt")
        git(root, "commit", "-q", "-m", "main change")
        document = delivery(base, tip)
        checked = CONTROLLER.check_delivery(document, root)
        assert checked.action == "merge"
        git(root, "merge", "--no-commit", "--no-ff", tip)
        advanced = CONTROLLER.advance_delivery(document, root, state_validator=lambda _: None)
        assert advanced.action == "advanced"

    with tempfile.TemporaryDirectory(prefix="metaflux-accept-") as temporary:
        root = Path(temporary) / "stale"
        base = repository(root)
        git(root, "switch", "-q", "-c", "candidate")
        tip = candidate(root, base)
        git(root, "switch", "-q", "main")
        git(root, "merge", "-q", "--no-ff", "-m", "merge candidate", tip)
        expect_error(delivery(base, tip), root, "acceptance.candidate-invalid")

    print("accept-and-advance self-tests: 9 groups passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
