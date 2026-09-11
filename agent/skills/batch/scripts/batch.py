#!/usr/bin/env python3
"""Check a committed delivery or prepare one recoverable Batch acceptance."""

from __future__ import annotations

import argparse
import copy
import datetime as dt
import json
import re
import sys
from pathlib import Path
from typing import Any, Callable

ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / "agent/skills/main/scripts"))
sys.path.insert(0, str(ROOT / "tools"))
import workflow_state as ws  # noqa: E402
from agent_diagnostics import emit_diagnostics, task_stop_error  # noqa: E402

FIELDS = {"schema_version", "epoch", "batch", "iteration", "lane", "base_revision", "tip_revision",
          "tests", "blockers", "knowledge_candidates", "acceptance_kind", "slice_objective",
          "verification_receipt", "exit_gate"}


def validate_manifest(document: Any) -> dict[str, Any]:
    ws.require(isinstance(document, dict) and set(document) == FIELDS and document.get("schema_version") == 2,
               "Delivery requires exactly the schema v2 fields")
    for field, pattern in {"epoch": r"epoch-\d{4}", "batch": r"batch-\d{4}",
                           "iteration": r"iteration-\d{4}", "lane": r"lane-[a-z0-9]+(?:-[a-z0-9]+)*",
                           "base_revision": r"[0-9a-f]{40}", "tip_revision": r"[0-9a-f]{40}"}.items():
        ws.require(isinstance(document[field], str) and bool(re.fullmatch(pattern, document[field])), "Invalid delivery " + field)
    ws.require(document["acceptance_kind"] in {"slice", "work-item"}, "Invalid acceptance kind")
    ws.require(isinstance(document["slice_objective"], str) and bool(document["slice_objective"].strip()), "A slice objective is required")
    ws.require(document["blockers"] == [], "Resolve the declared delivery blockers")
    ws.require(isinstance(document["knowledge_candidates"], list) and all(isinstance(x, str) for x in document["knowledge_candidates"]), "Invalid knowledge candidates")
    receipt = document["verification_receipt"]
    ws.require(isinstance(receipt, dict) and bool(receipt.get("results")), "Missing actual candidate verification receipt")
    expected = [{"command": result["argv"], "status": "passed" if result["returncode"] == 0 else "optional-skip"}
                for result in receipt["results"]]
    ws.require(document["tests"] == expected, "Reported tests differ from executed results")
    ws.require(document["exit_gate"] is None or isinstance(document["exit_gate"], dict), "Invalid Exit Gate claim")
    return document


def validate_goal(goal: dict[str, Any]) -> None:
    ws.require(goal.get("schema_version") == 4 and isinstance(goal.get("lanes"), list) and bool(goal["lanes"]), "Expected Goal schema v4")
    lanes = goal["lanes"]
    for field in ("id", "iteration", "work_item"):
        ws.require(len({x[field] for x in lanes}) == len(lanes), "Duplicate lane " + field)
    by_id = {x["id"]: x for x in lanes}
    visited, visiting = set(), set()
    def visit(name: str) -> None:
        ws.require(name in by_id and name not in visiting, "Invalid or cyclic lane dependency")
        if name in visited:
            return
        visiting.add(name)
        lane = by_id[name]
        ws.require(lane["status"] in {"planned", "integrated", "deferred"}, "Invalid lane status")
        ws.require(bool(re.fullmatch(r"iteration-\d{4}", lane["iteration"])), "Invalid Iteration")
        ws.require(len(set(lane["depends_on"])) == len(lane["depends_on"]), "Repeated lane dependency")
        for dependency in lane["depends_on"]:
            visit(dependency)
            ws.require(lane["status"] != "integrated" or by_id[dependency]["status"] == "integrated", "Integrated lane has unaccepted dependencies")
        visiting.remove(name)
        visited.add(name)
    for name in by_id:
        visit(name)
    planned = [x for x in lanes if x["status"] == "planned"]
    ws.require((goal["batch"]["status"] == "open") == bool(planned), "Batch status disagrees with remaining work")
    if planned:
        ready = next_ready(goal)
        ws.require(ready is not None and ready["work_item"] == goal["target"]["work_item"], "Target differs from the first dependency-ready lane")


