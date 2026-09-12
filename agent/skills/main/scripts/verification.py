#!/usr/bin/env python3
"""Resolve check coverage and execute one isolated, non-cacheable verification."""
from __future__ import annotations

import contextlib
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import time
import uuid
import xml.etree.ElementTree as ET

import workflow_state as ws


SELECTION = {"-R", "--tests-regex", "-E", "--exclude-regex", "-L", "--label-regex",
             "-LE", "--label-exclude", "-I", "--tests-information",
             "--tests-from-file", "--exclude-from-file"}
OUTPUT = {"--output-log", "-O"}


def ctest_context(argv: list[str]) -> list[str] | None:
    if Path(argv[0]).name != "ctest":
        if Path(argv[0]).name in {"nix", "bash", "sh", "env"}:
            ws.require(not any(re.search(r"(^|[\s/])ctest(?=$|\s)", item) for item in argv[1:]),
                       "Declare CTest directly in the check argv; initialize Nix before the controller")
        return None
    context = []
    i = 1
    while i < len(argv):
        item = argv[i]
        attached = re.fullmatch(r"(-LE|-R|-E|-L|-I|-O|-C|-j)(.+)", item)
        if attached:
            item = attached[1] + "=" + attached[2]
        option = item.split("=", 1)[0]
        ws.require(option not in {"--rerun-failed", "--show-only", "-N", "--output-junit",
                                 "--build-and-test", "-S", "--script", "-T", "--test-action",
                                 "-FA", "-FS", "-FC", "--repeat", "--repeat-until-fail"},
                   "Qualification CTest must execute an explicit set with automatic fixtures: " + item)
        if option in SELECTION | OUTPUT:
            i += 1 if "=" in item else 2
            ws.require(i <= len(argv), "Missing CTest selection argument")
            continue
        if item in {"--output-on-failure", "--verbose", "-V", "-VV", "--extra-verbose", "-U", "--union", "--progress"}:
            i += 1
            continue
        aliases = {"-C": "--build-config", "-j": "--parallel"}
        option = aliases.get(option, option)
        valued = {"--test-dir", "--preset", "--build-config", "--parallel", "--timeout",
                  "--stop-time", "--test-load", "--resource-spec-file"}
        if option in valued:
            if "=" in item:
                value = item.split("=", 1)[1]
                i += 1
            else:
                ws.require(i + 1 < len(argv), "CTest plan requires an explicit argument: " + item)
                value = argv[i + 1]
                i += 2
            if option in {"--test-dir", "--resource-spec-file"}:
                value = os.path.normpath(value)
            if option != "--parallel":
                context.append(option + "=" + value)
        else:
            ws.require(item in {"--stop-on-failure", "--schedule-random", "--progress", "--no-tests=error"},
                       "Unsupported qualification CTest argument; use an explicit check harness: " + item)
            context.append(item)
            i += 1
    return sorted(context)


def preset_context(root: Path, name: str) -> list[str]:
    """Resolve repository presets; reject external or opaque execution inputs."""
    ws.require(not (root / "CMakeUserPresets.json").exists(),
               "Qualification presets must be repository-owned; declare explicit CTest argv")
    document = ws.read_json(root / "CMakePresets.json")
    ws.require(not document.get("include"), "Included qualification presets need an explicit CTest plan")

    def resolve(kind: str, key: str, active=()) -> dict:
        ws.require(key not in active, "Cyclic CTest preset inheritance")
        rows = [x for x in document.get(kind, []) if x.get("name") == key]
        ws.require(len(rows) == 1, "Missing or ambiguous CTest preset: " + key)
        own = rows[0]
        parents = own.get("inherits", [])
        parents = [parents] if isinstance(parents, str) else parents
        value = {}
        for parent in reversed(parents):
            inherited = resolve(kind, parent, (*active, key))
            environment = {**value.get("environment", {}), **inherited.get("environment", {})}
            value.update(inherited)
            if environment:
                value["environment"] = environment
        environment = {**value.get("environment", {}), **own.get("environment", {})}
        value.update(own)
        if environment:
            value["environment"] = environment
        return value

    preset = resolve("testPresets", name)
    configured = resolve("configurePresets", preset["configurePreset"])
    directory = configured.get("binaryDir", "")
    directory = directory.replace("${sourceDir}", str(root)).replace("${presetName}", preset["configurePreset"])
    ws.require(directory and "$" not in directory, "Declare an explicit build directory for this qualification preset")
    result = ["--test-dir=" + str((root / directory).resolve())]
    if preset.get("configuration"):
        result.append("--build-config=" + preset["configuration"])
    environment = configured.get("environment", {}) if preset.get("inheritConfigureEnvironment", True) else {}
    environment = {**environment, **preset.get("environment", {})}
    if environment:
        # Preserve potentially expanded/device-specific environments as distinct.
        result.append("preset-environment=" + ws.digest([str(root), name, environment]))
    execution = {key: value for key, value in preset.get("execution", {}).items() if key != "jobs"}
    ws.require("repeat" not in execution, "Qualification preset must not retry tests")
    if execution:
        result.append("preset-execution=" + ws.digest(execution))
    return result


