#!/usr/bin/env python3
"""Validate the expert-skill routing corpus and score captured observations."""

from __future__ import annotations

import argparse
import copy
import datetime as dt
import hashlib
import json
import re
import runpy
import sys
import unicodedata
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any


CASE_ID_RE = re.compile(r"^[A-Z0-9]+(?:-[A-Z0-9]+)*$")
SKILL_SLUG_RE = re.compile(r"^[a-z0-9]+(?:-[a-z0-9]+)*$")
CASE_KINDS = {"single", "near-miss", "composition"}
CORPUS_FIELDS = {"schema_version", "domain_skills", "coverage", "cases"}
COVERAGE_FIELDS = {
    "required_locales",
    "minimum_positive_per_skill",
    "minimum_near_miss_per_skill",
    "minimum_composition_cases",
}
CASE_FIELDS = {
    "id",
    "kind",
    "locale",
    "prompt",
    "expected_skills",
    "forbidden_skills",
    "allow_additional_domain_skills",
}
RUNNER_FIELDS = {"product", "model", "host", "recorded_at", "repetitions"}
RUNNER_PLACEHOLDERS = {"HOST", "MODEL", "PLACEHOLDER", "TBD", "TODO"}
RUNNER_PRODUCTS = {"Codex"}
OBSERVATION_FIELDS = {
    "schema_version",
    "corpus_sha256",
    "routing_inputs_sha256",
    "runner",
    "runs",
}
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
REQUIRED_LOCALES = ("en", "zh-CN")
MINIMUM_POSITIVE_PER_SKILL = {"en": 2, "zh-CN": 1}
MINIMUM_NEAR_MISS_PER_SKILL = {"en": 1, "zh-CN": 1}
MINIMUM_COMPOSITION_CASES = 12


def read_json(path: Path) -> tuple[Any | None, list[str]]:
    try:
        return json.loads(path.read_text(encoding="utf-8")), []
    except FileNotFoundError:
        return None, [f"missing JSON file: {path}"]
    except UnicodeDecodeError as exc:
        return None, [f"invalid UTF-8 in {path}: {exc}"]
    except json.JSONDecodeError as exc:
        return None, [f"invalid JSON in {path}: {exc}"]


def string_list(value: Any, field: str, where: str, errors: list[str]) -> list[str]:
    if not isinstance(value, list) or any(not isinstance(item, str) for item in value):
        errors.append(f"{where}: {field} must be a string list")
        return []
    if len(value) != len(set(value)):
        errors.append(f"{where}: {field} contains duplicates")
    return value


def load_domain_skill_policy(root: Path, errors: list[str]) -> set[str]:
    """Load the canonical domain roster from the independent records gate."""

    policy_path = root / "tools" / "check-agent-records.py"
    try:
        namespace = runpy.run_path(str(policy_path))
    except (OSError, RuntimeError, SyntaxError) as exc:
        errors.append(f"cannot load domain-skill policy from {policy_path}: {exc}")
        return set()
    value = namespace.get("DOMAIN_SKILL_SLUGS")
    if not isinstance(value, set) or any(not isinstance(item, str) for item in value):
        errors.append(
            f"{policy_path}: DOMAIN_SKILL_SLUGS must be a set of string slugs"
        )
        return set()
    return set(value)


def corpus_sha256(corpus: dict[str, Any]) -> str:
    encoded = json.dumps(
        corpus,
        ensure_ascii=False,
        sort_keys=True,
        separators=(",", ":"),
    ).encode("utf-8")
    return hashlib.sha256(encoded).hexdigest()


