#!/usr/bin/env python3
"""Validate one committed Iteration delivery and advance canonical route state."""

from __future__ import annotations

import argparse
import copy
import datetime as dt
import json
import os
import re
import subprocess
import sys
import tempfile
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any, Callable


REPOSITORY_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPOSITORY_ROOT / "tools"))

from agent_diagnostics import (  # noqa: E402
    DiagnosticArgumentParser,
    DiagnosticError,
    add_diagnostic_format_argument,
    emit_diagnostics,
    task_stop_error,
)


SOURCE = "accept-and-advance"
REVISION_RE = re.compile(r"[0-9a-f]{40}")
EPOCH_RE = re.compile(r"epoch-[0-9]{4}")
BATCH_RE = re.compile(r"batch-[0-9]{4}")
ITERATION_RE = re.compile(r"iteration-[0-9]{4}")
LANE_RE = re.compile(r"lane-[a-z0-9]+(?:-[a-z0-9]+)*")
WORK_ITEM_RE = re.compile(r"work-item-[0-9]+\.[0-9]+\.[0-9]+\.[1-9][0-9]*")
DELIVERY_FIELDS = {
    "schema_version",
    "epoch",
    "batch",
    "iteration",
    "lane",
    "base_revision",
    "tip_revision",
    "tests",
    "blockers",
    "roast_candidates",
}
TEST_FIELDS = {"command", "status"}


@dataclass(frozen=True)
class Assessment:
    action: str
    epoch: str
    batch: str
    iteration: str
    lane: str
    work_item: str
    base_revision: str
    tip_revision: str
    changed_paths: tuple[str, ...]
    next_lane: str | None
    next_iteration: str | None
    next_work_item: str | None


def stop(
    *,
    code: str,
    summary: str,
    evidence: tuple[object, ...],
    responsibility: str = "current-agent",
    disposition: str = "fix-and-retry",
    required_action: str,
    resume_when: str,
) -> None:
    raise task_stop_error(
        code=code,
        source=SOURCE,
        summary=summary,
        evidence=evidence,
        responsibility=responsibility,
        disposition=disposition,
        required_action=required_action,
        resume_when=resume_when,
    )


def isolated_git_environment(root: Path) -> dict[str, str]:
    environment = dict(os.environ)
    result = subprocess.run(
        ["git", "rev-parse", "--local-env-vars"],
        cwd=root,
        env=environment,
        check=False,
        capture_output=True,
        text=True,
    )
    if result.returncode == 0:
        for variable in result.stdout.splitlines():
            environment.pop(variable, None)
    return environment


def git(root: Path, *arguments: str, check: bool = True) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(
        ["git", *arguments],
        cwd=root,
        env=isolated_git_environment(root),
        check=False,
        capture_output=True,
        text=True,
    )
    if check and result.returncode != 0:
        stop(
            code="acceptance.context-invalid",
            summary="Git could not validate the supplied integration context.",
            evidence=("git " + " ".join(arguments), result.stderr or result.stdout),
            responsibility="batch-integrator",
            disposition="preserve-and-report",
            required_action="Restore the supplied current-main integration context and retry the same check.",
            resume_when="The exact Git query succeeds without changing the candidate.",
        )
    return result


def require_string(value: Any, field: str, pattern: re.Pattern[str] | None = None) -> str:
    if not isinstance(value, str) or not value.strip():
        stop(
            code="acceptance.delivery-invalid",
            summary="The Iteration delivery manifest is malformed.",
            evidence=(f"{field} must be non-empty text",),
            required_action="Regenerate the transient delivery manifest from the committed Iteration output.",
            resume_when="Every delivery field matches the accept-and-advance contract.",
        )
    if pattern is not None and pattern.fullmatch(value) is None:
        stop(
            code="acceptance.delivery-invalid",
            summary="The Iteration delivery manifest is malformed.",
            evidence=(f"{field} has an invalid value: {value}",),
            required_action="Regenerate the transient delivery manifest with the exact governed identifier.",
            resume_when="Every delivery field matches the accept-and-advance contract.",
        )
    return value