def next_ready(goal: dict[str, Any]) -> dict[str, Any] | None:
    statuses = {x["id"]: x["status"] for x in goal["lanes"]}
    return next((x for x in goal["lanes"] if x["status"] == "planned" and
                 all(statuses.get(d) == "integrated" for d in x["depends_on"])), None)


def lane_for(goal: dict[str, Any], name: str) -> dict[str, Any]:
    matches = [x for x in goal["lanes"] if x["id"] == name]
    ws.require(len(matches) == 1, "Delivery lane does not resolve uniquely")
    return matches[0]


def transition(goal: dict[str, Any], delivery: dict[str, Any]) -> dict[str, Any]:
    validate_goal(goal)
    lane = lane_for(goal, delivery["lane"])
    ws.require(delivery["epoch"] == goal["epoch"] and delivery["batch"] == goal["batch"]["id"] and
               delivery["iteration"] == lane["iteration"], "Delivery identity is stale")
    ws.require(lane == next_ready(goal), "Delivery is not the current dependency-ready lane")
    result = copy.deepcopy(goal)
    current = lane_for(result, delivery["lane"])
    if delivery["acceptance_kind"] == "slice":
        maximum = max(int(x["iteration"].split("-")[1]) for x in result["lanes"])
        ws.require(maximum < 9999, "Iteration numbering exhausted; request an explicit bounded Batch plan")
        current["iteration"] = f"iteration-{maximum + 1:04d}"
    else:
        current["status"] = "integrated"
        following = next_ready(result)
        if following:
            result["target"]["work_item"] = following["work_item"]
        else:
            ws.require(not any(x["status"] == "planned" for x in result["lanes"]), "No dependency-ready next lane")
            result["batch"]["status"] = "integrated"
    validate_goal(result)
    return result


def find_work_item(root: Path, identifier: str) -> Path:
    matches = [p for p in (root / "agent/plan").glob("milestone-*/work/work-item-*.md")
               if re.search(rf"^id: {re.escape(identifier)}$", p.read_text(), re.MULTILINE)]
    ws.require(len(matches) == 1, "Work item does not resolve uniquely: " + identifier)
    return matches[0]


def exit_gate(text: str) -> str:
    match = re.search(r"^## Exit Gate\s*\n(.*?)(?=^## |\Z)", text, re.MULTILINE | re.DOTALL)
    ws.require(match is not None and bool(match[1].strip()), "Work item has no Exit Gate")
    return match[1].strip()


def require_exit_gate(root: Path, delivery: dict[str, Any], lane: dict[str, Any], receipt: dict[str, Any]) -> None:
    if delivery["acceptance_kind"] == "slice":
        ws.require(delivery["exit_gate"] is None, "A slice must not claim complete Exit Gate evidence")
        return
    path = find_work_item(root, lane["work_item"])
    claim = delivery["exit_gate"]
    ws.require(isinstance(claim, dict) and set(claim) == {"digest", "checks"}, "Whole work-item acceptance needs an Exit Gate mapping")
    ws.require(claim["digest"] == ws.digest(exit_gate(path.read_text())), "Exit Gate changed since review")
    passed = {r["id"] for r in receipt["results"] if r["returncode"] == 0 and not r.get("ctest", {}).get("skipped")}
    ws.require(isinstance(claim["checks"], list) and bool(claim["checks"]) and set(claim["checks"]) <= passed,
               "Whole Exit Gate checks must actually pass in this verification phase")