def routing_inputs_sha256(
    root: Path, corpus: dict[str, Any]
) -> tuple[str | None, list[str]]:
    """Hash the corpus and repository files that drive domain-skill routing."""

    domains = corpus.get("domain_skills")
    if not isinstance(domains, list) or any(not isinstance(item, str) for item in domains):
        return None, ["cannot hash routing inputs without valid domain_skills"]

    relative_paths = [Path("agent/skills/README.md")]
    for slug in sorted(domains):
        relative_paths.extend(
            (
                Path("agent/skills") / slug / "SKILL.md",
                Path("agent/skills") / slug / "agents/openai.yaml",
            )
        )

    files: list[dict[str, str]] = []
    errors: list[str] = []
    for relative_path in relative_paths:
        path = root / relative_path
        try:
            content = path.read_bytes()
        except OSError as exc:
            errors.append(f"cannot read routing input {relative_path}: {exc}")
            continue
        files.append(
            {
                "path": relative_path.as_posix(),
                "sha256": hashlib.sha256(content).hexdigest(),
            }
        )
    if errors:
        return None, errors

    payload = {
        "corpus_sha256": corpus_sha256(corpus),
        "files": files,
    }
    encoded = json.dumps(payload, sort_keys=True, separators=(",", ":")).encode("utf-8")
    return hashlib.sha256(encoded).hexdigest(), []


def normalized_prompt(prompt: str) -> str:
    """Normalize superficial Unicode and whitespace variation for uniqueness."""

    return " ".join(unicodedata.normalize("NFKC", prompt).casefold().split())