def resolved_context(root: Path, argv: list[str]) -> list[str] | None:
    context = ctest_context(argv)
    if context is None:
        return None
    names = [x.split("=", 1)[1] for x in context if x.startswith("--preset=")]
    ws.require(len(names) <= 1, "Declare one CTest preset")
    result = preset_context(root, names[0]) if names else []
    for item in context:
        if item.startswith("--preset="):
            continue
        if item.startswith(("--test-dir=", "--resource-spec-file=")):
            option, value = item.split("=", 1)
            item = option + "=" + str((root / value).resolve())
        if item.startswith(("--test-dir=", "--build-config=")):
            result = [x for x in result if not x.startswith(item.split("=", 1)[0] + "=")]
        result.append(item)
    if not any(x.startswith("--test-dir=") for x in result):
        result.append("--test-dir=" + str(root.resolve()))
    return sorted(result)


def inventory(root: Path, argv: list[str]) -> dict:
    try:
        process = subprocess.run([*argv, "--show-only=json-v1"], cwd=root,
                                 env=ws.environment(root), capture_output=True, text=True, timeout=30)
    except subprocess.TimeoutExpired as error:
        raise ws.WorkflowError("CTest enumeration timed out before qualification") from error
    ws.require(process.returncode == 0, "CTest enumeration failed:\n" + process.stdout + process.stderr)
    try:
        document = json.loads(process.stdout)
    except ValueError as error:
        raise ws.WorkflowError("CTest did not return its JSON inventory:\n" + process.stdout + process.stderr) from error
    tests = document.get("tests")
    ws.require(isinstance(tests, list) and bool(tests), "CTest selected no tests")
    rows = {}
    for test in tests:
        ws.require(isinstance(test.get("name"), str) and test["name"] not in rows, "Invalid CTest test identity")
        rows[test["name"]] = {"command": test.get("command", []), "properties": test.get("properties", [])}
    setups = {value for row in rows.values() for prop in row["properties"]
              if prop["name"] == "FIXTURES_SETUP" for value in prop["value"]}
    for name, row in rows.items():
        properties = {prop["name"]: prop["value"] for prop in row["properties"]}
        missing = set(properties.get("DEPENDS", [])) - rows.keys()
        ws.require(not missing, f"CTest {name} needs prerequisite tests in the same selection: {sorted(missing)}")
        missing = set(properties.get("FIXTURES_REQUIRED", [])) - setups
        ws.require(not missing, f"CTest {name} needs fixture setup: {sorted(missing)}")
    return rows


def preflight(root: Path, checks: list[dict], *, defer_unconfigured: bool = False) -> dict:
    """Read only; CTest remains the single owner of test definitions and selection."""
    ws.validate_plan(checks)
    commands, covered, resolved = {}, {}, {}
    preparation_seen = False
    for check in checks:
        argv = check["argv"]
        key = tuple(argv)
        ws.require(key not in commands, f"Duplicate check command: {commands.get(key)} and {check['id']}")
        commands[key] = check["id"]
        if Path(argv[0]).name == "cmake" and any(x in argv for x in ("--preset", "--build", "-S")):
            preparation_seen = True
        context = resolved_context(root, argv)
        if context is None:
            continue
        try:
            rows = inventory(root, argv)
        except ws.WorkflowError:
            if not (defer_unconfigured and preparation_seen):
                raise
            resolved[check["id"]] = {"deferred_until_configured": True}
            continue
        ws.require(set(check.get("allowed_ctest_skips", [])) <= rows.keys(),
                   "Allowed CTest skips must name selected tests: " + check["id"])
        for name, row in rows.items():
            identity = ws.digest([context, name, row])
            ws.require(identity not in covered,
                       f"Repeated CTest coverage: {covered.get(identity)} and {check['id']}: {name}; "
                       "choose one covering selection or disjoint selections before evaluation")
            covered[identity] = check["id"]
        resolved[check["id"]] = {"context": context, "selector_context": ctest_context(argv), "tests": rows}
    return resolved


