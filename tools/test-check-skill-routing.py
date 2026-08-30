#!/usr/bin/env python3
"""Self-test for tools/check-skill-routing.py."""

from __future__ import annotations

import copy
import importlib.util
import shutil
import stat
import subprocess
import sys
import tempfile
from pathlib import Path


sys.dont_write_bytecode = True
TOOLS_DIR = Path(__file__).resolve().parent
ROOT = TOOLS_DIR.parent
VALIDATOR_PATH = TOOLS_DIR / "check-skill-routing.py"

spec = importlib.util.spec_from_file_location("check_skill_routing", VALIDATOR_PATH)
assert spec is not None and spec.loader is not None
check_skill_routing = importlib.util.module_from_spec(spec)
spec.loader.exec_module(check_skill_routing)


def valid_observations(corpus: dict, repetitions: int = 2) -> dict:
    observed = check_skill_routing.build_observation_template(ROOT, corpus, repetitions)
    expected = {case["id"]: case["expected_skills"] for case in corpus["cases"]}
    for run in observed["runs"]:
        run["selected_skills"] = list(expected[run["case_id"]])
    observed["runner"]["model"] = "fixture-model"
    observed["runner"]["host"] = "fixture-host"
    return observed


def expect_error(name: str, errors: list[str], needle: str) -> None:
    if not any(needle in error for error in errors):
        raise AssertionError(f"{name}: expected {needle!r}, got {errors!r}")