def validate_corpus(root: Path, corpus: Any) -> list[str]:
    errors: list[str] = []
    if not isinstance(corpus, dict):
        return ["corpus root must be an object"]
    unknown_corpus_fields = set(corpus) - CORPUS_FIELDS
    if unknown_corpus_fields:
        errors.append(
            "corpus has unknown fields: " + ", ".join(sorted(unknown_corpus_fields))
        )
    if corpus.get("schema_version") != 1:
        errors.append("corpus schema_version must be 1")

    domains = string_list(corpus.get("domain_skills"), "domain_skills", "corpus", errors)
    if not domains:
        errors.append("domain_skills must be non-empty")
    domain_set = set(domains)
    expected_domains = load_domain_skill_policy(root, errors)
    if domain_set != expected_domains:
        missing = sorted(expected_domains - domain_set)
        extra = sorted(domain_set - expected_domains)
        detail = []
        if missing:
            detail.append("missing " + ", ".join(missing))
        if extra:
            detail.append("unexpected " + ", ".join(extra))
        errors.append("domain_skills must match the records-gate roster: " + "; ".join(detail))
    for slug in domains:
        if not SKILL_SLUG_RE.fullmatch(slug):
            errors.append(f"domain_skills: invalid slug {slug!r}")
        elif not (root / "agent" / "skills" / slug / "SKILL.md").is_file():
            errors.append(f"domain_skills: missing package {slug}")

    coverage = corpus.get("coverage")
    if not isinstance(coverage, dict):
        errors.append("coverage must be an object")
        coverage = {}
    unknown_coverage_fields = set(coverage) - COVERAGE_FIELDS
    if unknown_coverage_fields:
        errors.append(
            "coverage has unknown fields: "
            + ", ".join(sorted(unknown_coverage_fields))
        )
    locales = string_list(
        coverage.get("required_locales"), "required_locales", "coverage", errors
    )
    if locales != list(REQUIRED_LOCALES):
        errors.append(
            "coverage.required_locales must equal " + repr(list(REQUIRED_LOCALES))
        )
    positive_min = coverage.get("minimum_positive_per_skill")
    negative_min = coverage.get("minimum_near_miss_per_skill")
    if not isinstance(positive_min, dict):
        errors.append("coverage.minimum_positive_per_skill must be an object")
        positive_min = {}
    if not isinstance(negative_min, dict):
        errors.append("coverage.minimum_near_miss_per_skill must be an object")
        negative_min = {}
    if positive_min != MINIMUM_POSITIVE_PER_SKILL:
        errors.append(
            "coverage.minimum_positive_per_skill must equal "
            + repr(MINIMUM_POSITIVE_PER_SKILL)
        )
    if negative_min != MINIMUM_NEAR_MISS_PER_SKILL:
        errors.append(
            "coverage.minimum_near_miss_per_skill must equal "
            + repr(MINIMUM_NEAR_MISS_PER_SKILL)
        )
    minimum_compositions = coverage.get("minimum_composition_cases")
    if minimum_compositions != MINIMUM_COMPOSITION_CASES:
        errors.append(
            "coverage.minimum_composition_cases must equal "
            f"{MINIMUM_COMPOSITION_CASES}"
        )

    cases = corpus.get("cases")
    if not isinstance(cases, list) or not cases:
        errors.append("cases must be a non-empty list")
        return errors

    seen_ids: set[str] = set()
    seen_prompts: dict[tuple[str, str], str] = {}
    positives: Counter[tuple[str, str]] = Counter()
    negatives: Counter[tuple[str, str]] = Counter()
    composition_count = 0
    for index, case in enumerate(cases, start=1):
        where = f"case[{index}]"
        if not isinstance(case, dict):
            errors.append(f"{where}: must be an object")
            continue
        unknown = set(case) - CASE_FIELDS
        if unknown:
            errors.append(f"{where}: unknown fields: {', '.join(sorted(unknown))}")
        case_id = case.get("id")
        if not isinstance(case_id, str) or not CASE_ID_RE.fullmatch(case_id):
            errors.append(f"{where}: invalid id")
            case_id = where
        elif case_id in seen_ids:
            errors.append(f"{where}: duplicate id {case_id}")
        seen_ids.add(case_id)
        where = case_id

        kind = case.get("kind")
        if kind not in CASE_KINDS:
            errors.append(f"{where}: invalid kind {kind!r}")
        locale = case.get("locale")
        if locale not in REQUIRED_LOCALES:
            errors.append(f"{where}: locale {locale!r} is not required_locales")
        prompt = case.get("prompt")
        if not isinstance(prompt, str) or not prompt.strip():
            errors.append(f"{where}: prompt must be non-empty")
        elif isinstance(locale, str):
            prompt_key = (locale, normalized_prompt(prompt))
            previous = seen_prompts.get(prompt_key)
            if previous is not None:
                errors.append(
                    f"{where}: duplicate prompt for locale {locale!r} "
                    f"(already used by {previous})"
                )
            else:
                seen_prompts[prompt_key] = where

        expected = string_list(case.get("expected_skills"), "expected_skills", where, errors)
        forbidden = string_list(
            case.get("forbidden_skills"), "forbidden_skills", where, errors
        )
        if set(expected) & set(forbidden):
            errors.append(f"{where}: expected_skills and forbidden_skills overlap")
        for slug in expected + forbidden:
            if slug not in expected_domains:
                errors.append(f"{where}: unknown domain skill {slug!r}")
        allow_additional = case.get("allow_additional_domain_skills", False)
        if not isinstance(allow_additional, bool):
            errors.append(f"{where}: allow_additional_domain_skills must be boolean")

        if kind == "single" and len(expected) != 1:
            errors.append(f"{where}: single case must expect exactly one domain skill")
        if kind == "near-miss" and not forbidden:
            errors.append(f"{where}: near-miss case must forbid at least one domain skill")
        if kind == "composition":
            composition_count += 1
            if len(expected) < 2:
                errors.append(f"{where}: composition case must expect at least two skills")

        if isinstance(locale, str):
            if kind == "single":
                for slug in expected:
                    positives[(slug, locale)] += 1
            if kind == "near-miss":
                for slug in forbidden:
                    negatives[(slug, locale)] += 1

    for slug in expected_domains:
        for locale in REQUIRED_LOCALES:
            positive_required = MINIMUM_POSITIVE_PER_SKILL[locale]
            negative_required = MINIMUM_NEAR_MISS_PER_SKILL[locale]
            if positives[(slug, locale)] < positive_required:
                errors.append(
                    f"coverage: {slug} has {positives[(slug, locale)]} {locale} "
                    f"positive case(s), requires {positive_required}"
                )
            if negatives[(slug, locale)] < negative_required:
                errors.append(
                    f"coverage: {slug} has {negatives[(slug, locale)]} {locale} "
                    f"near-miss case(s), requires {negative_required}"
                )
    if composition_count < MINIMUM_COMPOSITION_CASES:
        errors.append(
            f"coverage: {composition_count} composition case(s), "
            f"requires {MINIMUM_COMPOSITION_CASES}"
        )
    _, routing_errors = routing_inputs_sha256(root, corpus)
    errors.extend(routing_errors)
    return errors