def validate_manifest(document: Any) -> dict[str, Any]:
    if not isinstance(document, dict) or set(document) != DELIVERY_FIELDS:
        actual = sorted(document) if isinstance(document, dict) else type(document).__name__
        stop(
            code="acceptance.delivery-invalid",
            summary="The Iteration delivery manifest has the wrong shape.",
            evidence=(f"expected fields: {sorted(DELIVERY_FIELDS)}", f"actual: {actual}"),
            required_action="Emit exactly the accept-and-advance delivery fields from start-work.",
            resume_when="The transient manifest has schema version 1 and no extra fields.",
        )
    if document.get("schema_version") != 1:
        stop(
            code="acceptance.delivery-invalid",
            summary="The Iteration delivery manifest uses an unsupported schema.",
            evidence=(f"schema_version: {document.get('schema_version')}",),
            required_action="Emit delivery schema version 1.",
            resume_when="schema_version is exactly 1.",
        )
    require_string(document.get("epoch"), "epoch", EPOCH_RE)
    require_string(document.get("batch"), "batch", BATCH_RE)
    require_string(document.get("iteration"), "iteration", ITERATION_RE)
    require_string(document.get("lane"), "lane", LANE_RE)
    require_string(document.get("base_revision"), "base_revision", REVISION_RE)
    require_string(document.get("tip_revision"), "tip_revision", REVISION_RE)

    tests = document.get("tests")
    if not isinstance(tests, list) or not tests:
        stop(
            code="acceptance.not-qualified",
            summary="The Iteration has no focused-test evidence.",
            evidence=("tests must contain at least one exact command and passed status",),
            required_action="Run the assigned focused gates and emit their exact commands and results.",
            resume_when="Every focused test is present and reports passed.",
        )
    for index, test in enumerate(tests):
        if not isinstance(test, dict) or set(test) != TEST_FIELDS:
            stop(
                code="acceptance.delivery-invalid",
                summary="A focused-test entry is malformed.",
                evidence=(f"tests[{index}] must contain exactly command and status",),
                required_action="Regenerate the test evidence entry without extra or missing fields.",
                resume_when="Every test entry matches the delivery contract.",
            )
        require_string(test.get("command"), f"tests[{index}].command")
        if test.get("status") != "passed":
            stop(
                code="acceptance.not-qualified",
                summary="The Iteration focused tests are not all passing.",
                evidence=(f"tests[{index}] status: {test.get('status')}",),
                required_action="Repair the same lane and rerun its focused gates before acceptance.",
                resume_when="Every supplied focused test reports passed.",
            )

    blockers = document.get("blockers")
    if not isinstance(blockers, list) or any(not isinstance(item, str) for item in blockers):
        stop(
            code="acceptance.delivery-invalid",
            summary="The blocker list is malformed.",
            evidence=("blockers must be a string list",),
            required_action="Emit blockers as a JSON string list.",
            resume_when="The blocker list matches the delivery contract.",
        )
    if blockers:
        stop(
            code="acceptance.not-qualified",
            summary="The Iteration still has declared blockers.",
            evidence=tuple(blockers),
            required_action="Resolve the blockers in the same lane or report the bounded task stop.",
            resume_when="The committed delivery has no unresolved blockers.",
        )
    roast = document.get("roast_candidates")
    if not isinstance(roast, list) or any(not isinstance(item, str) for item in roast):
        stop(
            code="acceptance.delivery-invalid",
            summary="The roast candidate list is malformed.",
            evidence=("roast_candidates must be a string list",),
            required_action="Emit material roast candidates as JSON strings.",
            resume_when="The roast candidate list matches the delivery contract.",
        )
    return document