def accepted_revision(root: Path, delivery: dict[str, Any]) -> str | None:
    identity = ws.digest(delivery)
    revisions = ws.git(root, "log", "--format=%H", "--fixed-strings", "--grep=" + identity, "HEAD").decode().splitlines()
    for revision in revisions:
        message = ws.git(root, "show", "-s", "--format=%B", revision).decode()
        lines = [x.removeprefix("MetaFlux-Acceptance: ") for x in message.splitlines() if x.startswith("MetaFlux-Acceptance: ")]
        if len(lines) != 1:
            continue
        try:
            record = json.loads(lines[0])
            ws.require(record["delivery"] == identity and record["candidate"] == delivery["tip_revision"] and
                       record["kind"] == delivery["acceptance_kind"], "Acceptance record identity mismatch")
            parents = ws.git(root, "show", "-s", "--format=%P", revision).decode().split()
            ws.require(parents and parents[0] == record["head"] and ws.ancestor(root, delivery["tip_revision"], revision), "Acceptance revision graph mismatch")
            before = json.loads(ws.git(root, "show", record["head"] + ":agent/goal.json"))
            after = json.loads(ws.git(root, "show", revision + ":agent/goal.json"))
            ws.require(transition(before, delivery) == after, "Acceptance Goal transition mismatch")
            old_entries, new_entries = ws.entries(root, record["head"]), ws.entries(root, revision)
            ws.require("agent/goal.json" in record["files"], "Acceptance has no Goal transaction")
            for path, blobs in record["files"].items():
                expected_after = blobs["after"]
                ws.require(new_entries.get(path) == (expected_after["mode"], expected_after["oid"]), "Acceptance authority after blob mismatch")
                if path == "agent/goal.json":
                    expected_before = blobs["before"]
                    ws.require(old_entries.get(path) == (expected_before["mode"], expected_before["oid"]), "Acceptance Goal before blob mismatch")
                else:
                    ws.require(isinstance(blobs["before"], dict), "Acceptance non-Goal before blob is missing")
            integrated = reconstruct_input(new_entries, record["files"])
            ws.require(ws.digest(integrated) == record["integration_input"]["content"] and
                       ws.toolchain_entries(integrated) == record["integration_input"]["toolchain"] and
                       record["integration_input"]["head"] == record["head"], "Acceptance combined input identity mismatch")
            lane = lane_for(before, delivery["lane"])
            if delivery["acceptance_kind"] == "work-item":
                work = next((p for p in record["files"] if p.startswith("agent/plan/") and
                             re.search(rf"^id: {re.escape(lane['work_item'])}$", ws.git(root, "show", revision + ":" + p).decode(), re.MULTILINE)), None)
                ws.require(work is not None and re.search(r"^status: Complete$", ws.git(root, "show", revision + ":" + work).decode(), re.MULTILINE), "Acceptance did not complete its work item")
            from main import commit_record
            workflow = commit_record(root, revision)
            ws.require(workflow["kind"] == "batch" and bool(record["integration_receipt"]), "Acceptance lacks guarded integration evidence")
        except (ws.WorkflowError, KeyError, ValueError, TypeError):
            continue
        return revision
    return None


def reconstruct_input(values: dict[str, tuple[str, str]], files: dict[str, Any]) -> dict[str, tuple[str, str]]:
    result = dict(values)
    for path, blobs in files.items():
        after, before = blobs["after"], blobs["before"]
        ws.require(result.get(path) == ((after["mode"], after["oid"]) if after else None), "Acceptance after blob changed: " + path)
        if before is None:
            result.pop(path, None)
        else:
            result[path] = (before["mode"], before["oid"])
    return result


