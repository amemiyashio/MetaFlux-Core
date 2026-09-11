#!/usr/bin/env python3
"""Validate the current MetaFlux workflow and its next bounded operation."""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path
from typing import Any

import workflow_state as ws
import rule_loading as rules

ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / "tools"))
from agent_diagnostics import emit_diagnostics, task_stop_error  # noqa: E402

NEXT = {"preparation": "confirm scope and supplied execution context",
        "implementation": "implement the bounded scope; request parent review",
        "review": "parent reviews exact content and check plan",
        "evaluation": "execute the reviewed check plan",
        "delivery": "stage the reviewed paths and create one guarded commit",
        "publication": "publish the exact commit through the governed transport",
        "handoff": "supply or reuse the exact next execution context",
        "complete": "report the exact delivery"}


def inspect(root: Path) -> dict[str, Any]:
    path = ws.local_path(root, "state.json")
    state = ws.read_json(path) if path.exists() else None
    goal_path = root / "agent/goal.json"
    goal = ws.read_json(goal_path) if goal_path.exists() else {}
    stage = state["stage"] if state else "preparation"
    target: Any = state["request"]["kind"] if state else None
    next_operation = NEXT[stage] if state else "read the request and dispatch $main skill, $epoch skill, $batch skill, or $iteration skill"
    if state and stage == "handoff" and state.get("context_refresh_required"):
        target = {"operation": state["request"]["kind"], "required_base_revision": state["remote_main_revision"]}
        next_operation = "application supplies a context at remote main; reread its Goal before selecting work"
    elif state and stage == "handoff" and state["request"]["kind"] == "batch":
        statuses = {lane["id"]: lane["status"] for lane in goal.get("lanes", [])}
        lane = next((x for x in goal.get("lanes", []) if x["status"] == "planned" and
                     all(statuses.get(d) == "integrated" for d in x["depends_on"])), None)
        if lane:
            target = {"epoch": goal["epoch"], "batch": goal["batch"]["id"], "iteration": lane["iteration"],
                      "lane": lane["id"], "work_item": lane["work_item"], "base_revision": ws.oid(root),
                      "objective": lane["outcome"], "acceptance": lane["acceptance"]}
    return {"stage": stage, "evidence": {"head": ws.oid(root), "epoch": goal.get("epoch"),
                                           "commit": state.get("commit") if state else None},
            "next_operation": next_operation, "delivery_target": target}


def changed_paths(root: Path, base: str) -> set[str]:
    before, after = ws.entries(root, base), ws.entries(root)
    return {p for p in before.keys() | after.keys() if before.get(p) != after.get(p)}


def scope(root: Path, request: dict[str, Any]) -> None:
    ws.require(ws.oid(root) == request["base_revision"], "Execution baseline changed; obtain a fresh exact application context or proposal")
    changed = changed_paths(root, request["base_revision"])
    allowed = request["allowed_paths"]
    outside = [p for p in changed if not any(p == a or (a.endswith("/") and p.startswith(a)) for a in allowed)]
    ws.require(not outside, "Changes outside the approved scope: " + ", ".join(sorted(outside)))
    if request["kind"] in {"maintenance", "iteration"}:
        ws.require("agent/goal.json" not in changed, "Only batch or epoch may change Goal")