def load_goal(root: Path) -> dict[str, Any]:
    path = root / "agent/goal.json"
    try:
        document = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
        stop(
            code="acceptance.context-invalid",
            summary="The active Goal cannot be read.",
            evidence=(str(error),),
            responsibility="batch-integrator",
            disposition="preserve-and-report",
            required_action="Restore valid current Goal authority without changing the delivery.",
            resume_when="agent/goal.json parses and passes the Agent-state gate.",
        )
    return document


def lane_for(goal: dict[str, Any], lane_id: str) -> dict[str, Any]:
    lanes = goal.get("lanes")
    if not isinstance(lanes, list):
        lanes = []
    matches = [lane for lane in lanes if isinstance(lane, dict) and lane.get("id") == lane_id]
    if len(matches) != 1:
        stop(
            code="acceptance.candidate-invalid",
            summary="The delivery lane does not resolve uniquely in the active Goal.",
            evidence=(f"lane: {lane_id}",),
            responsibility="batch-integrator",
            disposition="preserve-and-report",
            required_action="Preserve the candidate and use the exact active lane identity.",
            resume_when="The delivery names one lane in the current Goal.",
        )
    return matches[0]


def require_identity(document: dict[str, Any], goal: dict[str, Any], lane: dict[str, Any]) -> None:
    expected = {
        "epoch": goal.get("epoch"),
        "batch": goal.get("batch", {}).get("id") if isinstance(goal.get("batch"), dict) else None,
        "iteration": lane.get("iteration"),
        "lane": lane.get("id"),
    }
    mismatches = [
        f"{field}: delivery={document.get(field)} goal={value}"
        for field, value in expected.items()
        if document.get(field) != value
    ]
    work_item = lane.get("work_item")
    target = goal.get("target")
    target_work = target.get("work_item") if isinstance(target, dict) else None
    if lane.get("status") == "planned" and work_item != target_work:
        mismatches.append(f"work_item: lane={work_item} target={target_work}")
    if mismatches:
        stop(
            code="acceptance.candidate-invalid",
            summary="The delivery identity does not match the active route.",
            evidence=tuple(mismatches),
            responsibility="batch-integrator",
            disposition="preserve-and-report",
            required_action="Rebase and reverify the candidate against the exact active route identity.",
            resume_when="Epoch, Batch, Iteration, lane, and target work item all match.",
        )


def require_dependencies(goal: dict[str, Any], lane: dict[str, Any]) -> None:
    status = {
        item.get("id"): item.get("status")
        for item in goal.get("lanes", [])
        if isinstance(item, dict)
    }
    missing = [dependency for dependency in lane.get("depends_on", []) if status.get(dependency) != "integrated"]
    if missing:
        stop(
            code="acceptance.candidate-invalid",
            summary="The delivery lane is not dependency-ready.",
            evidence=tuple(f"unaccepted dependency: {item}" for item in missing),
            responsibility="batch-integrator",
            disposition="preserve-and-report",
            required_action="Accept the dependency lanes before this candidate.",
            resume_when="Every declared lane dependency is integrated.",
        )


def require_repository(root: Path, *, clean: bool) -> None:
    top = git(root, "rev-parse", "--show-toplevel").stdout.strip()
    if Path(top).resolve() != root.resolve():
        stop(
            code="acceptance.context-invalid",
            summary="The supplied root is not the active Git worktree root.",
            evidence=(f"supplied: {root.resolve()}", f"git: {top}"),
            responsibility="batch-integrator",
            disposition="preserve-and-report",
            required_action="Run the controller from the supplied integration worktree root.",
            resume_when="The root resolves to the current Git worktree.",
        )
    if clean:
        dirty = git(root, "status", "--porcelain", "--untracked-files=all").stdout.strip()
        if dirty:
            stop(
                code="acceptance.context-invalid",
                summary="The integration context is not clean.",
                evidence=tuple(dirty.splitlines()[:8]),
                responsibility="batch-integrator",
                disposition="preserve-and-report",
                required_action="Preserve unrelated state and supply a clean current-main integration context.",
                resume_when="Git reports no staged, unstaged, or untracked state outside ignored transient input.",
            )