def validate_pending(root: Path) -> None:
    path = ws.local_path(root, "acceptance.json")
    ws.require(path.is_file(), "Missing acceptance transaction; re-review and verify before commit")
    txn = ws.read_json(path)
    ws.validate_transaction(root, txn)
    delivery = validate_manifest(txn["delivery"])
    identity = txn["identity"]
    ws.require(txn["head"] == ws.oid(root) and identity["head"] == txn["head"] and identity["files"] == txn["files"],
               "Acceptance transaction identity changed")
    ws.require(identity["delivery"] == ws.digest(delivery) and identity["candidate"] == delivery["tip_revision"] and
               identity["kind"] == delivery["acceptance_kind"] and identity["integration_receipt"] == txn["integration_receipt"]["digest"] and
               identity["integration_input"] == txn["integration_receipt"]["input"], "Acceptance proof changed")
    merge_path = Path(ws.git(root, "rev-parse", "--path-format=absolute", "--git-path", "MERGE_HEAD").decode().strip())
    merge_parents = merge_path.read_text().splitlines() if merge_path.exists() else []
    expected_parents = [] if delivery["tip_revision"] == txn["head"] else [delivery["tip_revision"]]
    ws.require(merge_parents == expected_parents, "Acceptance candidate merge identity changed; restore the exact prepared merge and reverify")
    integrated = reconstruct_input(ws.entries(root), txn["files"])
    ws.validate_receipt(root, txn["integration_receipt"], kind="integration", base=delivery["base_revision"], tested_entries=integrated)
    ws.validate_receipt(root, delivery["verification_receipt"], kind="iteration", base=delivery["base_revision"], revision=delivery["tip_revision"])
    before_blob = txn["files"]["agent/goal.json"]["before"]
    before_bytes = ws.git(root, "cat-file", "blob", before_blob["oid"])
    ws.require(before_bytes == ws.git(root, "show", txn["head"] + ":agent/goal.json"), "Integration changed Goal before acceptance")
    before = json.loads(before_bytes)
    ws.require(transition(before, delivery) == ws.read_json(root / "agent/goal.json"), "Goal does not match the approved acceptance transition")
    require_exit_gate(root, delivery, lane_for(before, delivery["lane"]), txn["integration_receipt"])
    expected_files = {"agent/goal.json"}
    if delivery["acceptance_kind"] == "work-item":
        expected_statuses = {lane_for(before, delivery["lane"])["work_item"]: "Complete"}
        following = next_ready(transition(before, delivery))
        if following:
            expected_statuses[following["work_item"]] = "Active"
        for work_item, status in expected_statuses.items():
            work = find_work_item(root, work_item)
            name = work.relative_to(root).as_posix()
            expected_files.add(name)
            ws.require(name in txn["files"], "Acceptance work item is missing from transaction")
            old_text = ws.git(root, "cat-file", "blob", txn["files"][name]["before"]["oid"]).decode()
            expected = re.sub(r"^status: .+$", "status: " + status, old_text, count=1, flags=re.MULTILINE)
            expected = re.sub(r"^updated: .+$", "updated: " + txn["date"], expected, count=1, flags=re.MULTILINE)
            ws.require(work.read_text() == expected, "Work-item change exceeds its acceptance transition")
    ws.require(set(txn["files"]) == expected_files, "Acceptance transaction has unrelated authority paths")


def check_delivery(document: Any, root: Path, *, prepared: bool = False) -> dict[str, Any]:
    delivery = validate_manifest(document)
    for field in ("base_revision", "tip_revision"):
        ws.exact_commit(root, delivery[field])
    base, tip = delivery["base_revision"], delivery["tip_revision"]
    ws.require(base != tip and ws.ancestor(root, base, tip) and ws.content(root, base) != ws.content(root, tip), "Empty or invalid candidate range")
    accepted = accepted_revision(root, delivery)
    if accepted:
        return {"action": "no-op", "accepted_revision": accepted, "next": next_ready(ws.read_json(root / "agent/goal.json"))}
    goal = ws.read_json(root / "agent/goal.json")
    following = transition(goal, delivery)
    ws.validate_receipt(root, delivery["verification_receipt"], kind="iteration", revision=tip, base=base)
    require_exit_gate(root, delivery, lane_for(goal, delivery["lane"]), delivery["verification_receipt"])
    head = ws.oid(root)
    ws.require(ws.ancestor(root, base, head), "Candidate base is outside current main history")
    base_goal = json.loads(ws.git(root, "show", base + ":agent/goal.json"))
    ws.require(base_goal["epoch"] == goal["epoch"], "Candidate predates active Epoch")
    ws.require(ws.git(root, "show", base + ":agent/goal.json") == ws.git(root, "show", tip + ":agent/goal.json"), "Worker changed Goal")
    merge = ws.git(root, "rev-parse", "--verify", "MERGE_HEAD", check=False).decode().strip()
    if tip == head:
        action = "in-place"
    elif prepared and merge == tip:
        action = "prepared-merge"
    else:
        ws.require(not ws.ancestor(root, tip, head), "Stale non-HEAD candidate; supply a rebased and reverified delivery")
        action = "merge"
    if not prepared:
        ws.require(not ws.git(root, "status", "--porcelain", "--untracked-files=all").strip(), "Delivery check requires a clean supplied integration context")
    return {"action": action, "candidate": tip, "next": next_ready(following)}