def build_observation_template(
    root: Path, corpus: dict[str, Any], repetitions: int
) -> dict[str, Any]:
    routing_digest, errors = routing_inputs_sha256(root, corpus)
    if errors or routing_digest is None:
        raise ValueError("; ".join(errors))
    runs = []
    for case in corpus["cases"]:
        for iteration in range(1, repetitions + 1):
            runs.append(
                {
                    "case_id": case["id"],
                    "iteration": iteration,
                    "selected_skills": [],
                }
            )
    return {
        "schema_version": 2,
        "corpus_sha256": corpus_sha256(corpus),
        "routing_inputs_sha256": routing_digest,
        "runner": {
            "product": "Codex",
            "model": "MODEL",
            "host": "HOST",
            "recorded_at": dt.date.today().isoformat(),
            "repetitions": repetitions,
        },
        "runs": runs,
    }


def validate_observations(root: Path, corpus: dict[str, Any], observed: Any) -> list[str]:
    errors: list[str] = []
    if not isinstance(observed, dict):
        return ["observations root must be an object"]
    unknown_observation_fields = set(observed) - OBSERVATION_FIELDS
    if unknown_observation_fields:
        errors.append(
            "observations has unknown fields: "
            + ", ".join(sorted(unknown_observation_fields))
        )
    missing_observation_fields = OBSERVATION_FIELDS - set(observed)
    if missing_observation_fields:
        errors.append(
            "observations missing fields: "
            + ", ".join(sorted(missing_observation_fields))
        )
    if observed.get("schema_version") != 2:
        errors.append("observations schema_version must be 2")
    digest = observed.get("corpus_sha256")
    if not isinstance(digest, str) or not SHA256_RE.fullmatch(digest):
        errors.append("observations.corpus_sha256 must be a lowercase SHA-256")
    elif digest != corpus_sha256(corpus):
        errors.append("observations.corpus_sha256 does not match the current corpus")
    routing_digest = observed.get("routing_inputs_sha256")
    if not isinstance(routing_digest, str) or not SHA256_RE.fullmatch(routing_digest):
        errors.append("observations.routing_inputs_sha256 must be a lowercase SHA-256")
    else:
        expected_routing_digest, routing_errors = routing_inputs_sha256(root, corpus)
        errors.extend(routing_errors)
        if expected_routing_digest is not None and routing_digest != expected_routing_digest:
            errors.append(
                "observations.routing_inputs_sha256 does not match the current "
                "domain skill routing inputs"
            )
    runner = observed.get("runner")
    if not isinstance(runner, dict):
        errors.append("observations.runner must be an object")
        return errors
    unknown_runner = set(runner) - RUNNER_FIELDS
    if unknown_runner:
        errors.append(
            "observations.runner has unknown fields: "
            + ", ".join(sorted(unknown_runner))
        )
    missing_runner = RUNNER_FIELDS - set(runner)
    if missing_runner:
        errors.append(
            "observations.runner missing fields: " + ", ".join(sorted(missing_runner))
        )
    for field in RUNNER_FIELDS - {"repetitions"}:
        value = runner.get(field)
        if not isinstance(value, str) or not value.strip():
            errors.append(f"observations.runner.{field} must be non-empty")
        elif value.strip().upper() in RUNNER_PLACEHOLDERS:
            errors.append(f"observations.runner.{field} still contains a placeholder")
    if runner.get("product") not in RUNNER_PRODUCTS:
        errors.append(
            "observations.runner.product must be one of "
            + repr(sorted(RUNNER_PRODUCTS))
        )
    recorded_at = runner.get("recorded_at")
    if isinstance(recorded_at, str) and recorded_at.strip():
        try:
            recorded_date = dt.date.fromisoformat(recorded_at)
        except ValueError:
            errors.append("observations.runner.recorded_at must be an ISO date")
        else:
            if recorded_date > dt.date.today():
                errors.append("observations.runner.recorded_at must not be in the future")
    repetitions = runner.get("repetitions")
    if not isinstance(repetitions, int) or isinstance(repetitions, bool) or repetitions < 1:
        errors.append("observations.runner.repetitions must be a positive integer")
        return errors

    all_skills = {
        path.parent.name
        for path in (root / "agent" / "skills").glob("*/SKILL.md")
    }
    domain_skills = set(corpus["domain_skills"])
    cases = {case["id"]: case for case in corpus["cases"]}
    runs = observed.get("runs")
    if not isinstance(runs, list):
        return errors + ["observations.runs must be a list"]

    seen: defaultdict[str, list[int]] = defaultdict(list)
    for index, run in enumerate(runs, start=1):
        where = f"run[{index}]"
        if not isinstance(run, dict):
            errors.append(f"{where}: must be an object")
            continue
        if set(run) != {"case_id", "iteration", "selected_skills"}:
            errors.append(f"{where}: fields must be case_id, iteration, selected_skills")
        case_id = run.get("case_id")
        if case_id not in cases:
            errors.append(f"{where}: unknown case_id {case_id!r}")
            continue
        iteration = run.get("iteration")
        if not isinstance(iteration, int) or isinstance(iteration, bool):
            errors.append(f"{where}: iteration must be an integer")
            continue
        seen[case_id].append(iteration)
        selected = string_list(run.get("selected_skills"), "selected_skills", where, errors)
        unknown = set(selected) - all_skills
        if unknown:
            errors.append(f"{where}: unknown selected skills: {', '.join(sorted(unknown))}")

        case = cases[case_id]
        selected_domains = set(selected) & domain_skills
        expected = set(case["expected_skills"])
        forbidden = set(case["forbidden_skills"])
        missing = expected - selected_domains
        forbidden_loaded = forbidden & selected_domains
        unexpected = selected_domains - expected
        if missing:
            errors.append(f"{where}: missing expected skills: {', '.join(sorted(missing))}")
        if forbidden_loaded:
            errors.append(
                f"{where}: loaded forbidden skills: {', '.join(sorted(forbidden_loaded))}"
            )
        if unexpected and not case.get("allow_additional_domain_skills", False):
            errors.append(
                f"{where}: loaded unexpected domain skills: {', '.join(sorted(unexpected))}"
            )

    expected_iterations = list(range(1, repetitions + 1))
    for case_id in cases:
        if sorted(seen[case_id]) != expected_iterations:
            errors.append(
                f"observations: {case_id} iterations {sorted(seen[case_id])} "
                f"do not equal {expected_iterations}"
            )
    return errors


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "root",
        nargs="?",
        type=Path,
        default=Path(__file__).resolve().parents[1],
        help="repository root (defaults to the script's parent repository)",
    )
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--observed", type=Path, help="captured routing observations")
    mode.add_argument(
        "--emit-template",
        action="store_true",
        help="emit an observation JSON template to stdout",
    )
    parser.add_argument("--repetitions", type=int, default=3)
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    root = args.root.resolve()
    corpus_path = root / "agent" / "skills" / "trigger-evals.json"
    corpus, errors = read_json(corpus_path)
    if not errors:
        errors.extend(validate_corpus(root, corpus))
    if errors:
        for error in errors:
            print(f"skill routing error: {error}", file=sys.stderr)
        return 1

    assert isinstance(corpus, dict)
    if args.emit_template:
        if args.repetitions < 1:
            print("skill routing error: repetitions must be positive", file=sys.stderr)
            return 1
        json.dump(
            build_observation_template(root, copy.deepcopy(corpus), args.repetitions),
            sys.stdout,
            ensure_ascii=False,
            indent=2,
        )
        print()
        return 0

    if args.observed is not None:
        observed, observation_errors = read_json(args.observed)
        if not observation_errors:
            observation_errors.extend(validate_observations(root, corpus, observed))
        if observation_errors:
            for error in observation_errors:
                print(f"skill routing error: {error}", file=sys.stderr)
            return 1
        assert isinstance(observed, dict)
        repetitions = observed["runner"]["repetitions"]
        print(
            f"skill routing observations: ok ({len(corpus['cases'])} cases, "
            f"{repetitions} repetition(s))"
        )
        return 0

    print(
        f"skill routing corpus: ok ({len(corpus['domain_skills'])} domain skills, "
        f"{len(corpus['cases'])} cases)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
