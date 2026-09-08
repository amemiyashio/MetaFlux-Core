#!/usr/bin/env python3
"""Focused mutation tests for check-skill-routing.py."""

from __future__ import annotations

import copy
import importlib.util
import json
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = Path(__file__).with_name("check-skill-routing.py")
CORPUS = ROOT / "agent" / "skills" / "trigger-evals.json"


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
        if "govern-epoch" in case["expected_skills"]
    )
    explicit["prompt"] = explicit["prompt"].replace("$govern-epoch", "govern epoch")
    expect(changed, "must explicitly invoke $govern-epoch")

    changed = copy.deepcopy(corpus)
    explicit = next(
        case
        for case in changed["cases"]
        if "replan-roadmap" in case["expected_skills"]
    )
    explicit["prompt"] = explicit["prompt"].replace(
        "$replan-roadmap", "replan roadmap"
    )
    expect(changed, "must explicitly invoke $replan-roadmap")

    changed = copy.deepcopy(corpus)
    explicit = next(
        case
        for case in changed["cases"]
        if "integrate-batch" in case["expected_skills"]
    )
    explicit["prompt"] = explicit["prompt"].replace(
        "$integrate-batch", "integrate batch"
    )
    expect(changed, "must explicitly invoke $integrate-batch")

    changed = copy.deepcopy(corpus)
    changed["cases"][0]["forbidden_skills"] = list(
        changed["cases"][0]["expected_skills"]
    )
    expect(changed, "expects and forbids")

    changed = copy.deepcopy(corpus)
    changed["workflow_skills"] = ["roast"]
    expect(changed, "workflow_skills do not match")

    print("skill routing self-tests: 8 groups passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
