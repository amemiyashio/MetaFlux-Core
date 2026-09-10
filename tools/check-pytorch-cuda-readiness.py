#!/usr/bin/env python3
"""Check declared PyTorch CPU coverage and its canonical summary, not execution."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import sys


CORPUS = Path("tests/compatibility/pytorch_cuda_cpu_frontier_corpus_v1.json")
WORK_ITEM = Path("agent/plan/milestone-0.2.0.0-pytorch-cuda-compatibility/work/work-item-0.2.0.2-torch-kernel-intake.md")
WORK_ID = "work-item-0.2.0.2"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def identifiers(rows: object, name: str) -> list[str]:
    require(isinstance(rows, list), f"{name} must be a list")
    result = []
    for row in rows:
        require(isinstance(row, dict), f"{name} row must be an object")
        identifier = row.get("id")
        require(isinstance(identifier, str) and bool(identifier), f"{name} row needs an id")
        result.append(identifier)
    require(len(set(result)) == len(result), f"duplicate {name} identifier")
    return result


def coverage(root: Path, corpus: dict) -> tuple[dict[str, int], list[str]]:
    require(corpus.get("schema_version") == 1, "unsupported corpus schema")
    case_ids = identifiers(corpus.get("cases"), "cases")
    gap_ids = identifiers(corpus.get("gaps"), "gaps")
    for gap in corpus["gaps"]:
        require(gap.get("status") == "frontier-gap" and
                isinstance(gap.get("expected_error"), str) and bool(gap["expected_error"].strip()),
                "classified gaps need frontier-gap status and a non-empty expected_error")
    require(bool(case_ids), "the declared corpus must contain cases")
    require(not set(case_ids) & set(gap_ids), "case and gap identifiers overlap")
    scope = corpus.get("scope")
    require(isinstance(scope, dict), "corpus scope must be an object")
    require(type(scope.get("exit_gate_complete")) is bool, "exit_gate_complete must be boolean")
    subset = scope.get("compiled_subset")
    require(isinstance(subset, list) and all(isinstance(x, str) for x in subset),
            "compiled_subset must be a list of ids")
    require(len(set(subset)) == len(subset), "duplicate compiled_subset identifier")
    marked = [row for row in corpus["cases"] if "compiled" in row]
    require(subset == [row["id"] for row in marked],
            "compiled_subset differs from ordered cases with compiled sources")
    sources: set[str] = set()
    for row in marked:
        compiled = row["compiled"]
        require(isinstance(compiled, dict), "compiled source must be an object")
        source = compiled.get("ptx")
        require(isinstance(source, str) and bool(source), "compiled row needs a PTX source")
        path = Path(source)
        require(not path.is_absolute() and ".." not in path.parts,
                f"compiled source must be repository-relative: {source}")
        resolved = (root / path).resolve()
        require(resolved.is_relative_to(root.resolve()) and resolved.is_file(),
                f"missing or external compiled source: {source}")
        sources.add(resolved.relative_to(root.resolve()).as_posix())
    remaining = [identifier for identifier in case_ids if identifier not in set(subset)]
    return {
        "Supported cases": len(case_ids),
        "Compiled cases": len(marked),
        "Unique compiled PTX sources": len(sources),
        "Cases outside compiled subset": len(remaining),
        "Classified gaps": len(gap_ids),
    }, remaining


def validate(root: Path) -> dict:
    corpus = json.loads((root / CORPUS).read_text(encoding="utf-8"))
    require(isinstance(corpus, dict), "corpus must be an object")
    counts, remaining = coverage(root, corpus)
    text = (root / WORK_ITEM).read_text(encoding="utf-8")
    # Compare semantic coverage data in the owning document, not prose wording.
    for label, count in counts.items():
        matches = re.findall(r"^\|\s*" + re.escape(label) + r"\s*\|\s*(\d+)\s*\|\s*$", text, re.MULTILINE)
        require(len(matches) == 1 and int(matches[0]) == count,
                f"{WORK_ITEM}: {label} must occur once with derived count {count}")
    require(text.startswith("---\n") and "\n---\n" in text[4:], "work item needs frontmatter")
    frontmatter = text.split("---", 2)[1]
    status = re.findall(r"^status:\s*(\S+)\s*$", frontmatter, re.MULTILINE)
    require(len(status) == 1, "work item needs one status")
    goal = json.loads((root / "agent/goal.json").read_text(encoding="utf-8"))
    require(isinstance(goal, dict) and isinstance(goal.get("lanes"), list),
            "goal must be an object with a lanes list")
    require(all(isinstance(lane, dict) for lane in goal["lanes"]), "goal lane must be an object")
    accepted = status[0] == "Complete" or any(
        lane.get("work_item") == WORK_ID and lane.get("status") == "integrated"
        for lane in goal["lanes"]
    )
    declared_complete = corpus["scope"]["exit_gate_complete"]
    require(corpus.get("status") in {"frontier-not-frozen", "frozen"}, "unknown corpus status")
    if declared_complete or accepted:
        require(corpus["status"] == "frozen" and not remaining,
                "CPU completion requires a frozen corpus with every accepted case compiled")
    if accepted:
        require(declared_complete, "accepted CPU work item lacks declared full Exit Gate")
    return {"status": "consistent-declarations", "counts": counts,
            "remaining_case_ids": remaining, "execution_verified": False}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", type=Path, nargs="?", default=Path("."))
    args = parser.parse_args()
    try:
        result = validate(args.root.resolve())
    except (OSError, ValueError, KeyError, TypeError) as error:
        print(f"PyTorch readiness: {error}", file=sys.stderr)
        return 1
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