def begin(root: Path, request: dict[str, Any]) -> dict[str, Any]:
    if request.get("kind") == "read-only":
        return inspect(root)
    ws.require(request.get("schema_version") == 1, "Request schema must be 1")
    ws.require(request.get("kind") in {"maintenance", "iteration", "batch", "epoch"}, "Unknown workflow kind")
    ws.require(isinstance(request.get("objective"), str) and bool(request["objective"].strip()), "A bounded objective is required")
    ws.require(isinstance(request.get("allowed_paths"), list) and bool(request["allowed_paths"]) and
               all(isinstance(p, str) and p and not p.startswith(("/", "agent/tmp")) and ".." not in p.split("/")
                   for p in request["allowed_paths"]), "Explicit repository-relative scope paths are required")
    ws.validate_plan(request.get("checks"))
    ws.require(isinstance(request.get("skills", []), list), "Additional skills must be a list")
    rules.required(root, request["kind"], request["allowed_paths"], request.get("skills", []))
    ws.exact_commit(root, request.get("base_revision", ""))
    ws.require(ws.oid(root) == request["base_revision"], "Execution baseline changed; obtain the exact current context")
    ws.require(request.get("publication", "auto") in {"auto", "local"}, "Publication must be auto or local")
    if request["kind"] == "epoch":
        ws.require(isinstance(request.get("confirmation"), str) and bool(request["confirmation"].strip()),
                   "Epoch requires the explicit confirmed proposal from the conversation")
    if request["kind"] in {"iteration", "batch"}:
        goal = ws.read_json(root / "agent/goal.json")
        assignment = request.get("assignment", {})
        lanes = [x for x in goal["lanes"] if x["id"] == assignment.get("lane")]
        ws.require(len(lanes) == 1, "An exact application-supplied lane assignment is required")
        lane = lanes[0]
        ws.require(assignment == {"epoch": goal["epoch"], "batch": goal["batch"]["id"],
                                 "lane": lane["id"], "iteration": lane["iteration"]}, "Assignment is stale")
        ws.require(lane["status"] == "planned" and lane["work_item"] == goal["target"]["work_item"], "Assignment is not the current target")
    path = ws.local_path(root, "state.json")
    if path.exists():
        ws.require(ws.read_json(path)["stage"] == "complete", "An active operation exists; use inspect or resume")
    scope(root, request)
    state = {"schema_version": 1, "request": request, "run": ws.digest(request),
             "stage": "preparation", "input": ws.snapshot(root)}
    ws.atomic_json(path, state)
    return inspect(root)


def load_rules(root: Path, skills: list[str] | None = None) -> dict[str, Any]:
    path = ws.local_path(root, "state.json")
    ws.require(path.exists(), "Begin the bounded request before loading operation rules")
    state = ws.read_json(path)
    validate_state(root, state)
    request = state["request"]
    committed = bool(state.get("commit"))
    rules.load(root, request["kind"], ws.oid(root) if committed else request["base_revision"],
               skills=[*request.get("skills", []), *(skills or [])],
               paths=[] if committed else request["allowed_paths"])
    return inspect(root)


def require_rules(root: Path, state: dict[str, Any]) -> None:
    request = state["request"]
    committed = bool(state.get("commit"))
    loaded = rules.current(root, kind=request["kind"])
    expected = rules.required(root, request["kind"], [] if committed else request["allowed_paths"], request.get("skills", []))
    ws.require(set(expected) <= set(loaded["skills"]), "Required scope rules were not loaded; use $main skill (main.py load-rules)")
    ws.require(loaded["request"] == (None if committed else state["run"]), "Rules belong to another operation; use $main skill (main.py load-rules)")
    ws.require(loaded["base_revision"] == (ws.oid(root) if committed else request["base_revision"]),
               "Rules have a different operation baseline; use $main skill (main.py load-rules)")


def commit_record(root: Path, revision: str, run: str | None = None) -> dict[str, Any]:
    ws.exact_commit(root, revision)
    ws.require(ws.ancestor(root, revision, ws.oid(root)), "Recovered commit is outside current history")
    message = ws.git(root, "show", "-s", "--format=%B", revision).decode()
    lines = [x.removeprefix("MetaFlux-Workflow: ") for x in message.splitlines() if x.startswith("MetaFlux-Workflow: ")]
    ws.require(len(lines) == 1, "Commit has no unique workflow record")
    record = json.loads(lines[0])
    ws.require(record.get("schema_version") == 1 and record.get("kind") in {"maintenance", "iteration", "batch", "epoch"}, "Invalid workflow commit record")
    ws.require(run is None or record.get("run") == run, "Commit belongs to another operation")
    ws.require(record.get("tree") == ws.oid(root, revision + "^{tree}"), "Commit record tree mismatch")
    parents = ws.git(root, "show", "-s", "--format=%P", revision).decode().split()
    ws.require(parents and parents[0] == record.get("head"), "Commit record baseline mismatch")
    ws.require(isinstance(record.get("receipt"), str) and bool(re.fullmatch(r"[0-9a-f]{64}", record["receipt"])), "Commit lacks verification identity")
    return record