def lock_path(root: Path) -> Path:
    common = ws.git(root, "rev-parse", "--path-format=absolute", "--git-common-dir").decode().strip()
    return Path(common) / "metaflux-verification.lock"


def require_covered_removal(root: Path, removed: dict, replacements: list[dict]) -> None:
    """A repair may remove a redundant CTest selector, never its obligations."""
    context = resolved_context(root, removed["argv"])
    ws.require(context is not None, "Repair must retain every declared non-CTest check")
    expected = inventory(root, removed["argv"])
    covered = {}
    for replacement in replacements:
        if resolved_context(root, replacement["argv"]) != context:
            continue
        ws.require(replacement.get("optional_skip_reason") == removed.get("optional_skip_reason"),
                   "Coverage replacement must preserve the required/optional boundary")
        rows = inventory(root, replacement["argv"])
        weakened = (set(replacement.get("allowed_ctest_skips", [])) & expected.keys()) - set(removed.get("allowed_ctest_skips", []))
        ws.require(not weakened, "Coverage replacement would allow a previously required test to skip")
        covered.update(rows)
    ws.require(all(covered.get(name) == row for name, row in expected.items()),
               "Repair would remove required CTest coverage: " + removed["id"])


def status(root: Path) -> dict | None:
    """Probe existing state and lock without creating files or interpreting a PID."""
    path = ws.local_path(root, "verification.json")
    lock = lock_path(root)
    busy = False
    if lock.exists():
        with lock.open("rb") as stream:
            try:
                fcntl.flock(stream, fcntl.LOCK_EX | fcntl.LOCK_NB)
            except BlockingIOError:
                busy = True
    value = ws.read_json(path) if path.exists() else None
    if value is None:
        return {"lock_busy": True, "status": "running-in-shared-repository"} if busy else None
    value["lock_busy"] = busy
    if value["status"] == "running" and not busy:
        value["status"] = "interrupted"
    return value