def main() -> int:
    corpus, read_errors = check_skill_routing.read_json(
        ROOT / "agent" / "skills" / "trigger-evals.json"
    )
    assert not read_errors, read_errors
    assert isinstance(corpus, dict)
    errors = check_skill_routing.validate_corpus(ROOT, corpus)
    assert not errors, errors
    passed = 1

    old_schema = copy.deepcopy(corpus)
    old_schema["schema_version"] = 1
    expect_error(
        "old corpus schema",
        check_skill_routing.validate_corpus(ROOT, old_schema),
        "corpus schema_version must be 2",
    )
    passed += 1

    duplicate = copy.deepcopy(corpus)
    duplicate["cases"][1]["id"] = duplicate["cases"][0]["id"]
    expect_error("duplicate id", check_skill_routing.validate_corpus(ROOT, duplicate), "duplicate id")
    passed += 1

    duplicate_prompt = copy.deepcopy(corpus)
    duplicate_prompt["cases"][1]["prompt"] = duplicate_prompt["cases"][0]["prompt"]
    expect_error(
        "duplicate locale prompt",
        check_skill_routing.validate_corpus(ROOT, duplicate_prompt),
        "duplicate prompt for locale",
    )
    passed += 1

    missing_zh = copy.deepcopy(corpus)
    missing_zh["cases"] = [
        case
        for case in missing_zh["cases"]
        if not (
            case["locale"] == "zh-CN"
            and "runtime-contracts-registry" in case["expected_skills"]
            and case["kind"] == "single"
        )
    ]
    expect_error(
        "missing locale coverage",
        check_skill_routing.validate_corpus(ROOT, missing_zh),
        "runtime-contracts-registry has 0 zh-CN positive",
    )
    passed += 1

    unknown_skill = copy.deepcopy(corpus)
    unknown_skill["cases"][0]["expected_skills"] = ["unknown-skill"]
    expect_error(
        "unknown skill",
        check_skill_routing.validate_corpus(ROOT, unknown_skill),
        "unknown routed skill",
    )
    passed += 1

    missing_workflow_zh = copy.deepcopy(corpus)
    missing_workflow_zh["cases"] = [
        case
        for case in missing_workflow_zh["cases"]
        if not (
            case["locale"] == "zh-CN"
            and "govern-semantic-change" in case["expected_skills"]
            and case["kind"] == "single"
        )
    ]
    expect_error(
        "missing workflow locale coverage",
        check_skill_routing.validate_corpus(ROOT, missing_workflow_zh),
        "govern-semantic-change has 0 zh-CN positive",
    )
    passed += 1

    unknown_corpus_field = copy.deepcopy(corpus)
    unknown_corpus_field["notes"] = "unchecked"
    expect_error(
        "unknown corpus field",
        check_skill_routing.validate_corpus(ROOT, unknown_corpus_field),
        "corpus has unknown fields",
    )
    passed += 1

    unknown_coverage_field = copy.deepcopy(corpus)
    unknown_coverage_field["coverage"]["minimum_total"] = 1
    expect_error(
        "unknown coverage field",
        check_skill_routing.validate_corpus(ROOT, unknown_coverage_field),
        "coverage has unknown fields",
    )
    passed += 1

    reduced_roster = copy.deepcopy(corpus)
    reduced_roster["domain_skills"].remove("runtime-contracts-registry")
    reduced_roster["cases"] = [
        case
        for case in reduced_roster["cases"]
        if "runtime-contracts-registry" not in case["expected_skills"]
        and "runtime-contracts-registry" not in case["forbidden_skills"]
    ]
    expect_error(
        "self-reduced domain roster",
        check_skill_routing.validate_corpus(ROOT, reduced_roster),
        "must match the records-gate roster",
    )
    passed += 1

    reduced_workflow_roster = copy.deepcopy(corpus)
    reduced_workflow_roster["workflow_skills"].remove("distill-project-knowledge")
    reduced_workflow_roster["cases"] = [
        case
        for case in reduced_workflow_roster["cases"]
        if "distill-project-knowledge" not in case["expected_skills"]
        and "distill-project-knowledge" not in case["forbidden_skills"]
    ]
    expect_error(
        "self-reduced workflow roster",
        check_skill_routing.validate_corpus(ROOT, reduced_workflow_roster),
        "workflow_skills must match the records-gate roster",
    )
    passed += 1

    overlapping_rosters = copy.deepcopy(corpus)
    overlapping_rosters["domain_skills"].append("govern-semantic-change")
    expect_error(
        "domain workflow roster overlap",
        check_skill_routing.validate_corpus(ROOT, overlapping_rosters),
        "domain_skills and workflow_skills overlap",
    )
    passed += 1

    reduced_threshold = copy.deepcopy(corpus)
    reduced_threshold["coverage"]["minimum_composition_cases"] = 1
    expect_error(
        "self-reduced composition threshold",
        check_skill_routing.validate_corpus(ROOT, reduced_threshold),
        "minimum_composition_cases must equal",
    )
    passed += 1

    reduced_workflow_composition_threshold = copy.deepcopy(corpus)
    reduced_workflow_composition_threshold["coverage"][
        "minimum_composition_per_workflow_skill"
    ]["en"] = 0
    expect_error(
        "self-reduced workflow composition threshold",
        check_skill_routing.validate_corpus(
            ROOT, reduced_workflow_composition_threshold
        ),
        "minimum_composition_per_workflow_skill must equal",
    )
    passed += 1

    missing_workflow_composition = copy.deepcopy(corpus)
    missing_workflow_composition["cases"] = [
        case
        for case in missing_workflow_composition["cases"]
        if case["id"] != "COMBO-DISTILL-CPU"
    ]
    expect_error(
        "missing workflow composition coverage",
        check_skill_routing.validate_corpus(ROOT, missing_workflow_composition),
        "distill-project-knowledge has 0 en composition",
    )
    passed += 1

    observations = valid_observations(corpus)
    errors = check_skill_routing.validate_observations(ROOT, corpus, observations)
    assert not errors, errors
    passed += 1

    placeholder_metadata = copy.deepcopy(observations)
    placeholder_metadata["runner"]["model"] = "MODEL"
    expect_error(
        "placeholder runner metadata",
        check_skill_routing.validate_observations(ROOT, corpus, placeholder_metadata),
        "still contains a placeholder",
    )
    passed += 1

    wrong_product = copy.deepcopy(observations)
    wrong_product["runner"]["product"] = "Different Product"
    expect_error(
        "non-Codex runner product",
        check_skill_routing.validate_observations(ROOT, corpus, wrong_product),
        "runner.product must be one of",
    )
    passed += 1

    invalid_date = copy.deepcopy(observations)
    invalid_date["runner"]["recorded_at"] = "2026/08/28"
    expect_error(
        "invalid recorded date",
        check_skill_routing.validate_observations(ROOT, corpus, invalid_date),
        "must be an ISO date",
    )
    passed += 1

    unknown_runner_field = copy.deepcopy(observations)
    unknown_runner_field["runner"]["notes"] = "unchecked"
    expect_error(
        "unknown runner field",
        check_skill_routing.validate_observations(ROOT, corpus, unknown_runner_field),
        "runner has unknown fields",
    )
    passed += 1

    stale_observations = copy.deepcopy(observations)
    changed_corpus = copy.deepcopy(corpus)
    changed_corpus["cases"][0]["prompt"] = "A materially changed routing prompt."
    expect_error(
        "observations bound to corpus digest",
        check_skill_routing.validate_observations(ROOT, changed_corpus, stale_observations),
        "does not match the current corpus",
    )
    passed += 1

    with tempfile.TemporaryDirectory(prefix="metaflux-routing-selftest-") as temporary:
        changed_root = Path(temporary) / "repo"
        shutil.copytree(ROOT / "agent" / "skills", changed_root / "agent" / "skills")
        (changed_root / "tools").mkdir(parents=True)
        shutil.copy2(
            ROOT / "tools" / "check-agent-records.py",
            changed_root / "tools" / "check-agent-records.py",
        )
        skill_path = (
            changed_root
            / "agent"
            / "skills"
            / "runtime-contracts-registry"
            / "SKILL.md"
        )
        skill_path.chmod(skill_path.stat().st_mode | stat.S_IWUSR)
        skill_path.write_text(
            skill_path.read_text(encoding="utf-8").replace(
                "Design or review ecosystem-neutral registry and provider views",
                "Design or review changed registry and provider views",
                1,
            ),
            encoding="utf-8",
        )
        expect_error(
            "observations bound to skill routing inputs",
            check_skill_routing.validate_observations(
                changed_root, corpus, stale_observations
            ),
            "does not match the current routed skill inputs",
        )
    passed += 1

    with tempfile.TemporaryDirectory(prefix="metaflux-routing-selftest-") as temporary:
        changed_root = Path(temporary) / "repo"
        shutil.copytree(ROOT / "agent" / "skills", changed_root / "agent" / "skills")
        (changed_root / "tools").mkdir(parents=True)
        shutil.copy2(
            ROOT / "tools" / "check-agent-records.py",
            changed_root / "tools" / "check-agent-records.py",
        )
        skill_path = (
            changed_root
            / "agent"
            / "skills"
            / "govern-semantic-change"
            / "SKILL.md"
        )
        skill_path.chmod(skill_path.stat().st_mode | stat.S_IWUSR)
        skill_path.write_text(
            skill_path.read_text(encoding="utf-8").replace(
                "Govern a decision-authorized breaking change",
                "Govern a changed decision-authorized breaking change",
                1,
            ),
            encoding="utf-8",
        )
        expect_error(
            "observations bound to workflow routing inputs",
            check_skill_routing.validate_observations(
                changed_root, corpus, stale_observations
            ),
            "does not match the current routed skill inputs",
        )
    passed += 1

    missing_digest = copy.deepcopy(observations)
    del missing_digest["corpus_sha256"]
    expect_error(
        "observations missing corpus digest",
        check_skill_routing.validate_observations(ROOT, corpus, missing_digest),
        "observations missing fields",
    )
    passed += 1

    missing_expected = copy.deepcopy(observations)
    missing_expected["runs"][0]["selected_skills"] = []
    expect_error(
        "missing expected",
        check_skill_routing.validate_observations(ROOT, corpus, missing_expected),
        "missing expected skills",
    )
    passed += 1

    missing_workflow_expected = copy.deepcopy(observations)
    run = next(
        item
        for item in missing_workflow_expected["runs"]
        if item["case_id"] == "GOV-P1"
    )
    run["selected_skills"] = []
    expect_error(
        "missing expected workflow",
        check_skill_routing.validate_observations(
            ROOT, corpus, missing_workflow_expected
        ),
        "missing expected skills: govern-semantic-change",
    )
    passed += 1

    forbidden = copy.deepcopy(observations)
    negative_case = next(case for case in corpus["cases"] if case["forbidden_skills"])
    run = next(item for item in forbidden["runs"] if item["case_id"] == negative_case["id"])
    run["selected_skills"].append(negative_case["forbidden_skills"][0])
    expect_error(
        "forbidden selected",
        check_skill_routing.validate_observations(ROOT, corpus, forbidden),
        "loaded forbidden skills",
    )
    passed += 1

    forbidden_workflow = copy.deepcopy(observations)
    run = next(
        item
        for item in forbidden_workflow["runs"]
        if item["case_id"] == "GOV-N1"
    )
    run["selected_skills"].append("govern-semantic-change")
    expect_error(
        "forbidden workflow selected",
        check_skill_routing.validate_observations(ROOT, corpus, forbidden_workflow),
        "loaded forbidden skills: govern-semantic-change",
    )
    passed += 1

    unexpected = copy.deepcopy(observations)
    first_case = corpus["cases"][0]
    extra = next(
        slug
        for slug in corpus["workflow_skills"]
        if slug not in first_case["expected_skills"]
    )
    unexpected["runs"][0]["selected_skills"].append(extra)
    expect_error(
        "unexpected workflow selected",
        check_skill_routing.validate_observations(ROOT, corpus, unexpected),
        "loaded unexpected routed skills",
    )
    passed += 1

    unexpected_domain = copy.deepcopy(observations)
    run = next(
        item for item in unexpected_domain["runs"] if item["case_id"] == "GOV-P1"
    )
    run["selected_skills"].append("runtime-contracts-registry")
    expect_error(
        "unexpected domain selected",
        check_skill_routing.validate_observations(ROOT, corpus, unexpected_domain),
        "loaded unexpected routed skills: runtime-contracts-registry",
    )
    passed += 1

    ignored_unscored_workflow = copy.deepcopy(observations)
    run = next(
        item
        for item in ignored_unscored_workflow["runs"]
        if item["case_id"] == "DISTILL-N1"
    )
    run["selected_skills"].append("session-guidance")
    errors = check_skill_routing.validate_observations(
        ROOT, corpus, ignored_unscored_workflow
    )
    assert not errors, errors
    passed += 1

    missing_run = copy.deepcopy(observations)
    missing_run["runs"].pop()
    expect_error(
        "missing iteration",
        check_skill_routing.validate_observations(ROOT, corpus, missing_run),
        "do not equal",
    )
    passed += 1

    unknown_case = copy.deepcopy(observations)
    unknown_case["runs"][0]["case_id"] = "UNKNOWN-CASE"
    expect_error(
        "unknown case",
        check_skill_routing.validate_observations(ROOT, corpus, unknown_case),
        "unknown case_id",
    )
    passed += 1

    cli = subprocess.run(
        [
            sys.executable,
            str(VALIDATOR_PATH),
            str(ROOT),
            "--observed",
            "/definitely/missing-routing-observations.json",
            "--emit-template",
        ],
        check=False,
        capture_output=True,
        text=True,
    )
    if cli.returncode == 0:
        raise AssertionError("mutually exclusive CLI modes unexpectedly returned 0")
    passed += 1

    print(f"skill routing self-test: {passed}/{passed} cases passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
