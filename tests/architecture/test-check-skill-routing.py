#!/usr/bin/env python3
"""Focused mutation tests for check-skill-routing.py."""

from __future__ import annotations

import copy
import importlib.util
import json
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
SCRIPT = ROOT / "tools/check-skill-routing.py"
CORPUS = ROOT / "agent" / "skills" / "trigger-evals.json"
EXPANDED_EXPERTS = {
    "pytorch-cuda-profile",
    "cublas-compatibility",
    "compiler-worker-isolation",
    "compiler-artifact-cache",
    "daemon-execution-runtime",
    "process-activation",
}


def load_module():
    spec = importlib.util.spec_from_file_location("metaflux_skill_routing", SCRIPT)
    if spec is None or spec.loader is None:
        raise AssertionError("routing checker cannot be loaded")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


ROUTING = load_module()


def errors(corpus: dict):
    return ROUTING.validate_corpus(ROOT, corpus)


def expect(corpus: dict, fragment: str) -> None:
    found = errors(corpus)
    if not any(
        fragment in " ".join((error.summary, *error.evidence)) for error in found
    ):
        raise AssertionError(f"expected {fragment!r}, got {found}")
    for diagnostic in found:
        assert diagnostic.code == "skill-routing.invalid-authority"
        assert diagnostic.responsibility == "current-agent"
        assert diagnostic.required_action
        assert diagnostic.resume_when


def main() -> int:
    corpus = json.loads(CORPUS.read_text(encoding="utf-8"))
    assert not errors(corpus)

    changed = copy.deepcopy(corpus)
    changed["cases"][1]["id"] = changed["cases"][0]["id"]
    expect(changed, "duplicate case id")

    changed = copy.deepcopy(corpus)
    changed["cases"][0]["expected_skills"] = ["unknown-skill"]
    expect(changed, "unknown skills")

    changed = copy.deepcopy(corpus)
    explicit = next(
        case
        for case in changed["cases"]
        if "epoch" in case["expected_skills"]
    )
    explicit["prompt"] = explicit["prompt"].replace("$epoch", "govern epoch")
    expect(changed, "must explicitly invoke $epoch")

    changed = copy.deepcopy(corpus)
    explicit = next(
        case
        for case in changed["cases"]
        if "epoch" in case["expected_skills"]
    )
    explicit["prompt"] = explicit["prompt"].replace(
        "$epoch", "replan roadmap"
    )
    expect(changed, "must explicitly invoke $epoch")

    changed = copy.deepcopy(corpus)
    explicit = next(
        case
        for case in changed["cases"]
        if "batch" in case["expected_skills"]
    )
    explicit["prompt"] = explicit["prompt"].replace(
        "$batch", "integrate batch"
    )
    assert not errors(changed), "Qualified Batch acceptance supports automatic invocation"

    changed = copy.deepcopy(corpus)
    changed["cases"][0]["forbidden_skills"] = list(
        changed["cases"][0]["expected_skills"]
    )
    expect(changed, "expects and forbids")

    changed = copy.deepcopy(corpus)
    changed["workflow_skills"] = ["main"]
    expect(changed, "workflow_skills do not match")

    changed = copy.deepcopy(corpus)
    changed["cases"][0]["locale"] = "fr"
    expect(changed, ".locale is invalid")

    changed = copy.deepcopy(corpus)
    changed["cases"][0]["prompt"] = chr(0x4E2D) + chr(0x6587)
    expect(changed, ".prompt must be English")

    changed = copy.deepcopy(corpus)
    changed["cases"][1]["prompt"] = changed["cases"][0]["prompt"]
    expect(changed, "duplicates a locale/prompt pair")

    assert EXPANDED_EXPERTS <= set(corpus["domain_skills"])
    for skill in sorted(EXPANDED_EXPERTS):
        changed = copy.deepcopy(corpus)
        positive_ids = [
            case["id"] for case in changed["cases"]
            if skill in case["expected_skills"]
        ]
        removed = set(positive_ids[2:])
        changed["cases"] = [
            case for case in changed["cases"]
            if case["id"] not in removed
        ]
        expect(changed, f"{skill} has too few en positive cases")

    for skill in sorted(EXPANDED_EXPERTS):
        changed = copy.deepcopy(corpus)
        near_miss_ids = [
            case["id"] for case in changed["cases"]
            if case["kind"] == "near-miss" and skill in case["forbidden_skills"]
        ]
        removed = set(near_miss_ids[1:])
        changed["cases"] = [
            case for case in changed["cases"]
            if case["id"] not in removed
        ]
        expect(changed, f"{skill} has too few en near-miss cases")

    for skill in corpus["workflow_skills"]:
        changed = copy.deepcopy(corpus)
        composition_ids = [
            case["id"] for case in changed["cases"]
            if case["kind"] == "composition" and skill in case["expected_skills"]
        ]
        removed = set(composition_ids[1:])
        changed["cases"] = [
            case for case in changed["cases"]
            if case["id"] not in removed
        ]
        expect(changed, f"{skill} has too few en composition cases")

    changed = copy.deepcopy(corpus)
    changed["coverage"]["minimum_positive_per_skill"]["en"] = 2
    expect(changed, "coverage policy must match the fixed routing gate")

    print("skill routing self-tests: 15 groups passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