def resolve_commit(root: Path, revision: str, label: str) -> str:
    result = git(root, "rev-parse", "--verify", f"{revision}^{{commit}}", check=False)
    resolved = result.stdout.strip()
    if result.returncode != 0 or resolved != revision:
        stop(
            code="acceptance.candidate-invalid",
            summary="A delivery revision is not the exact committed object requested.",
            evidence=(f"{label}: {revision}", result.stderr),
            responsibility="batch-integrator",
            disposition="preserve-and-report",
            required_action="Supply the full committed base and tip object IDs.",
            resume_when="Both revisions resolve exactly to commit objects.",
        )
    return resolved


def is_ancestor(root: Path, ancestor: str, descendant: str) -> bool:
    return git(root, "merge-base", "--is-ancestor", ancestor, descendant, check=False).returncode == 0


def epoch_activation(root: Path, epoch: str) -> str:
    history = git(root, "rev-list", "--reverse", "HEAD", "--", "agent/goal.json").stdout.splitlines()
    for revision in history:
        shown = git(root, "show", f"{revision}:agent/goal.json", check=False)
        if shown.returncode != 0:
            continue
        try:
            candidate = json.loads(shown.stdout)
        except json.JSONDecodeError:
            continue
        if isinstance(candidate, dict) and candidate.get("epoch") == epoch:
            return revision
    stop(
        code="acceptance.candidate-invalid",
        summary="The active Epoch has no activation commit on current history.",
        evidence=(f"epoch: {epoch}",),
        responsibility="epoch-governor",
        disposition="stop-and-report",
        required_action="Repair or publish the active Epoch before accepting Iterations.",
        resume_when="The current history contains the active Epoch activation commit.",
    )


def require_revision_graph(root: Path, document: dict[str, Any]) -> tuple[str, str, tuple[str, ...]]:
    base = resolve_commit(root, document["base_revision"], "base_revision")
    tip = resolve_commit(root, document["tip_revision"], "tip_revision")
    if base == tip:
        stop(
            code="acceptance.candidate-empty",
            summary="The Iteration delivery contains no candidate revision range.",
            evidence=(f"base and tip are both {base}",),
            required_action="Commit a non-empty assigned-lane change and rerun its focused gates.",
            resume_when="tip is a distinct descendant of base with a non-empty tree diff.",
        )
    if not is_ancestor(root, base, tip):
        stop(
            code="acceptance.candidate-invalid",
            summary="The delivery base is not an ancestor of its tip.",
            evidence=(f"base: {base}", f"tip: {tip}"),
            responsibility="batch-integrator",
            disposition="preserve-and-report",
            required_action="Preserve the candidate and supply its exact ancestral base.",
            resume_when="base is an ancestor of tip.",
        )
    diff = git(root, "diff", "--name-only", base, tip, "--").stdout.splitlines()
    if not diff:
        stop(
            code="acceptance.candidate-empty",
            summary="The Iteration commits produce no tree change.",
            evidence=(f"range: {base}..{tip}",),
            required_action="Commit a non-empty assigned-lane change and rerun its focused gates.",
            resume_when="The exact base..tip tree diff contains at least one path.",
        )
    return base, tip, tuple(diff)


def next_ready(goal: dict[str, Any], accepted_lane: str) -> dict[str, Any] | None:
    statuses = {
        lane["id"]: ("integrated" if lane.get("id") == accepted_lane else lane.get("status"))
        for lane in goal["lanes"]
    }
    ready = [
        lane
        for lane in goal["lanes"]
        if lane.get("id") != accepted_lane
        and lane.get("status") == "planned"
        and all(statuses.get(dependency) == "integrated" for dependency in lane.get("depends_on", []))
    ]
    return min(ready, key=lambda lane: lane["iteration"]) if ready else None


