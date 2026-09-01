#!/usr/bin/env python3
"""Validate the current MetaFlux goal-first agent state."""

from __future__ import annotations

import argparse
from contextlib import contextmanager
import json
import os
import re
import subprocess
import sys
from pathlib import Path
from typing import Any
from urllib.parse import unquote

from agent_diagnostics import (
    DiagnosticArgumentParser,
    TaskStopDiagnostic,
    add_diagnostic_format_argument,
    emit_diagnostics,
    parse_diagnostic_envelope,
    task_stop_error,
)


MILESTONE_ID_RE = re.compile(r"milestone-(\d+\.\d+\.\d+\.0)")
WORK_ITEM_ID_RE = re.compile(r"work-item-(\d+\.\d+\.\d+\.([1-9]\d*))")
DECISION_ID_RE = re.compile(r"decision-[0-9]{4}")
EXPERIENCE_ID_RE = re.compile(r"experience-[0-9]{4}")
EPOCH_ID_RE = re.compile(r"epoch-[0-9]{4}")
BATCH_ID_RE = re.compile(r"batch-[0-9]{4}")
ITERATION_ID_RE = re.compile(r"iteration-[0-9]{4}")
LANE_ID_RE = re.compile(r"lane-[a-z0-9]+(?:-[a-z0-9]+)*")
SKILL_SLUG_RE = re.compile(r"[a-z0-9]+(?:-[a-z0-9]+)*")

MILESTONE_STATUSES = {
    "Draft",
    "Queued",
    "Active",
    "Blocked",
    "Complete",
    "Superseded",
}
WORK_ITEM_STATUSES = MILESTONE_STATUSES
EXPERIENCE_STATUSES = {"Candidate", "Validated", "Superseded"}
LANE_STATUSES = {"planned", "integrated", "deferred"}
BATCH_STATUSES = {"open", "integrated"}

DOMAIN_SKILL_SLUGS = {
    "runtime-contracts-registry",
    "cuda-driver-abi-compatibility",
    "nvml-telemetry-compatibility",
    "ptx-simt-semantics",
    "mlir-compiler-engineering",
    "cpu-backend-performance",
    "linux-device-driver-uapi",
    "gpu-virtualization-vfio-user",
    "pcie-vpci-device-model",
    "device-lifecycle-resilience",
    "vulkan-spirv-compute",
}
WORKFLOW_SKILL_SLUGS = {
    "detect-agent-tool",
    "integrate-batch",
    "govern-epoch",
    "roast",
}
EXPLICIT_ONLY_SKILLS = {"integrate-batch", "govern-epoch", "roast"}
OBSOLETE_SKILLS = {
    "record-" + "session",
    "session-" + "guidance",
    "converge-project-" + "changes",
    "govern-semantic-" + "change",
}

FORBIDDEN_PATHS = (
    Path("agent") / ("sessions"),
    Path("agent") / ("progress"),
    Path("agent") / ("semantic-" + "changes"),
    Path("tools") / ("new-" + "session.py"),
    Path("tools") / ("check-semantic-" + "change-edits.py"),
    Path("agent")
    / "skills"
    / "start-work"
    / "scripts"
    / ("commit_as_" + "harness.py"),
    Path("agent")
    / "skills"
    / "start-work"
    / "scripts"
    / ("test_commit_as_" + "harness.py"),
)


def parse_scalar(raw: str) -> Any:
    value = raw.strip()
    if value == "null":
        return None
    if value in {"true", "false"}:
        return value == "true"
    if value.startswith("[") and value.endswith("]"):
        body = value[1:-1].strip()
        return [] if not body else [item.strip() for item in body.split(",")]
    if len(value) >= 2 and value[0] == value[-1] and value[0] in {'"', "'"}:
        return value[1:-1]
    return value


def parse_frontmatter(text: str) -> dict[str, Any] | None:
    lines = text.splitlines()
    if not lines or lines[0].strip() != "---":
        return None
    result: dict[str, Any] = {}
    for line in lines[1:]:
        if line.strip() == "---":
            return result
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        if ":" not in line:
            return None
        key, value = line.split(":", 1)
        key = key.strip()
        if not key or key in result:
            return None
        result[key] = parse_scalar(value)
    return None


def coordinate(value: object) -> tuple[int, int, int, int] | None:
    if not isinstance(value, str) or re.fullmatch(r"\d+\.\d+\.\d+\.\d+", value) is None:
        return None
    return tuple(int(part) for part in value.split("."))  # type: ignore[return-value]


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
        for dependency in graph.get(node, []):
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


