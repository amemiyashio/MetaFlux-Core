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
DIAGNOSTICS_SCRIPT = SCRIPT.with_name("agent_diagnostics.py")


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


def isolated_git_environment(
    source: dict[str, str] | None = None,
) -> dict[str, str]:
    environment = dict(os.environ if source is None else source)
    result = subprocess.run(
        ["git", "rev-parse", "--local-env-vars"],
        check=False,
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        raise AssertionError(
            f"cannot enumerate Git local environment: {result.stderr}"
        )
    for variable in result.stdout.splitlines():
        environment.pop(variable, None)
    return environment


def git(
    root: Path,
    *arguments: str,
    environment: dict[str, str] | None = None,
) -> str:
    result = subprocess.run(
        ["git", *arguments],
        cwd=root,
        env=isolated_git_environment(environment),
        check=False,
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        raise AssertionError(f"git {' '.join(arguments)} failed: {result.stderr}")
    return result.stdout.strip()


def goal() -> dict:
    return {
        "schema_version": 3,
        "epoch": "epoch-0002",
        "batch": {"id": "batch-0001", "status": "open"},
        "target": {
            "milestone": "milestone-0.1.0.0",
            "work_item": "work-item-0.1.0.1",
        },
        "objective": "Deliver one candidate.",
        "references": [],
        "lanes": [
            {
                "id": "lane-one",
                "work_item": "work-item-0.1.0.1",
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
    write(
        root / "agent/memory/open-decisions.md",
        "| Milestone | Decision | Blocks | Closure condition |\n"
        "| --- | --- | --- | --- |\n",
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
        root / "tools/agent_diagnostics.py",
        DIAGNOSTICS_SCRIPT.read_text(encoding="utf-8"),
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


def errors(root: Path):
    checker = STATE.Checker(root)
    checker.run()
    return checker.errors


def diagnostic_text(diagnostic) -> str:
    return " ".join((diagnostic.summary, *diagnostic.evidence))


def has_fragment(diagnostics, fragment: str) -> bool:
    return any(fragment in diagnostic_text(error) for error in diagnostics)


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
    found = errors(root)
    assert has_fragment(found, "epoch must match")
    epoch_error = next(error for error in found if error.code == "agent-state.epoch-invalid")
    assert epoch_error.responsibility == "epoch-governor"
    assert epoch_error.disposition == "stop-and-report"

    document = goal()
    document["lanes"][0]["status"] = "in_progress"
    write(path, json.dumps(document))
    found = errors(root)
    assert has_fragment(found, "status is invalid")
    assert any(error.responsibility == "batch-integrator" for error in found)

    document = goal()
    document["lanes"][0]["status"] = "integrated"
    write(path, json.dumps(document))
    found = errors(root)
    assert has_fragment(found, "must be Complete when its lane is integrated")

    document = goal()
    second = dict(document["lanes"][0])
    second["id"] = "lane-two"
    second["depends_on"] = ["lane-missing"]
    document["lanes"].append(second)
    write(path, json.dumps(document))
    found = errors(root)
    assert has_fragment(found, "duplicate iteration")
    assert has_fragment(found, "unresolved dependency")

    reference_manifest = root / "references/catalog/fixture/fixture-v1.json"
    write(
        reference_manifest,
        json.dumps({"schema_version": 1, "id": "fixture-v1"}) + "\n",
    )
    document = goal()
    document["references"] = [
        {
            "id": "reference-fixture-v1",
            "entry": "fixture-v1",
            "required_by": ["lane-one"],
        }
    ]
    write(path, json.dumps(document))
    assert not errors(root)

    write(
        root / "references/sources/fixture/v1/upstream.md",
        "[upstream-specific-link](missing.md)\nSC2086\n",
    )
    assert not errors(root)

    document["references"].append(dict(document["references"][0]))
    write(path, json.dumps(document))
    found = errors(root)
    assert has_fragment(found, "duplicate reference id")
    assert has_fragment(found, "duplicate reference entry")

    document = goal()
    document["references"] = [
        {
            "id": "reference-missing",
            "entry": "missing",
            "required_by": ["lane-missing"],
        }
    ]
    write(path, json.dumps(document))
    found = errors(root)
    assert has_fragment(found, "entry does not resolve")
    assert has_fragment(found, "required_by has unresolved lane")


def test_plan_and_legacy(root: Path) -> None:
    create_fixture(root)
    work = root / "agent/plan/milestone-0.1.0.0-core/work/work-item-0.1.0.1-one.md"
    write(work, work.read_text(encoding="utf-8").replace("## Exit Gate", "## Acceptance"))
    assert has_fragment(errors(root), "requires an Exit Gate")

    legacy = "METAFLUX_" + "SESSION_ID"
    duplicate_epoch = "METAFLUX_AGENT_" + "EPOCH"
    write(root / "legacy.md", legacy + "\n" + duplicate_epoch + "\n")
    assert has_fragment(errors(root), "legacy execution marker")


def test_plan_semantic_drift(root: Path) -> None:
    create_fixture(root)
    work = root / "agent/plan/milestone-0.1.0.0-core/work/work-item-0.1.0.1-one.md"
    write(work, work.read_text(encoding="utf-8") + "\n## Measured progress (today)\n")
    assert has_fragment(errors(root), "execution-history heading")

    complete_root = root / "complete"
    create_fixture(complete_root)
    plan = complete_root / "agent/plan/milestone-0.1.0.0-core/plan.md"
    write(
        plan,
        plan.read_text(encoding="utf-8")
        .replace("status: Active", "status: Complete")
        .replace("None.", "1. Choose the stable format."),
    )
    assert has_fragment(errors(complete_root), "complete plan record cannot retain")


def test_open_decision_coverage(root: Path) -> None:
    create_fixture(root)
    plan = root / "agent/plan/milestone-0.1.0.0-core/plan.md"
    write(
        plan,
        plan.read_text(encoding="utf-8").replace(
            "None.", "1. Choose the stable format."
        ),
    )
    ledger = root / "agent/memory/open-decisions.md"
    write(
        ledger,
        ledger.read_text(encoding="utf-8")
        + "| milestone-0.1.0.0 | Choose the stable format | Contract | Before integration |\n",
    )
    assert not errors(root)

    write(
        ledger,
        ledger.read_text(encoding="utf-8").replace(
            "Choose the stable format", "Choose another format"
        ),
    )
    found = errors(root)
    assert has_fragment(found, "missing open-decision row")
    assert has_fragment(found, "has no plan owner")


def test_broken_link(root: Path) -> None:
    create_fixture(root)
    write(root / "README.md", "[missing](missing.md)\n")
    assert has_fragment(errors(root), "broken local link")


def test_commit_environment(root: Path) -> None:
    create_fixture(root)
    valid = os.environ.copy()
    valid.update(
        {
            "METAFLUX_AGENT_TOOL": "fixture-agent",
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
    checker = STATE.Checker(root)
    checker.validate_commit_environment(invalid)
    assert has_fragment(checker.errors, "GIT_AUTHOR_NAME")
    assert all(error.code == "commit-gate.environment-invalid" for error in checker.errors)

    mismatched = dict(valid)
    mismatched["METAFLUX_AGENT_TOOL"] = "Fixture-Agent"
    checker = STATE.Checker(root)
    checker.validate_commit_environment(mismatched)
    assert has_fragment(checker.errors, "conversation-emitted harness name")

    rejected = dict(valid)
    rejected.pop("METAFLUX_AGENT_TOOL")
    checker = STATE.Checker(root)
    checker.validate_commit_environment(rejected)
    assert [error.code for error in checker.errors] == ["agent-tool.missing-declaration"]
    assert checker.errors[0].responsibility == "user-or-application"


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

    write(root / "candidate.txt", "candidate\n")
    git(root, "add", "candidate.txt")
    git(root, "commit", "-q", "-m", "candidate")
    candidate = git(root, "rev-parse", "HEAD")

    checker = STATE.Checker(root)
    checker.validate_integration_revisions(activation, candidate)
    assert not checker.errors
    checker = STATE.Checker(root)
    checker.validate_integration_revisions(activation, activation)
    assert has_fragment(checker.errors, "non-empty candidate")
    checker = STATE.Checker(root)
    checker.validate_integration_revisions(old_revision, candidate)
    assert has_fragment(checker.errors, "predates the current Epoch")
    assert checker.errors[0].code == "integration.revision-invalid"
    assert checker.errors[0].responsibility == "user-or-application"
    assert checker.errors[0].disposition == "preserve-and-report"


def test_json_cli(root: Path) -> None:
    create_fixture(root)
    document = goal()
    document["lanes"][0]["status"] = "invalid"
    write(root / "agent/goal.json", json.dumps(document))
    result = subprocess.run(
        [
            sys.executable,
            "-B",
            str(SCRIPT),
            str(root),
            "--diagnostic-format",
            "json",
        ],
        check=False,
        capture_output=True,
        text=True,
    )
    assert result.returncode == 1
    assert result.stdout == ""
    envelope = json.loads(result.stderr)
    assert envelope["schema_version"] == 1
    assert envelope["status"] == "error"
    assert any(
        error["code"] == "agent-state.goal-invalid"
        and error["responsibility"] == "batch-integrator"
        for error in envelope["errors"]
    )


def test_foreign_git_environment_isolation(root: Path) -> None:
    root.mkdir()
    outer = root / "outer"
    outer.mkdir()
    git(outer, "init", "-q")
    git(outer, "config", "user.name", "Outer")
    git(outer, "config", "user.email", "outer@example.invalid")
    write(outer / "sentinel.txt", "outer\n")
    git(outer, "add", "sentinel.txt")
    git(outer, "commit", "-q", "-m", "outer sentinel")
    outer_revision = git(outer, "rev-parse", "HEAD")
    outer_git_dir = git(outer, "rev-parse", "--absolute-git-dir")

    fixture = root / "fixture"
    fixture.mkdir()
    poisoned = os.environ.copy()
    poisoned.update(
        {
            "GIT_DIR": outer_git_dir,
            "GIT_COMMON_DIR": outer_git_dir,
            "GIT_WORK_TREE": str(outer),
            "GIT_INDEX_FILE": str(Path(outer_git_dir) / "index"),
        }
    )
    git(fixture, "init", "-q", environment=poisoned)
    git(fixture, "config", "user.name", "Fixture", environment=poisoned)
    git(
        fixture,
        "config",
        "user.email",
        "fixture@example.invalid",
        environment=poisoned,
    )
    write(fixture / "value.txt", "fixture\n")
    git(fixture, "add", "value.txt", environment=poisoned)
    git(fixture, "commit", "-q", "-m", "fixture", environment=poisoned)

    assert git(outer, "rev-parse", "HEAD") == outer_revision
    assert git(outer, "status", "--porcelain") == ""
    assert git(outer, "config", "--bool", "core.bare") == "false"
    assert git(fixture, "log", "-1", "--format=%s") == "fixture"


def main() -> int:
    test_parsers()
    test_cycle_detection()
    with tempfile.TemporaryDirectory(prefix="metaflux-agent-state-") as temp:
        test_goal_variants(Path(temp) / "goal")
        test_plan_and_legacy(Path(temp) / "plan")
        test_plan_semantic_drift(Path(temp) / "semantic")
        test_open_decision_coverage(Path(temp) / "open-decisions")
        test_broken_link(Path(temp) / "link")
        test_commit_environment(Path(temp) / "commit")
        test_integration_ancestry(Path(temp) / "integration")
        test_json_cli(Path(temp) / "json")
        test_foreign_git_environment_isolation(Path(temp) / "isolation")
    print("agent state self-tests: 13 groups passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