def default_state_validator(root: Path) -> None:
    import subprocess
    result = subprocess.run([sys.executable, "-B", "tools/check-agent-state.py", "."], cwd=root)
    ws.require(result.returncode == 0, "Advanced Goal failed the Agent-state gate")


def advance_delivery(document: Any, root: Path, *, integration_receipt: dict[str, Any] | None = None,
                     state_validator: Callable[[Path], None] = default_state_validator) -> dict[str, Any]:
    with ws.lock(root):
        checked = check_delivery(document, root, prepared=True)
        if checked["action"] == "no-op":
            return checked
        ws.require(checked["action"] in {"in-place", "prepared-merge"}, "Prepare the exact candidate merge first")
        ws.validate_receipt(root, integration_receipt, kind="integration", base=document["base_revision"])
        goal = ws.read_json(root / "agent/goal.json")
        lane = lane_for(goal, document["lane"])
        require_exit_gate(root, document, lane, integration_receipt)
        new_goal = transition(goal, document)
        current = find_work_item(root, lane["work_item"])
        ws.require(re.search(r"^status: Active$", current.read_text(), re.MULTILINE) is not None, "Current work item is not Active")
        replacements = {"agent/goal.json": json.dumps(new_goal, indent=2) + "\n"}
        if document["acceptance_kind"] == "work-item":
            replacements[current.relative_to(root).as_posix()] = re.sub(r"^status: Active$", "status: Complete", current.read_text(), flags=re.MULTILINE)
            following = next_ready(new_goal)
            if following:
                next_path = find_work_item(root, following["work_item"])
                ws.require(re.search(r"^status: (Queued|Active)$", next_path.read_text(), re.MULTILINE) is not None, "Following work item has incompatible status")
                replacements[next_path.relative_to(root).as_posix()] = re.sub(r"^status: (Queued|Active)$", "status: Active", next_path.read_text(), flags=re.MULTILINE)
            for path in list(replacements):
                if path != "agent/goal.json":
                    replacements[path] = re.sub(r"^updated: .+$", "updated: " + dt.date.today().isoformat(), replacements[path], flags=re.MULTILINE)
        identity = {"schema_version": 1, "delivery": ws.digest(document), "candidate": document["tip_revision"],
                    "kind": document["acceptance_kind"], "epoch": document["epoch"], "batch": document["batch"],
                    "iteration": document["iteration"], "lane": document["lane"],
                    "integration_receipt": integration_receipt["digest"], "integration_input": integration_receipt["input"], "head": ws.oid(root)}
        txn = ws.begin_transaction(root, replacements, identity)
        identity["files"] = txn["files"]
        txn["delivery"] = document
        txn["integration_receipt"] = integration_receipt
        txn["date"] = dt.date.today().isoformat()
        ws.atomic_json(ws.local_path(root, "acceptance.json"), txn)
        try:
            ws.apply_transaction(root, txn)
            state_validator(root)
        except BaseException:
            ws.recover_transaction(root)
            raise
        return {"action": "advanced", "kind": document["acceptance_kind"], "next": next_ready(new_goal),
                "changed_paths": sorted(replacements), "transaction": str(ws.local_path(root, "acceptance.json"))}


def integration_context(root: Path, document: Any) -> tuple[dict, dict]:
    import main as controller
    checked = check_delivery(document, root, prepared=True)
    if checked["action"] == "no-op":
        return checked, {}
    ws.require(checked["action"] in {"in-place", "prepared-merge"}, "Prepare the exact candidate before integration verification")
    ws.require(not ws.local_path(root, "acceptance.json").exists(), "Recover pending acceptance before another integration")
    state = ws.read_json(ws.local_path(root, "state.json"))
    controller.validate_state(root, state)
    ws.require(state["request"]["kind"] == "batch" and state["stage"] == "implementation" and not state.get("commit"),
               "Integration verification requires the prepared Batch implementation stage")
    controller.scope(root, state["request"])
    expected = {key: document[key] for key in ("epoch", "batch", "iteration", "lane")}
    ws.require(state["request"]["assignment"] == expected, "Batch context differs from the candidate assignment")
    return checked, state