class Checker:
    def __init__(self, root: Path):
        self.root = root.resolve()
        self.errors: list[TaskStopDiagnostic] = []
        self.records: dict[str, Path] = {}
        self.frontmatter: dict[str, dict[str, Any]] = {}
        self.decisions: set[str] = set()
        self._diagnostic_policy = {
            "code": "agent-state.invalid-authority",
            "responsibility": "current-agent",
            "disposition": "fix-and-retry",
            "required_action": (
                "Correct the canonical candidate file named in evidence without "
                "changing unrelated authority."
            ),
            "resume_when": "The same Agent state checker passes.",
        }

    def relative(self, path: Path) -> str:
        try:
            return path.relative_to(self.root).as_posix()
        except ValueError:
            return str(path)

    @contextmanager
    def diagnostic_policy(
        self,
        *,
        code: str,
        responsibility: str,
        disposition: str,
        required_action: str,
        resume_when: str,
    ):
        previous = self._diagnostic_policy
        self._diagnostic_policy = {
            "code": code,
            "responsibility": responsibility,
            "disposition": disposition,
            "required_action": required_action,
            "resume_when": resume_when,
        }
        try:
            yield
        finally:
            self._diagnostic_policy = previous

    def error(
        self,
        path: Path,
        message: str,
        *,
        code: str | None = None,
        responsibility: str | None = None,
        disposition: str | None = None,
        required_action: str | None = None,
        resume_when: str | None = None,
        extra_evidence: tuple[object, ...] = (),
    ) -> None:
        policy = self._diagnostic_policy
        effective_disposition = disposition or policy["disposition"]
        self.errors.append(
            task_stop_error(
                code=code or policy["code"],
                source="check-agent-state",
                summary=message,
                evidence=(f"path: {self.relative(path)}", *extra_evidence),
                responsibility=responsibility or policy["responsibility"],
                disposition=effective_disposition,
                required_action=required_action or policy["required_action"],
                resume_when=resume_when or policy["resume_when"],
                retry_command=(
                    "nix develop . --command python3 -B tools/check-agent-state.py ."
                    if effective_disposition == "fix-and-retry"
                    else None
                ),
            ).diagnostic
        )

    def read_text(self, path: Path) -> str | None:
        try:
            return path.read_text(encoding="utf-8")
        except (OSError, UnicodeDecodeError) as exc:
            self.error(path, f"cannot read UTF-8 text: {exc}")
            return None

    def document(self, path: Path) -> tuple[str, dict[str, Any]] | None:
        text = self.read_text(path)
        if text is None:
            return None
        fields = parse_frontmatter(text)
        if fields is None:
            self.error(path, "requires simple YAML frontmatter")
            return None
        return text, fields

    def validate_forbidden_paths(self) -> None:
        for relative in FORBIDDEN_PATHS:
            path = self.root / relative
            if path.exists() or path.is_symlink():
                self.error(path, "obsolete execution-history surface is forbidden")
        for slug in OBSOLETE_SKILLS:
            path = self.root / "agent" / "skills" / slug
            if path.exists() or path.is_symlink():
                self.error(path, "obsolete workflow skill is forbidden")

    def add_record(self, record_id: str, path: Path, fields: dict[str, Any]) -> None:
        previous = self.records.get(record_id)
        if previous is not None:
            self.error(path, f"duplicate id {record_id}; first seen at {self.relative(previous)}")
            return
        self.records[record_id] = path
        self.frontmatter[record_id] = fields

    def validate_plans(self) -> None:
        root = self.root / "agent" / "plan"
        if not root.is_dir():
            self.error(root, "plan directory is missing")
            return
        milestone_ids: set[str] = set()
        work_items: list[tuple[str, str, Path]] = []

        for directory in sorted(path for path in root.iterdir() if path.is_dir()):
            plan_path = directory / "plan.md"
            loaded = self.document(plan_path)
            if loaded is None:
                continue
            text, fields = loaded
            record_id = fields.get("id")
            delivery = fields.get("delivery")
            if not isinstance(record_id, str) or MILESTONE_ID_RE.fullmatch(record_id) is None:
                self.error(plan_path, "milestone id must match milestone-MAJOR.MINOR.PATCH.0")
                continue
            if directory.name != record_id + directory.name[len(record_id):] or not directory.name.startswith(record_id + "-"):
                self.error(directory, f"directory must start with {record_id}-")
            if delivery != record_id.removeprefix("milestone-"):
                self.error(plan_path, "milestone id must equal its dotted delivery")
            parsed = coordinate(delivery)
            if parsed is None or parsed[3] != 0:
                self.error(plan_path, "milestone delivery must end in .0")
            release = fields.get("release")
            if parsed is not None and release != f"v{parsed[0]}.{parsed[1]}.{parsed[2]}":
                self.error(plan_path, "release must match the first three delivery components")
            if fields.get("status") not in MILESTONE_STATUSES:
                self.error(plan_path, "milestone status is invalid")
            if not isinstance(fields.get("depends_on"), list):
                self.error(plan_path, "depends_on must be an inline list")
            self.add_record(record_id, plan_path, fields)
            milestone_ids.add(record_id)

            work_root = directory / "work"
            if not work_root.is_dir():
                self.error(work_root, "milestone work directory is missing")
                continue
            for work_path in sorted(work_root.glob("*.md")):
                work_loaded = self.document(work_path)
                if work_loaded is None:
                    continue
                work_text, work_fields = work_loaded
                work_id = work_fields.get("id")
                work_delivery = work_fields.get("delivery")
                if not isinstance(work_id, str) or WORK_ITEM_ID_RE.fullmatch(work_id) is None:
                    self.error(work_path, "work item id must match work-item-MAJOR.MINOR.PATCH.WORK")
                    continue
                if not work_path.name.startswith(work_id + "-"):
                    self.error(work_path, f"filename must start with {work_id}-")
                if work_delivery != work_id.removeprefix("work-item-"):
                    self.error(work_path, "work item id must equal its dotted delivery")
                parsed_work = coordinate(work_delivery)
                if parsed is not None and parsed_work is not None and parsed_work[:3] != parsed[:3]:
                    self.error(work_path, "work item delivery must share its milestone release")
                if work_fields.get("milestone") != record_id:
                    self.error(work_path, f"milestone must be {record_id}")
                if work_fields.get("status") not in WORK_ITEM_STATUSES:
                    self.error(work_path, "work item status is invalid")
                if not isinstance(work_fields.get("depends_on"), list):
                    self.error(work_path, "depends_on must be an inline list")
                if "## Exit Gate" not in work_text:
                    self.error(work_path, "work item requires an Exit Gate")
                self.add_record(work_id, work_path, work_fields)
                work_items.append((work_id, record_id, work_path))

        graph: dict[str, list[str]] = {}
        for record_id, fields in self.frontmatter.items():
            dependencies = fields.get("depends_on")
            if not isinstance(dependencies, list):
                continue
            graph[record_id] = []
            for dependency in dependencies:
                if not isinstance(dependency, str) or dependency not in self.records:
                    self.error(self.records[record_id], f"unresolved dependency: {dependency!r}")
                else:
                    graph[record_id].append(dependency)
        cycle = find_cycle(graph)
        if cycle is not None:
            self.error(root, "plan dependency cycle: " + " -> ".join(cycle))

    def validate_goal(self) -> None:
        with self.diagnostic_policy(
            code="agent-state.goal-invalid",
            responsibility="batch-integrator",
            disposition="stop-and-report",
            required_action=(
                "The explicit Batch integrator must correct Goal schema, target, "
                "Batch, or lane authority in its product integration commit; a "
                "worker must leave goal.json unchanged."
            ),
            resume_when="The corrected Goal passes this checker in the authorized integration context.",
        ):
            self._validate_goal()

    def _validate_goal(self) -> None:
        path = self.root / "agent" / "goal.json"
        try:
            goal = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
            self.error(path, f"cannot read goal JSON: {exc}")
            return
        if not isinstance(goal, dict):
            self.error(path, "goal must be an object")
            return
        expected = {"schema_version", "epoch", "batch", "target", "objective", "lanes"}
        if set(goal) != expected:
            self.error(path, f"goal keys must be exactly {sorted(expected)}")
        if goal.get("schema_version") != 1:
            self.error(path, "schema_version must be 1")
        epoch = goal.get("epoch")
        if not isinstance(epoch, str) or EPOCH_ID_RE.fullmatch(epoch) is None:
            self.error(
                path,
                "epoch must match epoch-NNNN",
                code="agent-state.epoch-invalid",
                responsibility="epoch-governor",
                disposition="stop-and-report",
                required_action=(
                    "The explicit Epoch governor must correct Epoch authority; a "
                    "worker or Batch integrator must not invent or advance it."
                ),
                resume_when=(
                    "The published or candidate Epoch is valid in its authorized "
                    "governance context."
                ),
            )

        batch = goal.get("batch")
        if not isinstance(batch, dict) or set(batch) != {"id", "status"}:
            self.error(path, "batch must contain exactly id and status")
        else:
            if not isinstance(batch.get("id"), str) or BATCH_ID_RE.fullmatch(batch["id"]) is None:
                self.error(path, "batch.id must match batch-NNNN")
            if batch.get("status") not in BATCH_STATUSES:
                self.error(path, "batch.status is invalid")

        target = goal.get("target")
        target_milestone = target_work = None
        if not isinstance(target, dict) or set(target) != {"milestone", "work_item"}:
            self.error(path, "target must contain exactly milestone and work_item")
        else:
            target_milestone = target.get("milestone")
            target_work = target.get("work_item")
            if target_milestone not in self.records or MILESTONE_ID_RE.fullmatch(str(target_milestone)) is None:
                self.error(path, "target milestone does not resolve")
            if target_work not in self.records or WORK_ITEM_ID_RE.fullmatch(str(target_work)) is None:
                self.error(path, "target work item does not resolve")
            elif self.frontmatter[str(target_work)].get("milestone") != target_milestone:
                self.error(path, "target work item does not belong to target milestone")
        if not isinstance(goal.get("objective"), str) or not goal["objective"].strip():
            self.error(path, "objective must be a non-empty string")

        lanes = goal.get("lanes")
        if not isinstance(lanes, list) or not lanes:
            self.error(path, "lanes must be a non-empty list")
            return
        lane_ids: set[str] = set()
        iterations: set[str] = set()
        graph: dict[str, list[str]] = {}
        expected_lane_keys = {
            "id",
            "iteration",
            "outcome",
            "status",
            "depends_on",
            "acceptance",
        }
        for index, lane in enumerate(lanes):
            where = f"lane[{index}]"
            if not isinstance(lane, dict) or set(lane) != expected_lane_keys:
                self.error(path, f"{where} keys must be exactly {sorted(expected_lane_keys)}")
                continue
            lane_id = lane.get("id")
            iteration = lane.get("iteration")
            if not isinstance(lane_id, str) or LANE_ID_RE.fullmatch(lane_id) is None:
                self.error(path, f"{where}.id must be a descriptive lane slug")
                continue
            if lane_id in lane_ids:
                self.error(path, f"duplicate lane id: {lane_id}")
            lane_ids.add(lane_id)
            if not isinstance(iteration, str) or ITERATION_ID_RE.fullmatch(iteration) is None:
                self.error(path, f"{where}.iteration must match iteration-NNNN")
            elif iteration in iterations:
                self.error(path, f"duplicate iteration id: {iteration}")
            else:
                iterations.add(iteration)
            if not isinstance(lane.get("outcome"), str) or not lane["outcome"].strip():
                self.error(path, f"{where}.outcome must be non-empty")
            if lane.get("status") not in LANE_STATUSES:
                self.error(path, f"{where}.status is invalid")
            dependencies = lane.get("depends_on")
            if not isinstance(dependencies, list) or any(not isinstance(item, str) for item in dependencies):
                self.error(path, f"{where}.depends_on must be a string list")
                graph[lane_id] = []
            else:
                graph[lane_id] = dependencies
            acceptance = lane.get("acceptance")
            if not isinstance(acceptance, list) or not acceptance or any(
                not isinstance(item, str) or not item.strip() for item in acceptance
            ):
                self.error(path, f"{where}.acceptance must be a non-empty string list")
        for lane_id, dependencies in graph.items():
            for dependency in dependencies:
                if dependency not in lane_ids:
                    self.error(path, f"lane {lane_id} has unresolved dependency {dependency}")
        cycle = find_cycle(graph)
        if cycle is not None:
            self.error(path, "lane dependency cycle: " + " -> ".join(cycle))
        if isinstance(batch, dict) and batch.get("status") == "integrated":
            if any(isinstance(lane, dict) and lane.get("status") == "planned" for lane in lanes):
                self.error(path, "integrated Batch cannot contain planned lanes")

    def current_epoch(self) -> str | None:
        path = self.root / "agent" / "goal.json"
        try:
            goal = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, UnicodeDecodeError, json.JSONDecodeError):
            return None
        epoch = goal.get("epoch") if isinstance(goal, dict) else None
        return epoch if isinstance(epoch, str) and EPOCH_ID_RE.fullmatch(epoch) else None

    def validate_commit_environment(
        self, environment: dict[str, str] | None = None
    ) -> None:
        with self.diagnostic_policy(
            code="commit-gate.environment-invalid",
            responsibility="current-agent",
            disposition="fix-and-retry",
            required_action=(
                "Use the start-work commit helper with the exact detected executable; "
                "do not override Author, Committer, or the candidate gate."
            ),
            resume_when="The same candidate commit environment passes this gate.",
        ):
            self._validate_commit_environment(environment)

    def _validate_commit_environment(
        self, environment: dict[str, str] | None = None
    ) -> None:
        values = os.environ if environment is None else environment
        path = self.root / "agent" / "goal.json"
        detector = (
            self.root
            / "agent"
            / "skills"
            / "detect-agent-tool"
            / "scripts"
            / "detect_agent_tool.py"
        )
        try:
            result = subprocess.run(
                [
                    sys.executable,
                    "-B",
                    str(detector),
                    "--json",
                    "--diagnostic-format",
                    "json",
                ],
                check=False,
                capture_output=True,
                text=True,
                timeout=10,
                env=values,
            )
        except (OSError, subprocess.TimeoutExpired) as error:
            self.error(
                path,
                "agent-tool detector did not complete",
                extra_evidence=(f"failure: {type(error).__name__}",),
            )
            return
        if result.returncode != 0:
            try:
                self.errors.extend(parse_diagnostic_envelope(result.stderr))
            except (ValueError, json.JSONDecodeError):
                self.error(
                    path,
                    "agent-tool detector rejected the commit environment without a valid diagnostic",
                    extra_evidence=(
                        f"return code: {result.returncode}",
                        f"detector stderr: {result.stderr.strip() or '<empty>'}",
                    ),
                )
            return
        try:
            tool = json.loads(result.stdout)
        except json.JSONDecodeError:
            self.error(
                path,
                "agent-tool detector did not emit valid JSON",
                extra_evidence=(f"detector stdout: {result.stdout.strip() or '<empty>'}",),
            )
            return
        expected_tool_fields = {
            "schema_version",
            "subject",
            "interface",
            "executable",
            "version",
            "source",
            "version_probe",
            "help_available",
            "executable_sha256",
        }
        if not isinstance(tool, dict) or set(tool) != expected_tool_fields:
            self.error(path, "agent-tool detector emitted fields outside tool evidence")
            return
        subject = tool.get("subject") if isinstance(tool, dict) else None
        if not isinstance(subject, str) or SKILL_SLUG_RE.fullmatch(subject) is None:
            self.error(path, "agent-tool detector emitted an invalid subject")
            return
        declared_executable = values.get("METAFLUX_AGENT_TOOL_EXECUTABLE")
        executable = tool.get("executable")
        if (
            not isinstance(declared_executable, str)
            or not Path(declared_executable).is_absolute()
            or not isinstance(executable, str)
            or executable != declared_executable
        ):
            self.error(
                path,
                "METAFLUX_AGENT_TOOL_EXECUTABLE must be the exact detected path",
            )
        if (
            tool.get("schema_version") != 1
            or tool.get("interface") != "cli"
            or not isinstance(tool.get("version"), str)
            or tool.get("version_probe") != "--version"
            or not isinstance(tool.get("help_available"), bool)
            or not isinstance(tool.get("executable_sha256"), str)
        ):
            self.error(path, "agent-tool detector emitted invalid tool evidence")
        expected_identity = {
            "GIT_AUTHOR_NAME": subject,
            "GIT_AUTHOR_EMAIL": f"{subject}@localhost",
            "GIT_COMMITTER_NAME": subject,
            "GIT_COMMITTER_EMAIL": f"{subject}@localhost",
        }
        for name, expected in expected_identity.items():
            if values.get(name) != expected:
                self.error(path, f"{name} must be exactly {expected}")

    def git(self, *arguments: str) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            ["git", *arguments],
            cwd=self.root,
            check=False,
            capture_output=True,
            text=True,
        )

    def validate_integration_revisions(self, base: str, tip: str) -> None:
        with self.diagnostic_policy(
            code="integration.revision-invalid",
            responsibility="user-or-application",
            disposition="preserve-and-report",
            required_action=(
                "Preserve the candidate and supply exact committed base/tip revisions "
                "whose base is on current main at or after Epoch activation; do not "
                "spawn a replacement worker or source copy."
            ),
            resume_when=(
                "The supplied committed revisions pass Epoch and ancestry validation."
            ),
        ):
            self._validate_integration_revisions(base, tip)

    def _validate_integration_revisions(self, base: str, tip: str) -> None:
        path = self.root / "agent" / "goal.json"
        epoch = self.current_epoch()
        if epoch is None:
            self.error(path, "cannot validate integration without a current Epoch")
            return

        resolved: dict[str, str] = {}
        for label, revision in (("base", base), ("tip", tip)):
            result = self.git("rev-parse", "--verify", f"{revision}^{{commit}}")
            if result.returncode != 0:
                self.error(path, f"integration {label} is not a committed Git revision")
                return
            resolved[label] = result.stdout.strip()

        history = self.git("rev-list", "--reverse", "HEAD", "--", "agent/goal.json")
        if history.returncode != 0:
            self.error(path, "cannot inspect Epoch activation history")
            return
        activation = None
        for revision in history.stdout.splitlines():
            shown = self.git("show", f"{revision}:agent/goal.json")
            if shown.returncode != 0:
                continue
            try:
                candidate = json.loads(shown.stdout)
            except json.JSONDecodeError:
                continue
            if isinstance(candidate, dict) and candidate.get("epoch") == epoch:
                activation = revision
                break
        if activation is None:
            self.error(
                path,
                f"{epoch} has no published activation commit",
                code="integration.epoch-activation-missing",
                responsibility="epoch-governor",
                disposition="stop-and-report",
                required_action=(
                    "The Epoch governor must publish or repair the activation commit; "
                    "the integrator must leave candidates and Goal state unchanged."
                ),
                resume_when="The current Epoch resolves to one published activation commit.",
            )
            return

        ancestry = (
            (activation, resolved["base"], "integration base predates the current Epoch"),
            (resolved["base"], resolved["tip"], "integration base is not an ancestor of tip"),
            (resolved["base"], "HEAD", "integration base is not on current main history"),
        )
        for ancestor, descendant, message in ancestry:
            if self.git("merge-base", "--is-ancestor", ancestor, descendant).returncode != 0:
                self.error(path, message)

    def validate_decisions(self) -> None:
        path = self.root / "agent" / "memory" / "decisions-index.md"
        text = self.read_text(path)
        if text is None:
            return
        for line in text.splitlines():
            match = re.match(r"^\|\s*(decision-[0-9]{4})\s*\|", line)
            if match is None:
                continue
            decision = match.group(1)
            if decision in self.decisions:
                self.error(path, f"duplicate decision row: {decision}")
            self.decisions.add(decision)
        if "decision-0033" not in self.decisions:
            self.error(path, "decision-0033 must establish the active execution model")
        if "decision-0034" not in self.decisions:
            self.error(path, "decision-0034 must establish agent-tool detection")
        for document in self.text_files():
            text_value = self.read_text(document)
            if text_value is None:
                continue
            for decision in set(DECISION_ID_RE.findall(text_value)):
                if decision not in self.decisions:
                    self.error(document, f"decision reference does not resolve: {decision}")

    def validate_experiences(self) -> None:
        root = self.root / "agent" / "experience"
        if not root.is_dir():
            self.error(root, "experience directory is missing")
            return
        actual: set[str] = set()
        for path in sorted(root.glob("experience-*.md")):
            loaded = self.document(path)
            if loaded is None:
                continue
            _, fields = loaded
            record_id = fields.get("id")
            if not isinstance(record_id, str) or EXPERIENCE_ID_RE.fullmatch(record_id) is None:
                self.error(path, "experience id must match experience-NNNN")
                continue
            if not path.name.startswith(record_id + "-"):
                self.error(path, f"filename must start with {record_id}-")
            if fields.get("status") not in EXPERIENCE_STATUSES:
                self.error(path, "experience status is invalid")
            actual.add(record_id)
        index = self.read_text(root / "README.md")
        if index is not None:
            indexed = set(EXPERIENCE_ID_RE.findall(index))
            if indexed != actual:
                self.error(root / "README.md", f"experience index drift: actual={sorted(actual)}, indexed={sorted(indexed)}")

    def validate_skills(self) -> None:
        root = self.root / "agent" / "skills"
        if not root.is_dir():
            self.error(root, "skills directory is missing")
            return
        entry = self.root / ".agents" / "skills"
        if not entry.is_symlink() or entry.readlink().as_posix() != "../agent/skills":
            self.error(entry, "must be a symlink to ../agent/skills")
        actual = {
            path.name
            for path in root.iterdir()
            if path.is_dir() and SKILL_SLUG_RE.fullmatch(path.name)
        }
        for slug in sorted(actual):
            skill_path = root / slug / "SKILL.md"
            loaded = self.document(skill_path)
            if loaded is None:
                continue
            _, fields = loaded
            if fields.get("name") != slug:
                self.error(skill_path, f"skill name must equal directory slug {slug}")
            description = fields.get("description")
            if not isinstance(description, str) or not description.strip():
                self.error(skill_path, "skill description must be non-empty")
            yaml_path = root / slug / "agents" / "openai.yaml"
            if slug in DOMAIN_SKILL_SLUGS | WORKFLOW_SKILL_SLUGS | {"start-work"} and not yaml_path.is_file():
                self.error(yaml_path, "routed skill requires agents/openai.yaml")
            if yaml_path.is_file():
                yaml_text = self.read_text(yaml_path)
                if yaml_text is None:
                    continue
                token = "$" + slug
                if yaml_text.count(token) != 1:
                    self.error(yaml_path, f"default prompt must contain {token} exactly once")
                if slug in EXPLICIT_ONLY_SKILLS and "allow_implicit_invocation: false" not in yaml_text:
                    self.error(yaml_path, "explicit-only skill must disable implicit invocation")
        index_path = root / "README.md"
        index_text = self.read_text(index_path)
        if index_text is None:
            return
        indexed = set(
            match.group(1)
            for match in re.finditer(r"^\| \[([a-z0-9-]+)\]\(\1/SKILL\.md\) \|", index_text, re.MULTILINE)
        )
        if indexed != actual:
            self.error(index_path, f"skill index drift: actual={sorted(actual)}, indexed={sorted(indexed)}")
        self.validate_trigger_corpus(root)

    def validate_trigger_corpus(self, skills_root: Path) -> None:
        path = skills_root / "trigger-evals.json"
        try:
            corpus = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
            self.error(path, f"cannot read routing corpus: {exc}")
            return
        if not isinstance(corpus, dict):
            self.error(path, "routing corpus must be an object")
            return
        if set(corpus.get("domain_skills", [])) != DOMAIN_SKILL_SLUGS:
            self.error(path, "domain_skills must match the current domain roster")
        if set(corpus.get("workflow_skills", [])) != WORKFLOW_SKILL_SLUGS:
            self.error(path, "workflow_skills must match the current workflow roster")
        known = DOMAIN_SKILL_SLUGS | WORKFLOW_SKILL_SLUGS
        cases = corpus.get("cases")
        if not isinstance(cases, list) or not cases:
            self.error(path, "routing corpus requires cases")
            return
        for index, case in enumerate(cases):
            if not isinstance(case, dict):
                self.error(path, f"case[{index}] must be an object")
                continue
            for field in ("expected_skills", "forbidden_skills"):
                values = case.get(field)
                if not isinstance(values, list) or any(value not in known for value in values):
                    self.error(path, f"case[{index}].{field} contains an unknown skill")

    def headings(self, path: Path) -> set[str]:
        text = self.read_text(path)
        if text is None:
            return set()
        slugs: set[str] = set()
        counts: dict[str, int] = {}
        for line in text.splitlines():
            match = re.match(r"^#{1,6}\s+(.+?)\s*#*\s*$", line)
            if match is None:
                continue
            heading = re.sub(r"`([^`]*)`", r"\1", match.group(1)).lower()
            slug = re.sub(r"[^\w\- ]", "", heading, flags=re.UNICODE)
            slug = re.sub(r"\s+", "-", slug.strip())
            count = counts.get(slug, 0)
            counts[slug] = count + 1
            slugs.add(slug if count == 0 else f"{slug}-{count}")
        return slugs

    def validate_links(self) -> None:
        for path in self.markdown_files():
            text = self.read_text(path)
            if text is None:
                continue
            for match in re.finditer(r"!?\[[^\]]*\]\(([^)]+)\)", text):
                raw = match.group(1).strip()
                if not raw or raw.startswith(("http://", "https://", "mailto:", "#")):
                    continue
                if raw.startswith("<") and raw.endswith(">"):
                    raw = raw[1:-1]
                if " " in raw and not raw.startswith("../") and not raw.startswith("./"):
                    raw = raw.split(" ", 1)[0]
                target_text, separator, anchor = raw.partition("#")
                if not target_text or any(token in target_text for token in ("{", "}", "$")):
                    continue
                target = (path.parent / unquote(target_text)).resolve()
                if not target.exists():
                    self.error(path, f"broken local link: {raw}")
                    continue
                if separator and target.is_file() and target.suffix.lower() == ".md":
                    if unquote(anchor).lower() not in self.headings(target):
                        self.error(path, f"broken Markdown anchor: {raw}")

    def markdown_files(self) -> list[Path]:
        return [path for path in self.root.rglob("*.md") if ".git" not in path.parts and not any(part.startswith("build") for part in path.parts)]

    def text_files(self) -> list[Path]:
        allowed = {".md", ".json", ".py", ".sh", ".cmake", ".txt", ".yaml", ".yml"}
        result: list[Path] = []
        for path in self.root.rglob("*"):
            if not path.is_file() or ".git" in path.parts or any(part.startswith("build") for part in path.parts):
                continue
            if path.suffix.lower() in allowed or path.name in {"AGENTS.md", "CMakeLists.txt", "CLAUDE.md"}:
                result.append(path)
        return result

    def validate_legacy_markers(self) -> None:
        literal_markers = (
            "METAFLUX_" + "SESSION_ID",
            "METAFLUX_AGENT_" + "HARNESS",
            "METAFLUX_AGENT_" + "EPOCH",
            "owner_" + "session",
            "agent/" + "sessions",
            "agent/" + "progress/focus.json",
            "agent/" + "semantic-changes",
            "commit_as_" + "harness.py",
            "test_commit_as_" + "harness.py",
        )
        legacy_patterns = (
            re.compile(r"(?<![A-Za-z0-9])(?:M|W|D)[0-9]{4,}(?![A-Za-z0-9])", re.IGNORECASE),
            re.compile(r"(?<![A-Za-z0-9])E[0-9]{4}(?![A-Za-z0-9])", re.IGNORECASE),
            re.compile(r"(?<![A-Za-z0-9])SC[0-9]{4}(?![A-Za-z0-9])", re.IGNORECASE),
            re.compile(r"\bP[0-9]{8}-[0-9]{3}\b"),
            re.compile(r"\bS[0-9]{4,}-[0-9]{8}-[0-9]{3}-[a-z0-9-]+\b"),
            re.compile(r"\bG[0-9]{3}\b"),
            re.compile(r"\bA[0-9]{3}\b"),
        )
        for path in self.root.rglob("*"):
            if ".git" in path.parts or any(part.startswith("build") for part in path.parts):
                continue
            relative = path.relative_to(self.root)
            for part in relative.parts:
                for pattern in legacy_patterns[:5]:
                    match = pattern.search(part)
                    if match is not None:
                        self.error(path, f"legacy abbreviated record id in path: {match.group(0)}")
        for path in self.text_files():
            text = self.read_text(path)
            if text is None:
                continue
            for marker in literal_markers:
                if marker in text:
                    self.error(path, f"legacy execution marker is forbidden: {marker}")
            for pattern in legacy_patterns:
                match = pattern.search(text)
                if match is not None:
                    self.error(path, f"legacy abbreviated record id is forbidden: {match.group(0)}")

    def run(
        self,
        *,
        commit_gate: bool = False,
        integration_revisions: tuple[str, str] | None = None,
        diagnostic_format: str = "human",
    ) -> bool:
        self.validate_forbidden_paths()
        self.validate_plans()
        self.validate_goal()
        if commit_gate:
            self.validate_commit_environment()
        if integration_revisions is not None:
            self.validate_integration_revisions(*integration_revisions)
        self.validate_decisions()
        self.validate_experiences()
        self.validate_skills()
        self.validate_links()
        self.validate_legacy_markers()
        if self.errors:
            emit_diagnostics(
                sorted(
                    set(self.errors),
                    key=lambda error: (
                        error.code,
                        error.source,
                        error.summary,
                        error.evidence,
                    ),
                ),
                diagnostic_format=diagnostic_format,
            )
            return False
        print(
            "agent state: ok "
            f"({len(self.records)} plan records, {len(self.decisions)} decisions)"
        )
        return True


