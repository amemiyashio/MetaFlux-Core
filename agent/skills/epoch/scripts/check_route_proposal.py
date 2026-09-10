#!/usr/bin/env python3
"""Validate a transient epoch proposal without writing repository state."""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
from pathlib import Path
from typing import Any


EPOCH_RE = re.compile(r"epoch-(\d{4})")
REVISION_RE = re.compile(r"[0-9a-f]{40}")
NODE_RE = re.compile(r"[a-z0-9]+(?:[.-][a-z0-9]+)*")
DISPOSITIONS = {"keep", "reorder", "rewrite", "delete"}


class ProposalError(ValueError):
    pass


def isolated_git_environment() -> dict[str, str]:
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
        env=isolated_git_environment(),
        check=False,
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        raise ProposalError(result.stderr.strip() or "Git command failed")
    return result.stdout.strip()


def require_string(value: Any, name: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise ProposalError(f"{name} must be a non-empty string")
    return value


def find_cycle(graph: dict[str, list[str]]) -> list[str] | None:
    visiting: set[str] = set()
    visited: set[str] = set()
    stack: list[str] = []

    def visit(node: str) -> list[str] | None:
        if node in visited:
            return None
        if node in visiting:
            start = stack.index(node)
            return stack[start:] + [node]
        visiting.add(node)
        stack.append(node)
        for dependency in graph[node]:
            cycle = visit(dependency)
            if cycle is not None:
                return cycle
        stack.pop()
        visiting.remove(node)
        visited.add(node)
        return None

    for node in graph:
        cycle = visit(node)
        if cycle is not None:
            return cycle
    return None


def validate_graph(route: Any) -> None:
    if not isinstance(route, dict):
        raise ProposalError("route must be an object")
    require_string(route.get("objective"), "route.objective")
    require_string(route.get("observable_success"), "route.observable_success")
    nodes = route.get("nodes")
    if not isinstance(nodes, list) or not nodes:
        raise ProposalError("route.nodes must be a non-empty list")
    graph: dict[str, list[str]] = {}
    for index, node in enumerate(nodes):
        if not isinstance(node, dict):
            raise ProposalError(f"route.nodes[{index}] must be an object")
        node_id = require_string(node.get("id"), f"route.nodes[{index}].id")
        if NODE_RE.fullmatch(node_id) is None:
            raise ProposalError(f"invalid route node id: {node_id}")
        if node_id in graph:
            raise ProposalError(f"duplicate route node id: {node_id}")
        if node.get("disposition") not in DISPOSITIONS:
            raise ProposalError(f"invalid disposition for {node_id}")
        dependencies = node.get("depends_on")
        if not isinstance(dependencies, list) or any(
            not isinstance(value, str) for value in dependencies
        ):
            raise ProposalError(f"depends_on for {node_id} must be a string list")
        graph[node_id] = dependencies
    unresolved = sorted(
        dependency
        for dependencies in graph.values()
        for dependency in dependencies
        if dependency not in graph
    )
    if unresolved:
        raise ProposalError(f"unresolved route dependencies: {unresolved}")
    cycle = find_cycle(graph)
    if cycle is not None:
        raise ProposalError(f"route DAG contains a cycle: {' -> '.join(cycle)}")


def next_epoch(epoch: str) -> str:
    match = EPOCH_RE.fullmatch(epoch)
    if match is None:
        raise ProposalError("baseline_epoch must match epoch-NNNN")
    if int(match.group(1)) >= 9999:
        raise ProposalError("Epoch numbering is exhausted; no wrap is permitted")
    return f"epoch-{int(match.group(1)) + 1:04d}"


def evaluate(document: Any, root: Path, *, confirmed: bool) -> str:
    if not isinstance(document, dict):
        raise ProposalError("proposal must be an object")
    if document.get("schema_version") != 1:
        raise ProposalError("schema_version must be 1")

    focus = document.get("focus")
    candidates = document.get("candidates")
    route = document.get("route")
    if focus is None:
        if not isinstance(candidates, list) or not 1 <= len(candidates) <= 3:
            raise ProposalError("an absent focus requires one to three candidates")
        for index, candidate in enumerate(candidates):
            if not isinstance(candidate, dict):
                raise ProposalError(f"candidates[{index}] must be an object")
            require_string(candidate.get("objective"), f"candidates[{index}].objective")
            require_string(
                candidate.get("observable_success"),
                f"candidates[{index}].observable_success",
            )
        if route is not None or confirmed:
            raise ProposalError("candidate selection cannot authorize governance")
        return "select-target"

    require_string(focus, "focus")
    if candidates not in (None, []):
        raise ProposalError("a focused proposal cannot also carry candidates")
    validate_graph(route)

    baseline_revision = require_string(
        document.get("baseline_revision"), "baseline_revision"
    )
    if REVISION_RE.fullmatch(baseline_revision) is None:
        raise ProposalError("baseline_revision must be a full lowercase Git OID")
    baseline_epoch = require_string(document.get("baseline_epoch"), "baseline_epoch")
    current_revision = git(root, "rev-parse", "HEAD")
    if current_revision != baseline_revision:
        raise ProposalError("proposal baseline changed; rerun the read-only audit")
    if git(root, "status", "--porcelain", "--untracked-files=all"):
        raise ProposalError("proposal requires a clean worktree")

    goal = json.loads((root / "agent/goal.json").read_text(encoding="utf-8"))
    if goal.get("epoch") != baseline_epoch:
        raise ProposalError("active Epoch changed; rerun the read-only audit")

    semantic_change = document.get("semantic_change")
    if not isinstance(semantic_change, bool):
        raise ProposalError("semantic_change must be boolean")
    proposed_epoch = require_string(document.get("proposed_epoch"), "proposed_epoch")
    if not semantic_change:
        if proposed_epoch != baseline_epoch:
            raise ProposalError("a no-op must keep the active Epoch")
        return "no-op"
    if proposed_epoch != next_epoch(baseline_epoch):
        raise ProposalError("a semantic replan must propose the next Epoch")
    return "ready-to-govern" if confirmed else "await-confirmation"


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(description=__doc__)
    result.add_argument("proposal", type=Path)
    result.add_argument("--root", type=Path, default=Path("."))
    result.add_argument("--confirm", action="store_true")
    return result


def main() -> int:
    arguments = parser().parse_args()
    try:
        document = json.loads(arguments.proposal.read_text(encoding="utf-8"))
        action = evaluate(document, arguments.root.resolve(), confirmed=arguments.confirm)
    except (OSError, UnicodeDecodeError, json.JSONDecodeError, ProposalError) as error:
        print(f"replan roadmap proposal: invalid: {error}", file=sys.stderr)
        return 1
    print(f"replan roadmap proposal: {action}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