def load_integration_rules(root: Path, document: Any) -> dict:
    import main as controller
    import rule_loading
    checked, state = integration_context(root, document)
    if checked["action"] == "no-op":
        return checked
    # The operation base is current HEAD; the receipt base is the delivery base.
    base = document["base_revision"]
    rule_loading.load(root, "integration", base, paths=controller.changed_paths(root, base),
                      skills=state["request"].get("skills", []))
    return {"action": "integration-rules-loaded", "base_revision": base, "head": ws.oid(root)}


def verify_delivery(root: Path, document: Any, checks: list[dict], summary: str) -> dict:
    import rule_loading
    checked, state = integration_context(root, document)
    if checked["action"] == "no-op":
        return checked
    base = document["base_revision"]
    rules = rule_loading.current(root, kind="integration")
    ws.require(rules["base_revision"] == base and rules["head"] == ws.oid(root) and rules["request"] == state["run"],
               "Integration rules use another baseline; run $batch skill load-rules DELIVERY before verification")
    ws.validate_plan(checks)
    if document["acceptance_kind"] == "work-item":
        ws.require(set(document["exit_gate"]["checks"]) <= {x["id"] for x in checks}, "Integration plan omits Exit Gate checks")
    reviewed = ws.review(root, summary, checks)
    receipt = ws.evaluate(root, "integration", base, checks, reviewed)
    require_exit_gate(root, document, lane_for(ws.read_json(root / "agent/goal.json"), document["lane"]), receipt)
    return {"action": "verified", "base_revision": base, "head": ws.oid(root), "digest": receipt["digest"],
            "receipt": str(ws.local_path(root, "receipts/" + receipt["digest"] + ".json"))}


def check_metadata(root: Path) -> dict:
    validate_pending(root)
    transaction = ws.read_json(ws.local_path(root, "acceptance.json"))
    return {"action": "metadata-verified", "changed_paths": sorted(transaction["files"]),
            "candidate": transaction["identity"]["candidate"],
            "integration_receipt": transaction["integration_receipt"]["digest"]}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("check", "load-rules", "verify", "advance", "check-metadata"))
    parser.add_argument("delivery", type=Path, nargs="?")
    parser.add_argument("--root", type=Path, default=Path("."))
    parser.add_argument("--receipt", type=Path)
    parser.add_argument("--checks", type=Path)
    parser.add_argument("--summary")
    args = parser.parse_args()
    try:
        root = args.root.resolve()
        if args.action == "check-metadata":
            ws.require(args.delivery is None and args.receipt is None and args.checks is None and args.summary is None,
                       "check-metadata uses only the exact pending transaction")
            result = check_metadata(root)
        else:
            ws.require(args.delivery is not None, "Supply the exact delivery")
            document = ws.read_json(args.delivery)
            if args.action == "check":
                result = check_delivery(document, root)
            elif args.action in {"load-rules", "verify"}:
                with ws.lock(root):
                    if args.action == "load-rules":
                        result = load_integration_rules(root, document)
                    else:
                        ws.require(args.checks is not None and bool(args.summary), "verify requires a check plan and parent review summary")
                        result = verify_delivery(root, document, ws.read_json(args.checks), args.summary)
            else:
                result = advance_delivery(document, root, integration_receipt=ws.read_json(args.receipt) if args.receipt else None)
    except (ws.WorkflowError, OSError, ValueError, KeyError) as error:
        emit_diagnostics((task_stop_error(code="acceptance.candidate-invalid", source="batch",
            summary="The delivery has not satisfied the acceptance boundary.", evidence=(str(error),),
            responsibility="batch-integrator", disposition="preserve-and-report",
            required_action="Inspect the exact delivery and use $main resume to repair current evidence.",
            resume_when="Identity, scope, review, verification, and transaction state agree.").diagnostic,))
        return 1
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