def recover(root: Path, revision: str | None = None) -> dict[str, Any]:
    path = ws.local_path(root, "state.json")
    if not path.exists():
        if revision is None:
            result = inspect(root)
            result["next_operation"] = "re-read the request, review and evaluate before commit; supply an exact revision for post-commit recovery"
            return result
        record = commit_record(root, revision)
        ws.atomic_json(path, {"schema_version": 1, "request": {"kind": record["kind"],
                       "publication": record["publication"]}, "run": record["run"],
                       "commit": revision, "stage": "handoff" if record["publication"] == "local" or record["kind"] == "iteration" else "publication"})
        return inspect(root)
    state = ws.read_json(path)
    if state.get("commit"):
        validate_state(root, state)
        clear_committed_transaction(root, state["commit"])
        return inspect(root)
    candidates = ws.git(root, "log", "--format=%H", state["request"]["base_revision"] + "..HEAD").decode().splitlines()
    for candidate in candidates:
        try:
            commit_record(root, candidate, state["run"])
        except (ws.WorkflowError, ValueError, KeyError):
            continue
        state["commit"] = candidate
        state["stage"] = "handoff" if state["request"].get("publication") == "local" or state["request"]["kind"] == "iteration" else "publication"
        ws.atomic_json(path, state)
        clear_committed_transaction(root, candidate)
        return inspect(root)
    ws.require(ws.oid(root) == state["request"]["base_revision"], "Unrelated HEAD change invalidated the execution baseline; obtain a fresh context")
    ws.recover_transaction(root)
    rules_path = ws.local_path(root, "rules.json")
    if rules_path.exists():
        rules_path.unlink()
    if state.get("input") != ws.snapshot(root) or state["stage"] in {"evaluation", "delivery"}:
        state["stage"] = "review"
        state.pop("receipt", None)
        state.pop("review", None)
        state["input"] = ws.snapshot(root)
        ws.atomic_json(path, state)
    return inspect(root)


def validate_state(root: Path, state: dict[str, Any]) -> None:
    if state.get("commit"):
        record = commit_record(root, state["commit"], state["run"])
        ws.require(record["kind"] == state["request"]["kind"] and
                   record["publication"] == state["request"].get("publication", "auto"), "Cached request disagrees with its committed delivery")
    else:
        ws.require(ws.digest(state["request"]) == state["run"], "Current request changed; re-read the user scope")
        scope(root, state["request"])


def clear_committed_transaction(root: Path, revision: str) -> None:
    path = ws.local_path(root, "acceptance.json")
    if not path.exists():
        return
    txn = ws.read_json(path)
    ws.validate_transaction(root, txn)
    message = ws.git(root, "show", "-s", "--format=%B", revision).decode()
    lines = [x.removeprefix("MetaFlux-Acceptance: ") for x in message.splitlines() if x.startswith("MetaFlux-Acceptance: ")]
    ws.require(len(lines) == 1 and json.loads(lines[0]) == txn["identity"], "Committed acceptance differs from pending transaction")
    committed = ws.entries(root, revision)
    for name, values in txn["files"].items():
        after = values["after"]
        ws.require(committed.get(name) == (after["mode"], after["oid"]), "Committed acceptance blob mismatch")
    path.unlink()


