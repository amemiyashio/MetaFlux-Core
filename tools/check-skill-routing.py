#!/usr/bin/env python3
"""Validate MetaFlux's static bilingual expert-skill routing contract."""

from __future__ import annotations

import argparse
import json
import re
import runpy
import sys
from collections import Counter
from pathlib import Path
from typing import Any


CASE_ID_RE = re.compile(r"[A-Z0-9]+(?:-[A-Z0-9]+)*")
SKILL_RE = re.compile(r"[a-z0-9]+(?:-[a-z0-9]+)*")
KINDS = {"single", "near-miss", "composition"}
LOCALES = {"en", "zh-CN"}
EXPLICIT_ONLY = {"integrate-batch", "govern-epoch", "roast"}
MINIMUM_POSITIVE = {"en": 2, "zh-CN": 1}
MINIMUM_NEAR_MISS = {"en": 1, "zh-CN": 1}
MINIMUM_COMPOSITIONS = 12
MINIMUM_WORKFLOW_COMPOSITION = {"en": 1, "zh-CN": 1}
CASE_FIELDS = {
    "id",
    "kind",
    "locale",
    "prompt",
    "expected_skills",
    "forbidden_skills",
}


def read_json(path: Path) -> tuple[Any | None, list[str]]:
    try:
        return json.loads(path.read_text(encoding="utf-8")), []
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
        return None, [f"{path}: cannot read routing corpus: {error}"]


def policy_rosters(root: Path) -> tuple[set[str], set[str], list[str]]:
    path = root / "tools" / "check-agent-state.py"
    try:
        namespace = runpy.run_path(str(path))
    except (OSError, RuntimeError, SyntaxError) as error:
        return set(), set(), [f"{path}: cannot load routing policy: {error}"]
    domains = namespace.get("DOMAIN_SKILL_SLUGS")
    workflows = namespace.get("WORKFLOW_SKILL_SLUGS")
    if not isinstance(domains, set) or not isinstance(workflows, set):
        return set(), set(), [f"{path}: routed-skill rosters must be sets"]
    return set(domains), set(workflows), []


def string_list(value: Any, where: str, errors: list[str]) -> list[str]:
    if not isinstance(value, list) or any(not isinstance(item, str) for item in value):
        errors.append(f"{where} must be a string list")
        return []
    if len(value) != len(set(value)):
        errors.append(f"{where} contains duplicates")
    return value