@contextlib.contextmanager
def execution_lock(root: Path):
    with lock_path(root).open("a+b") as stream:
        try:
            fcntl.flock(stream, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError as error:
            raise ws.WorkflowError("Verification already running; use $main skill inspect and the existing tool session") from error
        yield stream


def junit_result(path: Path, selected: dict, allowed: list[str]) -> dict:
    try:
        suite = ET.parse(path).getroot()
    except (ET.ParseError, OSError) as error:
        raise ws.WorkflowError("Missing or malformed actual CTest report: " + str(path)) from error
    cases = list(suite.iter("testcase"))
    names = [case.get("name") for case in cases]
    ws.require(len(names) == len(set(names)) and set(names) == selected.keys(),
               "Executed CTest set differs from the enumerated required set")
    skipped = []
    for case in cases:
        name = case.get("name")
        ws.require(case.find("failure") is None and case.find("error") is None, "CTest failed: " + str(name))
        if case.find("skipped") is not None or case.get("status") in {"notrun", "disabled"}:
            skipped.append(name)
        else:
            ws.require(case.get("status", "run") == "run", "CTest did not execute: " + str(name))
    ws.require(set(skipped) <= set(allowed), "Undeclared CTest skips: " + ", ".join(sorted(set(skipped) - set(allowed))))
    return {"report": str(path), "report_digest": hashlib.sha256(path.read_bytes()).hexdigest(),
            "passed": sorted(set(names) - set(skipped)), "skipped": sorted(skipped)}


def execute(root: Path, kind: str, base: str, checks: list[dict], reviewed: dict, before: dict) -> dict:
    with execution_lock(root) as guard:
        ws.require(ws.snapshot(root) == before, "Verification inputs changed before acquiring its lock")
        attempt = uuid.uuid4().hex
        directory = ws.local_path(root, "logs/" + attempt)
        directory.mkdir(parents=True, exist_ok=False)
        state_path = ws.local_path(root, "verification.json")
        state = {"attempt": attempt, "request": reviewed["rules"]["request"],
                 "kind": kind, "base_revision": base, "input": before,
                 "plan": ws.digest(checks), "check": "preflight", "status": "running",
                 "started": time.time(), "completed_checks": 0}
        ws.atomic_json(state_path, state)
        results = []
        enumerated = False
        try:
            resolved = preflight(root, checks, defer_unconfigured=True)
            for number, check in enumerate(checks):
                if ctest_context(check["argv"]) is not None and not enumerated:
                    state.update(check="preflight")
                    state.pop("log", None)
                    state.pop("check_started", None)
                    ws.atomic_json(state_path, state)
                    resolved = preflight(root, checks)
                    enumerated = True
                log = directory / f"{number:04d}.log"
                report = directory / f"{number:04d}.xml"
                argv = check["argv"]
                ctest = resolved.get(check["id"])
                if ctest:
                    ws.require(inventory(root, argv) == ctest["tests"],
                               "CTest definitions changed after plan enumeration")
                executed = [*argv, "--output-junit", str(report)] if ctest else argv
                state.update(check=check["id"], log=str(log), check_started=time.time())
                ws.atomic_json(state_path, state)
                with log.open("xb") as output:
                    process = subprocess.Popen(executed, cwd=root, env=ws.environment(root),
                                               stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                               start_new_session=True, pass_fds=(guard.fileno(),))
                    assert process.stdout is not None
                    try:
                        for block in iter(lambda: process.stdout.read1(65536), b""):
                            output.write(block)
                            output.flush()
                            print(block.decode(errors="replace"), end="", flush=True)
                        code = process.wait()
                    except BaseException:
                        if process.poll() is None:
                            os.killpg(process.pid, signal.SIGTERM)
                            try:
                                process.wait(timeout=5)
                            except subprocess.TimeoutExpired:
                                os.killpg(process.pid, signal.SIGKILL)
                                process.wait()
                        raise
                    finally:
                        process.stdout.close()
                result = {"id": check["id"], "argv": argv, "executed_argv": executed, "returncode": code,
                          "log": str(log), "log_digest": hashlib.sha256(log.read_bytes()).hexdigest(),
                          "elapsed_seconds": time.time() - state["check_started"]}
                if code == 77 and check.get("optional_skip_reason"):
                    result["optional_skip_reason"] = check["optional_skip_reason"]
                ws.require(code == 0 or (code == 77 and bool(check.get("optional_skip_reason"))),
                           "Required verification failed: " + check["id"] + "; inspect " + str(log))
                if ctest:
                    ws.require(inventory(root, argv) == ctest["tests"],
                               "CTest definitions changed during execution")
                    result["ctest"] = {**ctest, **junit_result(report, ctest["tests"], check.get("allowed_ctest_skips", []))}
                results.append(result)
                ws.require(ws.snapshot(root) == before, "Verification changed its tested inputs; review and evaluate again")
                state["completed_checks"] = len(results)
                ws.atomic_json(state_path, state)
            receipt = {"schema_version": 2, "attempt": attempt, "kind": kind, "base_revision": base,
                       "input": before, "review": reviewed, "checks": checks, "results": results}
            receipt["digest"] = ws.digest(receipt)
            ws.atomic_json(ws.local_path(root, "receipts/" + receipt["digest"] + ".json"), receipt)
            state.update(status="passed", receipt=receipt["digest"])
            return receipt
        except BaseException as error:
            state.update(status="failed", error=str(error))
            raise
        finally:
            state["finished"] = time.time()
            ws.atomic_json(state_path, state)


def validate_execution(root: Path, receipt: dict, *, logs: bool) -> None:
    attempt = receipt.get("attempt")
    ws.require(isinstance(attempt, str) and len(attempt) == 32 and all(x in "0123456789abcdef" for x in attempt),
               "Missing exact verification attempt")
    # Candidate receipts may come from another supplied checkout. Their exact
    # absolute log paths stay bound; never relocate or regenerate old evidence.
    for number, (check, result) in enumerate(zip(receipt["checks"], receipt["results"])):
        log = Path(result["log"])
        ws.require(log.name == f"{number:04d}.log" and log.parent.name == attempt, "Verification log belongs to another attempt")
        if ctest_context(check["argv"]) is None:
            ws.require("ctest" not in result and result.get("executed_argv") == check["argv"], "Executed command changed")
            continue
        data = result.get("ctest", {})
        report = log.with_suffix(".xml")
        # Preset expansion belongs to the executed tree. A candidate's checkout
        # may have moved since then; validate its bound result, not current files.
        ws.require(data.get("selector_context") == ctest_context(check["argv"]) and
                   isinstance(data.get("context"), list) and bool(data["context"]) and
                   result.get("executed_argv") == [*check["argv"], "--output-junit", str(report)] and
                   data.get("report") == str(report) and bool(data.get("tests")), "Missing resolved CTest execution")
        ws.require(set(data.get("passed", [])) | set(data.get("skipped", [])) == data["tests"].keys() and
                   not set(data.get("passed", [])) & set(data.get("skipped", [])) and
                   set(data.get("skipped", [])) <= set(check.get("allowed_ctest_skips", [])), "CTest coverage is incomplete")
        if logs:
            actual = junit_result(report, data["tests"], check.get("allowed_ctest_skips", []))
            ws.require(all(data.get(key) == value for key, value in actual.items()), "CTest report changed after verification")