def assessment(
    *,
    action: str,
    document: dict[str, Any],
    lane: dict[str, Any],
    paths: tuple[str, ...],
    following: dict[str, Any] | None,
) -> Assessment:
    return Assessment(
        action=action,
        epoch=document["epoch"],
        batch=document["batch"],
        iteration=document["iteration"],
        lane=document["lane"],
        work_item=lane["work_item"],
        base_revision=document["base_revision"],
        tip_revision=document["tip_revision"],
        changed_paths=paths,
        next_lane=following.get("id") if following else None,
        next_iteration=following.get("iteration") if following else None,
        next_work_item=following.get("work_item") if following else None,
    )


def check_delivery(document: Any, root: Path) -> Assessment:
    delivery = validate_manifest(document)
    root = root.resolve()
    require_repository(root, clean=True)
    goal = load_goal(root)
    lane = lane_for(goal, delivery["lane"])
    require_identity(delivery, goal, lane)
    if lane.get("status") != "planned":
        stop(
            code="acceptance.candidate-invalid",
            summary="The delivery lane is not planned.",
            evidence=(f"lane status: {lane.get('status')}",),
            responsibility="batch-integrator",
            disposition="preserve-and-report",
            required_action="Use the active planned lane or inspect the already accepted state.",
            resume_when="The delivery addresses the current planned lane.",
        )
    require_dependencies(goal, lane)
    base, tip, paths = require_revision_graph(root, delivery)
    head = git(root, "rev-parse", "HEAD").stdout.strip()
    activation = epoch_activation(root, delivery["epoch"])
    if not is_ancestor(root, activation, base) or not is_ancestor(root, base, head):
        stop(
            code="acceptance.candidate-invalid",
            summary="The candidate base is outside current Epoch/main history.",
            evidence=(f"activation: {activation}", f"base: {base}", f"HEAD: {head}"),
            responsibility="batch-integrator",
            disposition="preserve-and-report",
            required_action="Rebase the candidate onto current main at or after Epoch activation and reverify it.",
            resume_when="Epoch activation is an ancestor of base and base is an ancestor of current HEAD.",
        )
    if tip == head:
        action = "in-place"
    elif is_ancestor(root, tip, head):
        stop(
            code="acceptance.candidate-invalid",
            summary="The candidate tip is a stale non-HEAD ancestor.",
            evidence=(f"tip: {tip}", f"HEAD: {head}"),
            responsibility="batch-integrator",
            disposition="preserve-and-report",
            required_action="Rebase and reverify the lane candidate at current main instead of obscuring it in later history.",
            resume_when="The candidate is current HEAD or a clean divergent descendant of its base.",
        )
    else:
        action = "merge"
    return assessment(
        action=action,
        document=delivery,
        lane=lane,
        paths=paths,
        following=next_ready(goal, lane["id"]),
    )


def find_work_item(root: Path, identifier: str) -> Path:
    matches = []
    for path in (root / "agent/plan").glob("milestone-*/work/work-item-*.md"):
        text = path.read_text(encoding="utf-8")
        if re.search(rf"^id: {re.escape(identifier)}$", text, re.MULTILINE):
            matches.append(path)
    if len(matches) != 1:
        stop(
            code="acceptance.state-invalid",
            summary="A route work item does not resolve uniquely.",
            evidence=(f"work item: {identifier}", f"matches: {len(matches)}"),
            responsibility="batch-integrator",
            disposition="preserve-and-report",
            required_action="Repair current plan authority before advancing Goal state.",
            resume_when="Every lane work item resolves to one canonical plan file.",
        )
    return matches[0]


def frontmatter_field(text: str, field: str) -> str | None:
    match = re.search(rf"^{re.escape(field)}: (.+)$", text, re.MULTILINE)
    return match.group(1) if match else None


