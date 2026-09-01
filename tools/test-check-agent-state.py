#!/usr/bin/env python3
"""Focused self-tests for check-agent-state.py."""

from __future__ import annotations

import importlib.util
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path


SCRIPT = Path(__file__).with_name("check-agent-state.py").resolve()
DETECTOR_SCRIPT = (
    SCRIPT.parents[1]
    / "agent"
    / "skills"
    / "detect-agent-tool"
    / "scripts"
    / "detect_agent_tool.py"
)


def load_module():
    spec = importlib.util.spec_from_file_location("metaflux_agent_state", SCRIPT)
    if spec is None or spec.loader is None:
        raise AssertionError("state checker cannot be loaded")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


STATE = load_module()


def write(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def git(root: Path, *arguments: str) -> str:
    result = subprocess.run(
        ["git", *arguments],
        cwd=root,
        check=False,
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        raise AssertionError(f"git {' '.join(arguments)} failed: {result.stderr}")
    return result.stdout.strip()


def goal() -> dict:
    return {
        "schema_version": 1,
        "epoch": "epoch-0002",
        "batch": {"id": "batch-0001", "status": "open"},
        "target": {
            "milestone": "milestone-0.1.0.0",
            "work_item": "work-item-0.1.0.1",
        },
        "objective": "Deliver one candidate.",
        "lanes": [
            {
                "id": "lane-one",
                "iteration": "iteration-0001",
                "outcome": "Produce one tested change.",
                "status": "planned",
                "depends_on": [],
                "acceptance": ["Focused tests pass."],
            }
        ],
    }


def create_fixture(root: Path) -> None:
    write(
        root / "agent/plan/milestone-0.1.0.0-core/plan.md",
        """---
id: milestone-0.1.0.0
delivery: 0.1.0.0
release: v0.1.0
status: Active
depends_on: []
---

# Core

## Decisions to Close

None.
""",
    )
    write(
        root / "agent/plan/milestone-0.1.0.0-core/work/work-item-0.1.0.1-one.md",
        """---
id: work-item-0.1.0.1
delivery: 0.1.0.1
milestone: milestone-0.1.0.0
status: Active
depends_on: []
---

# One

## Exit Gate

Pass.
""",
    )
    write(root / "agent/goal.json", json.dumps(goal(), indent=2) + "\n")
    write(
        root / "agent/memory/decisions-index.md",
        "| ID | Topic | Canonical source | Source status |\n"
        "| --- | --- | --- | --- |\n"
        "| decision-0033 | Current execution | [goal](../goal.json) | Verified |\n"
        "| decision-0034 | Agent-tool detection | [tool](../skills/detect-agent-tool/SKILL.md) | Verified |\n",
    )
    write(root / "agent/experience/README.md", "# Experience\n")
    write(
        root / "agent/skills/start-work/SKILL.md",
        "---\nname: start-work\ndescription: Start current work.\n---\n\n# Start\n",
    )
    write(
        root / "agent/skills/start-work/agents/openai.yaml",
        "interface:\n"
        "  display_name: \"Start Work\"\n"
        "  short_description: \"Start one current repository work iteration\"\n"
        "  default_prompt: \"Use $start-work to begin.\"\n",
    )
    write(
        root / "agent/skills/detect-agent-tool/SKILL.md",
        "---\n"
        "name: detect-agent-tool\n"
        "description: Detect the current executable agent tool.\n"
        "---\n\n"
        "# Detect Agent Tool\n",
    )
    write(
        root / "agent/skills/detect-agent-tool/agents/openai.yaml",
        "interface:\n"
        "  display_name: \"Detect Agent Tool\"\n"
        "  short_description: \"Detect the current executable agent tool\"\n"
        "  default_prompt: \"Use $detect-agent-tool to report the tool.\"\n",
    )
    write(
        root
        / "agent/skills/detect-agent-tool/scripts/detect_agent_tool.py",
        DETECTOR_SCRIPT.read_text(encoding="utf-8"),
    )
    write(
        root / "agent/skills/README.md",
        "## Index\n\n"
        "| Skill | Status | Use when |\n"
        "| --- | --- | --- |\n"
        "| [start-work](start-work/SKILL.md) | Active | Starting |\n"
        "| [detect-agent-tool](detect-agent-tool/SKILL.md) | Active | Detecting |\n",
    )
    write(
        root / "agent/skills/trigger-evals.json",
        json.dumps(
            {
                "domain_skills": sorted(STATE.DOMAIN_SKILL_SLUGS),
                "workflow_skills": sorted(STATE.WORKFLOW_SKILL_SLUGS),
                "cases": [
                    {
                        "expected_skills": ["roast"],
                        "forbidden_skills": [],
                    }
                ],
            }
        ),
    )
    (root / ".agents").mkdir()
    (root / ".agents/skills").symlink_to("../agent/skills")


def errors(root: Path) -> list[str]:
    checker = STATE.Checker(root)
    checker.run()
    return checker.errors


def test_parsers() -> None:
    fields = STATE.parse_frontmatter("---\nid: value\ndepends_on: [one, two]\n---\n")
    assert fields == {"id": "value", "depends_on": ["one", "two"]}
    assert STATE.coordinate("1.2.3.4") == (1, 2, 3, 4)
    assert STATE.coordinate("1.2") is None


def test_cycle_detection() -> None:
    assert STATE.find_cycle({"a": ["b"], "b": []}) is None
    assert STATE.find_cycle({"a": ["b"], "b": ["a"]}) == ["a", "b", "a"]


def test_goal_variants(root: Path) -> None:
    create_fixture(root)
    checker = STATE.Checker(root)
    checker.validate_plans()
    checker.validate_goal()
    assert not checker.errors

    path = root / "agent/goal.json"
    document = goal()
    document["epoch"] = "E" + "0001"
    write(path, json.dumps(document))
    assert any("epoch must match" in error for error in errors(root))

    document = goal()
    document["lanes"][0]["status"] = "in_progress"
    write(path, json.dumps(document))
    assert any("status is invalid" in error for error in errors(root))

    document = goal()
    second = dict(document["lanes"][0])
    second["id"] = "lane-two"
    second["depends_on"] = ["lane-missing"]
    document["lanes"].append(second)
    write(path, json.dumps(document))
    found = errors(root)
    assert any("duplicate iteration" in error for error in found)
    assert any("unresolved dependency" in error for error in found)


def test_plan_and_legacy(root: Path) -> None:
    create_fixture(root)
    work = root / "agent/plan/milestone-0.1.0.0-core/work/work-item-0.1.0.1-one.md"
    write(work, work.read_text(encoding="utf-8").replace("## Exit Gate", "## Acceptance"))
    assert any("requires an Exit Gate" in error for error in errors(root))

    legacy = "METAFLUX_" + "SESSION_ID"
    write(root / "legacy.md", legacy + "\n")
    assert any("legacy execution marker" in error for error in errors(root))


def test_broken_link(root: Path) -> None:
    create_fixture(root)
    write(root / "README.md", "[missing](missing.md)\n")
    assert any("broken local link" in error for error in errors(root))


def test_commit_environment(root: Path) -> None:
    create_fixture(root)
    tool = root / "fixture-agent"
    write(
        tool,
        "#!/bin/sh\n"
        "if [ \"$1\" = \"--version\" ]; then printf '%s\\n' 'fixture-cli 1.2.3'; exit 0; fi\n"
        "if [ \"$1\" = \"--help\" ]; then exit 0; fi\n"
        "exit 2\n",
    )
    tool.chmod(0o755)
    valid = os.environ.copy()
    valid.update(
        {
            "METAFLUX_AGENT_TOOL_EXECUTABLE": str(tool),
            "METAFLUX_AGENT_EPOCH": "epoch-0002",
            "GIT_AUTHOR_NAME": "fixture-agent",
            "GIT_AUTHOR_EMAIL": "fixture-agent@localhost",
            "GIT_COMMITTER_NAME": "fixture-agent",
            "GIT_COMMITTER_EMAIL": "fixture-agent@localhost",
        }
    )
    checker = STATE.Checker(root)
    checker.validate_commit_environment(valid)
    assert not checker.errors
    invalid = dict(valid)
    invalid["GIT_AUTHOR_NAME"] = "unrelated-name"
    invalid["METAFLUX_AGENT_EPOCH"] = "epoch-9999"
    checker = STATE.Checker(root)
    checker.validate_commit_environment(invalid)
    assert any("GIT_AUTHOR_NAME" in error for error in checker.errors)
    assert any("candidate Epoch" in error for error in checker.errors)

    relative = dict(valid)
    relative["METAFLUX_AGENT_TOOL_EXECUTABLE"] = tool.name
    relative["PATH"] = str(root)
    checker = STATE.Checker(root)
    checker.validate_commit_environment(relative)
    assert any("exact detected path" in error for error in checker.errors)


def test_integration_ancestry(root: Path) -> None:
    create_fixture(root)
    document = goal()
    document["epoch"] = "epoch-0000"
    write(root / "agent/goal.json", json.dumps(document, indent=2) + "\n")
    git(root, "init", "-q")
    git(root, "config", "user.name", "Fixture")
    git(root, "config", "user.email", "fixture@example.invalid")
    git(root, "add", ".")
    git(root, "commit", "-q", "-m", "old epoch")
    old_revision = git(root, "rev-parse", "HEAD")

    write(root / "agent/goal.json", json.dumps(goal(), indent=2) + "\n")
    git(root, "add", "agent/goal.json")
    git(root, "commit", "-q", "-m", "activate epoch")
    activation = git(root, "rev-parse", "HEAD")

    checker = STATE.Checker(root)
    checker.validate_integration_revisions(activation, activation)
    assert not checker.errors
    checker = STATE.Checker(root)
    checker.validate_integration_revisions(old_revision, activation)
    assert any("predates the current Epoch" in error for error in checker.errors)


def main() -> int:
    test_parsers()
    test_cycle_detection()
    with tempfile.TemporaryDirectory(prefix="metaflux-agent-state-") as temp:
        test_goal_variants(Path(temp) / "goal")
        test_plan_and_legacy(Path(temp) / "plan")
        test_broken_link(Path(temp) / "link")
        test_commit_environment(Path(temp) / "commit")
        test_integration_ancestry(Path(temp) / "integration")
    print("agent state self-tests: 9 groups passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