def parser() -> argparse.ArgumentParser:
    result = DiagnosticArgumentParser(
        description=__doc__, diagnostic_source="check-agent-state"
    )
    result.add_argument("root", nargs="?", default=".", type=Path)
    result.add_argument("--commit-gate", action="store_true")
    result.add_argument("--integration-base")
    result.add_argument("--integration-tip")
    add_diagnostic_format_argument(result)
    return result


def main() -> int:
    arguments = parser().parse_args()
    if (arguments.integration_base is None) != (arguments.integration_tip is None):
        error = task_stop_error(
            code="integration.revision-pair-required",
            source="check-agent-state",
            summary="Integration base and tip must be provided together.",
            evidence=(
                f"integration base supplied: {arguments.integration_base is not None}",
                f"integration tip supplied: {arguments.integration_tip is not None}",
            ),
            responsibility="current-agent",
            disposition="fix-and-retry",
            required_action="Supply both exact committed revisions in the same invocation.",
            resume_when="The integration gate receives both base and tip revisions.",
        )
        emit_diagnostics(
            (error.diagnostic,), diagnostic_format=arguments.diagnostic_format
        )
        return 2
    revisions = None
    if arguments.integration_base is not None:
        revisions = (arguments.integration_base, arguments.integration_tip)
    return 0 if Checker(arguments.root).run(
        commit_gate=arguments.commit_gate,
        integration_revisions=revisions,
        diagnostic_format=arguments.diagnostic_format,
    ) else 1


if __name__ == "__main__":
    raise SystemExit(main())