def validate_corpus(root: Path, corpus: Any) -> list[str]:
    errors: list[str] = []
    domains, workflows, policy_errors = policy_rosters(root)
    errors.extend(policy_errors)
    if not isinstance(corpus, dict):
        return errors + ["routing corpus must be an object"]
    if corpus.get("schema_version") != 2:
        errors.append("schema_version must be 2")

    declared_domains = string_list(corpus.get("domain_skills"), "domain_skills", errors)
    declared_workflows = string_list(
        corpus.get("workflow_skills"), "workflow_skills", errors
    )
    if set(declared_domains) != domains:
        errors.append("domain_skills do not match check-agent-state.py")
    if set(declared_workflows) != workflows:
        errors.append("workflow_skills do not match check-agent-state.py")
    known = domains | workflows

    expected_coverage = {
        "required_locales": ["en", "zh-CN"],
        "minimum_positive_per_skill": MINIMUM_POSITIVE,
        "minimum_near_miss_per_skill": MINIMUM_NEAR_MISS,
        "minimum_composition_cases": MINIMUM_COMPOSITIONS,
        "minimum_composition_per_workflow_skill": MINIMUM_WORKFLOW_COMPOSITION,
    }
    if corpus.get("coverage") != expected_coverage:
        errors.append("coverage policy must match the fixed routing gate")

    cases = corpus.get("cases")
    if not isinstance(cases, list) or not cases:
        return errors + ["cases must be a non-empty list"]

    case_ids: set[str] = set()
    prompts: set[tuple[str, str]] = set()
    positives: Counter[tuple[str, str]] = Counter()
    near_misses: Counter[tuple[str, str]] = Counter()
    workflow_compositions: Counter[tuple[str, str]] = Counter()
    composition_count = 0

    for index, case in enumerate(cases):
        where = f"case[{index}]"
        if not isinstance(case, dict) or set(case) != CASE_FIELDS:
            errors.append(f"{where} fields must be exactly {sorted(CASE_FIELDS)}")
            continue
        case_id = case["id"]
        kind = case["kind"]
        locale = case["locale"]
        prompt = case["prompt"]
        if not isinstance(case_id, str) or CASE_ID_RE.fullmatch(case_id) is None:
            errors.append(f"{where}.id must be an uppercase hyphenated identifier")
        elif case_id in case_ids:
            errors.append(f"duplicate case id: {case_id}")
        else:
            case_ids.add(case_id)
        if kind not in KINDS:
            errors.append(f"{where}.kind is invalid")
        if locale not in LOCALES:
            errors.append(f"{where}.locale is invalid")
        if not isinstance(prompt, str) or not prompt.strip():
            errors.append(f"{where}.prompt must be non-empty")
            prompt = ""
        prompt_key = (str(locale), prompt)
        if prompt_key in prompts:
            errors.append(f"{where} duplicates a locale/prompt pair")
        prompts.add(prompt_key)

        expected = string_list(case["expected_skills"], f"{where}.expected_skills", errors)
        forbidden = string_list(case["forbidden_skills"], f"{where}.forbidden_skills", errors)
        unknown = (set(expected) | set(forbidden)) - known
        if unknown:
            errors.append(f"{where} names unknown skills: {sorted(unknown)}")
        overlap = set(expected) & set(forbidden)
        if overlap:
            errors.append(f"{where} expects and forbids the same skills: {sorted(overlap)}")
        if kind == "single" and len(expected) != 1:
            errors.append(f"{where} single case must expect exactly one skill")
        if kind == "composition":
            composition_count += 1
            if len(expected) < 2:
                errors.append(f"{where} composition must expect at least two skills")

        for skill in expected:
            positives[(skill, str(locale))] += 1
            if skill in EXPLICIT_ONLY and f"${skill}" not in prompt:
                errors.append(f"{where} must explicitly invoke ${skill}")
            if kind == "composition" and skill in workflows:
                workflow_compositions[(skill, str(locale))] += 1
        if kind == "near-miss":
            for skill in forbidden:
                near_misses[(skill, str(locale))] += 1

    for skill in sorted(known):
        for locale, minimum in MINIMUM_POSITIVE.items():
            if positives[(skill, locale)] < minimum:
                errors.append(f"{skill} has too few {locale} positive cases")
        for locale, minimum in MINIMUM_NEAR_MISS.items():
            if near_misses[(skill, locale)] < minimum:
                errors.append(f"{skill} has too few {locale} near-miss cases")
    if composition_count < MINIMUM_COMPOSITIONS:
        errors.append("too few composition cases")
    for skill in sorted(workflows):
        for locale, minimum in MINIMUM_WORKFLOW_COMPOSITION.items():
            if workflow_compositions[(skill, locale)] < minimum:
                errors.append(f"{skill} has too few {locale} composition cases")

    for skill in sorted(known):
        if SKILL_RE.fullmatch(skill) is None:
            errors.append(f"invalid skill slug: {skill}")
        skill_path = root / "agent" / "skills" / skill / "SKILL.md"
        interface_path = root / "agent" / "skills" / skill / "agents" / "openai.yaml"
        if not skill_path.is_file():
            errors.append(f"missing routed skill: {skill_path}")
        if not interface_path.is_file():
            errors.append(f"missing routed skill interface: {interface_path}")
    return errors


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(description=__doc__)
    result.add_argument("root", nargs="?", default=".", type=Path)
    return result


def main() -> int:
    arguments = parser().parse_args()
    root = arguments.root.resolve()
    corpus, errors = read_json(root / "agent" / "skills" / "trigger-evals.json")
    if not errors:
        errors = validate_corpus(root, corpus)
    if errors:
        for error in sorted(set(errors)):
            print(f"error: {error}", file=sys.stderr)
        print(f"skill routing failed with {len(set(errors))} error(s)", file=sys.stderr)
        return 1
    count = len(corpus["cases"])
    print(f"skill routing: ok ({count} bilingual cases)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