def update_work_item(text: str, *, expected: set[str], status: str, today: str) -> str:
    current = frontmatter_field(text, "status")
    if current not in expected:
        stop(
            code="acceptance.state-invalid",
            summary="A work-item status conflicts with the route transition.",
            evidence=(f"expected one of: {sorted(expected)}", f"actual: {current}"),
            responsibility="batch-integrator",
            disposition="preserve-and-report",
            required_action="Reconcile the canonical work-item status with the active Goal before acceptance.",
            resume_when="The accepted work item is Active and the following work item is Queued or Active.",
        )
    result = re.sub(r"^status: .+$", f"status: {status}", text, count=1, flags=re.MULTILINE)
    result = re.sub(r"^updated: .+$", f"updated: {today}", result, count=1, flags=re.MULTILINE)
    return result


def write_atomic(path: Path, text: str) -> None:
    with tempfile.NamedTemporaryFile(
        mode="w",
        encoding="utf-8",
        dir=path.parent,
        prefix=f".{path.name}.",
        delete=False,
    ) as stream:
        temporary = Path(stream.name)
        stream.write(text)
        stream.flush()
        os.fsync(stream.fileno())
    os.replace(temporary, path)


def default_state_validator(root: Path) -> None:
    result = subprocess.run(
        [sys.executable, "-B", "tools/check-agent-state.py", "."],
        cwd=root,
        check=False,
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        stop(
            code="acceptance.state-invalid",
            summary="The advanced route failed the Agent-state gate.",
            evidence=(result.stderr or result.stdout,),
            responsibility="batch-integrator",
            disposition="fix-and-retry",
            required_action="Repair the acceptance transition and rerun the same state gate.",
            resume_when="The advanced Goal and plans pass tools/check-agent-state.py.",
        )


def advance_delivery(
    document: Any,
    root: Path,
    *,
    state_validator: Callable[[Path], None] = default_state_validator,
) -> Assessment:
    delivery = validate_manifest(document)
    root = root.resolve()
    goal = load_goal(root)
    lane = lane_for(goal, delivery["lane"])
    require_identity(delivery, goal, lane)
    require_dependencies(goal, lane)
    base, tip, paths = require_revision_graph(root, delivery)
    head = git(root, "rev-parse", "HEAD").stdout.strip()
    activation = epoch_activation(root, delivery["epoch"])
    if not is_ancestor(root, activation, base) or not is_ancestor(root, base, head):
        stop(
            code="acceptance.candidate-invalid",
            summary="The accepted candidate base is outside current Epoch/main history.",
            evidence=(f"activation: {activation}", f"base: {base}", f"HEAD: {head}"),
            responsibility="batch-integrator",
            disposition="preserve-and-report",
            required_action="Rebase and reverify the candidate before advancing state.",
            resume_when="The candidate base belongs to current Epoch/main history.",
        )

    merge_head = git(root, "rev-parse", "--verify", "MERGE_HEAD", check=False)
    in_place = tip == head
    completed_merge = is_ancestor(root, tip, head)
    prepared_merge = merge_head.returncode == 0 and merge_head.stdout.strip() == tip
    if in_place or completed_merge:
        require_repository(root, clean=True)
    elif prepared_merge:
        unresolved = git(root, "diff", "--name-only", "--diff-filter=U", check=False).stdout.strip()
        if unresolved:
            stop(
                code="acceptance.context-invalid",
                summary="The prepared candidate merge still has unresolved conflicts.",
                evidence=tuple(unresolved.splitlines()[:8]),
                responsibility="batch-integrator",
                disposition="preserve-and-report",
                required_action="Resolve bounded composition conflicts and rerun the combined gates.",
                resume_when="The prepared merge has no unmerged paths.",
            )
    else:
        stop(
            code="acceptance.context-invalid",
            summary="The candidate tip is absent from the acceptance context.",
            evidence=(f"tip: {tip}", f"HEAD: {head}"),
            responsibility="batch-integrator",
            disposition="preserve-and-report",
            required_action="Retain an in-place candidate or prepare its exact non-fast-forward merge before advancing state.",
            resume_when="tip is HEAD, an ancestor of HEAD, or the exact prepared MERGE_HEAD.",
        )

    following = next_ready(goal, lane["id"])
    if lane.get("status") == "integrated":
        return assessment(
            action="no-op",
            document=delivery,
            lane=lane,
            paths=(),
            following=following,
        )
    if lane.get("status") != "planned":
        stop(
            code="acceptance.candidate-invalid",
            summary="Only a planned lane can advance.",
            evidence=(f"lane status: {lane.get('status')}",),
            responsibility="batch-integrator",
            disposition="preserve-and-report",
            required_action="Use the active planned lane without skipping deferred work.",
            resume_when="The delivery addresses the current planned lane.",
        )

    new_goal = copy.deepcopy(goal)
    new_lane = lane_for(new_goal, delivery["lane"])
    new_lane["status"] = "integrated"
    following = next_ready(goal, lane["id"])
    if following is None:
        remaining = [item for item in new_goal["lanes"] if item.get("status") == "planned"]
        if remaining:
            stop(
                code="acceptance.state-invalid",
                summary="No planned lane becomes dependency-ready after acceptance.",
                evidence=tuple(item["id"] for item in remaining),
                responsibility="batch-integrator",
                disposition="preserve-and-report",
                required_action="Repair the Goal DAG instead of skipping its unresolved planned lanes.",
                resume_when="Acceptance exposes a dependency-ready lane or closes the Batch.",
            )
        new_goal["batch"]["status"] = "integrated"
    else:
        new_goal["target"]["work_item"] = following["work_item"]

    today = dt.date.today().isoformat()
    current_path = find_work_item(root, lane["work_item"])
    originals: dict[Path, str] = {
        root / "agent/goal.json": (root / "agent/goal.json").read_text(encoding="utf-8"),
        current_path: current_path.read_text(encoding="utf-8"),
    }
    replacements: dict[Path, str] = {
        root / "agent/goal.json": json.dumps(new_goal, indent=2, ensure_ascii=True) + "\n",
        current_path: update_work_item(
            originals[current_path], expected={"Active"}, status="Complete", today=today
        ),
    }
    if following is not None:
        next_path = find_work_item(root, following["work_item"])
        if next_path not in originals:
            originals[next_path] = next_path.read_text(encoding="utf-8")
        replacements[next_path] = update_work_item(
            originals[next_path], expected={"Queued", "Active"}, status="Active", today=today
        )

    try:
        for path, text in replacements.items():
            write_atomic(path, text)
        state_validator(root)
    except BaseException:
        for path, text in originals.items():
            write_atomic(path, text)
        raise

    changed = tuple(sorted(path.relative_to(root).as_posix() for path in replacements))
    return assessment(
        action="advanced",
        document=delivery,
        lane=lane,
        paths=changed,
        following=following,
    )


def parser() -> argparse.ArgumentParser:
    result = DiagnosticArgumentParser(description=__doc__, diagnostic_source=SOURCE)
    result.add_argument("action", choices=("check", "advance"))
    result.add_argument("delivery", type=Path)
    result.add_argument("--root", type=Path, default=Path("."))
    add_diagnostic_format_argument(result)
    return result


def main() -> int:
    arguments = parser().parse_args()
    try:
        document = json.loads(arguments.delivery.read_text(encoding="utf-8"))
        if arguments.action == "check":
            result = check_delivery(document, arguments.root)
        else:
            result = advance_delivery(document, arguments.root)
    except DiagnosticError as error:
        emit_diagnostics((error.diagnostic,), diagnostic_format=arguments.diagnostic_format)
        return 1
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
        diagnostic = task_stop_error(
            code="acceptance.delivery-invalid",
            source=SOURCE,
            summary="The transient delivery manifest cannot be read.",
            evidence=(str(error),),
            responsibility="current-agent",
            disposition="fix-and-retry",
            required_action="Write valid delivery schema version 1 JSON under tmp/work/.",
            resume_when="The controller can parse the exact transient delivery path.",
        )
        emit_diagnostics((diagnostic.diagnostic,), diagnostic_format=arguments.diagnostic_format)
        return 1
    print(json.dumps(asdict(result), sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