def step(root: Path, event: str, payload: dict[str, Any]) -> dict[str, Any]:
    path = ws.local_path(root, "state.json")
    ws.require(path.exists(), "No active operation; read the request and begin")
    state = ws.read_json(path)
    validate_state(root, state)
    stage, request = state["stage"], state["request"]
    if event not in {"repair", "handoff"}:
        require_rules(root, state)
    if event == "prepared":
        ws.require(stage == "preparation", "prepared requires preparation")
        scope(root, request)
        ws.require(state["input"] == ws.snapshot(root),
                   "Files changed after begin but before preparation; preserve changes and re-review with $main skill (main.py resume)")
        state["stage"] = "implementation"
    elif event == "review":
        ws.require(stage in {"implementation", "review", "evaluation", "delivery"}, "Review is only a pre-commit operation")
        scope(root, request)
        state["review"] = ws.review(root, payload.get("summary", ""), request["checks"])
        state["input"] = ws.snapshot(root)
        state["stage"] = "evaluation"
        state.pop("receipt", None)
    elif event == "evaluate":
        ws.require(stage == "evaluation", "evaluate requires a current parent review")
        scope(root, request)
        try:
            state["receipt"] = ws.evaluate(root, request["kind"], request["base_revision"], request["checks"], state["review"])
        except BaseException:
            state["stage"] = "implementation"
            state.pop("receipt", None)
            ws.atomic_json(path, state)
            raise
        state["stage"] = "delivery"
    elif event == "deliver":
        ws.require(stage == "delivery", "deliver requires executed verification")
        scope(root, request)
        tree = ws.oid(root, ":")
        ws.commit_guard(root, state["receipt"]["input"]["head"], tree, state["receipt"], kind=request["kind"])
        receipt_path = ws.local_path(root, "receipts/" + state["receipt"]["digest"] + ".json")
        record = {"schema_version": 1, "run": state["run"], "kind": request["kind"],
                  "publication": request.get("publication", "auto"), "head": ws.oid(root),
                  "tree": tree, "receipt": state["receipt"]["digest"]}
        message = payload.get("message", "")
        ws.require(isinstance(message, str) and bool(message.strip()) and "MetaFlux-" not in message, "Supply a plain commit message")
        message += "\n\nMetaFlux-Workflow: " + json.dumps(record, sort_keys=True, separators=(",", ":"))
        txn_path = ws.local_path(root, "acceptance.json")
        if request["kind"] == "batch":
            ws.require(txn_path.exists(), "Batch delivery needs its exact acceptance transaction")
            txn = ws.read_json(txn_path)
            message += "\nMetaFlux-Acceptance: " + json.dumps(txn["identity"], sort_keys=True, separators=(",", ":"))
        message_path = ws.local_path(root, "commit-message.txt")
        message_path.write_text(message + "\n", encoding="utf-8")
        helper = Path(__file__).with_name("commit_as_agent_tool.py")
        environment = ws.environment(root)
        descriptors = ()
        if ws.ACTIVE_LOCK_FD is not None:
            environment["METAFLUX_MAIN_LOCK_FD"] = str(ws.ACTIVE_LOCK_FD)
            descriptors = (ws.ACTIVE_LOCK_FD,)
        result = subprocess.run([sys.executable, "-B", str(helper), "--agent-tool", payload.get("agent_tool", ""),
                                 "--expected-head", record["head"], "--expected-tree", tree,
                                 "--receipt", str(receipt_path), "--kind", request["kind"], "--", "-F", str(message_path)],
                                cwd=root, env=environment, pass_fds=descriptors)
        ws.require(result.returncode == 0, "Guarded commit failed; preserve reviewed state and child diagnostics")
        state["commit"] = ws.oid(root)
        commit_record(root, state["commit"], state["run"])
        if txn_path.exists() and request["kind"] == "batch":
            txn_path.unlink()
        state["stage"] = "handoff" if request.get("publication") == "local" or request["kind"] == "iteration" else "publication"
    elif event == "publish":
        ws.require(stage == "publication", "publish requires an exact committed delivery")
        commit_record(root, state["commit"], state["run"])
        helper = Path(__file__).with_name("push_repository.py")
        result = subprocess.run([sys.executable, "-B", str(helper), "--root", str(root), "push", "--revision", state["commit"]],
                                cwd=root, capture_output=True, text=True)
        print(result.stdout, end="")
        print(result.stderr, end="", file=sys.stderr)
        ws.require(result.returncode == 0, "Publication incomplete; retain the exact commit and resume publication after transport evidence changes")
        reports = [json.loads(line) for line in result.stdout.splitlines() if line.startswith("{")]
        reports = [x for x in reports if x.get("published_revision") == state["commit"]]
        ws.require(len(reports) == 1, "Publication helper did not report the exact completed revision")
        state["remote_main_revision"] = reports[0]["remote_main_revision"]
        state["context_refresh_required"] = reports[0]["context_refresh_required"]
        state["stage"] = "handoff"
    elif event == "handoff":
        ws.require(stage == "handoff", "handoff requires delivery and its publication boundary")
        state["stage"] = "complete"
        state["handoff"] = payload.get("assignment_request", "Report the completed delivery; application supplies the next exact context")
    elif event == "repair":
        ws.require(stage in {"implementation", "review", "evaluation", "delivery"}, "Committed operations recover publication instead of reimplementation")
        ws.require(set(payload) <= {"checks"}, "Repair may revise checks only; it preserves scope, baseline, and publication")
        if "checks" in payload:
            checks = payload["checks"]
            ws.validate_plan(checks)
            replacements = {check["id"]: check for check in checks}
            for check in request["checks"]:
                ws.require(check["id"] in replacements, "Repair must retain every declared check")
                ws.require(replacements[check["id"]].get("optional_skip_reason") == check.get("optional_skip_reason"),
                           "Repair must preserve each check's required/optional boundary")
            state["request"] = {**request, "checks": checks}
            state["run"] = ws.digest(state["request"])
        state["stage"] = "implementation"
        state.pop("receipt", None)
        state.pop("review", None)
    else:
        raise ws.WorkflowError("Unknown workflow event: " + event)
    ws.atomic_json(path, state)
    return inspect(root)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path("."))
    sub = parser.add_subparsers(dest="action", required=True)
    sub.add_parser("inspect")
    start = sub.add_parser("begin")
    start.add_argument("request", type=Path, nargs="?")
    start.add_argument("--request-json")
    loading = sub.add_parser("load-rules")
    loading.add_argument("--skill", action="append", default=[])
    event = sub.add_parser("step")
    event.add_argument("event", choices=("prepared", "review", "evaluate", "deliver", "publish", "handoff", "repair"))
    payload = event.add_mutually_exclusive_group()
    payload.add_argument("--payload", type=Path)
    payload.add_argument("--payload-json")
    resume = sub.add_parser("resume")
    resume.add_argument("--revision")
    args = parser.parse_args()
    root = args.root.resolve()
    try:
        if args.action == "inspect":
            result = inspect(root)
        else:
            with ws.lock(root):
                if args.action == "begin":
                    ws.require(bool(args.request) != bool(args.request_json), "Supply one request file or --request-json")
                    result = begin(root, ws.read_json(args.request) if args.request else json.loads(args.request_json))
                elif args.action == "load-rules":
                    result = load_rules(root, args.skill)
                elif args.action == "resume":
                    result = recover(root, args.revision)
                else:
                    result = step(root, args.event, ws.read_json(args.payload) if args.payload else json.loads(args.payload_json) if args.payload_json else {})
    except (ws.WorkflowError, OSError, ValueError, KeyError, TypeError) as error:
        emit_diagnostics((task_stop_error(code="workflow.transition-invalid", source="main",
            summary="The next workflow transition needs corrected evidence.", evidence=(str(error),),
            responsibility="current-agent", disposition="preserve-and-report",
            required_action="Inspect current Git state and use $main skill with resume within the original task scope.",
            resume_when="The exact context and required review, verification, or publication evidence match.").diagnostic,))
        return 1
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
