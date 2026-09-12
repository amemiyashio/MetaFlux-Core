#!/usr/bin/env python3
"""Codex lifecycle guard for loading rules before repository mutations.

This prevents missed workflow steps in supported tool calls. It is not a shell
sandbox, an authorization service, or an agent identity mechanism.
"""

from __future__ import annotations

import argparse
import io
import json
import re
import shlex
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any

sys.path.insert(0, str(Path(__file__).resolve().parents[4] / "agent/lib"))
sys.path.insert(0, str(Path(__file__).resolve().parents[4] / "agent/skills/main/scripts"))
import main as controller
import rule_loading
import stage_rules
import workflow_state as ws

ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / "tools"))
from agent_diagnostics import emit_diagnostics, task_stop_error  # noqa: E402

MAX_CONTEXT_BYTES = 128 * 1024
MAIN_SCRIPT = "agent/skills/main/scripts/main.py"
MAIN_NOTICE = (
    "MetaFlux: load $main skill and the scope-owning skills before repository writes. "
    "Use $main skill (main.py inspect) for read-only questions; preserve an existing operation. "
    "Use its controller begin and load-rules before prepared. Enter commands through "
    "nix develop . --ignore-environment --keep HOME --keep USER --command; "
    "do not retain host PATH or LD_PRELOAD. Rule receipts record emitted "
    "bodies, not understanding or authorization. Coding subagents follow their "
    "parent briefing and never govern, commit, publish, or create contexts."
)
READ_TOOLS = {
    "view_image", "get_goal", "list_mcp_resources", "list_mcp_resource_templates",
    "read_mcp_resource", "mcp__filesystem__read_file", "mcp__filesystem__read_text_file",
    "mcp__filesystem__read_multiple_files", "mcp__filesystem__list_directory",
    "mcp__filesystem__directory_tree", "mcp__filesystem__get_file_info",
    "mcp__filesystem__search_files", "mcp__filesystem__list_allowed_directories",
    "mcp__codex_app__read_thread", "mcp__codex_app__list_threads",
    "mcp__codex_app__list_projects", "mcp__codex_app__read_thread_terminal",
}
FILE_TOOLS = {
    "mcp__filesystem__write_file": ("path",),
    "mcp__filesystem__edit_file": ("path",),
    "mcp__filesystem__create_directory": ("path",),
    "mcp__filesystem__move_file": ("source", "destination"),
}
EVENT_STAGES = {
    "prepared": {"preparation"},
    "review": {"implementation", "review", "evaluation", "delivery"},
    "evaluate": {"evaluation"},
    "deliver": {"delivery"},
    "publish": {"publication"},
    "handoff": {"handoff"},
    "repair": {"implementation", "review", "evaluation", "delivery"},
}


class ContextBuffer(io.StringIO):
    """Reject oversized bodies before rule_loading can issue its receipt."""

    def __init__(self) -> None:
        super().__init__()
        self.size = 0

    def write(self, value: str) -> int:
        self.size += len(value.encode("utf-8"))
        ws.require(self.size <= MAX_CONTEXT_BYTES, "Rule bodies exceed the hook context limit; narrow the current scope and load its required rules")
        return super().write(value)


@dataclass
class Operation:
    kind: str
    argv: list[str] | None = None
    paths: list[str] | None = None
    action: str | None = None
    nix: bool = False
    wrapper: list[str] | None = None
    delivery: str | None = None


def relative_path(root: Path, cwd: Path, value: str) -> str:
    ws.require(isinstance(value, str) and bool(value.strip()), "A concrete repository path is required")
    path = Path(value)
    ws.require(".." not in path.parts, "Parent traversal is outside a structured write target")
    target = path if path.is_absolute() else cwd / path
    target = target.resolve()
    ws.require(target.is_relative_to(root), "Structured write target is outside this repository")
    return target.relative_to(root).as_posix()


def patch_paths(command: str) -> list[str]:
    ws.require(isinstance(command, str), "apply_patch needs its structured command body")
    lines = command.strip().splitlines()
    ws.require(lines and lines[0] == "*** Begin Patch" and lines[-1] == "*** End Patch", "Malformed apply_patch envelope")
    paths = []
    for line in lines[1:-1]:
        for prefix in ("*** Add File: ", "*** Update File: ", "*** Delete File: ", "*** Move to: "):
            if line.startswith(prefix):
                paths.append(line[len(prefix):])
                break
    ws.require(bool(paths), "apply_patch contains no structured write targets")
    return paths


def segments(command: str) -> list[list[str]] | None:
    # This intentionally recognizes a small shell language. Unknown syntax is
    # treated as potentially mutating, never claimed to have been sandboxed.
    try:
        lexer = shlex.shlex(command, posix=True, punctuation_chars=";&|<>()\n")
        lexer.whitespace = " \t\r"
        lexer.whitespace_split = True
        lexer.commenters = ""
        words = list(lexer)
    except ValueError:
        return None
    result: list[list[str]] = [[]]
    for word in words:
        if word in {";", "&&", "||", "|", "\n"}:
            if result[-1]:
                result.append([])
        elif word and all(c in ";&|<>()\n" for c in word):
            return None
        else:
            result[-1].append(word)
    return [row for row in result if row]


def nix_inner(root: Path, cwd: Path, argv: list[str]) -> list[str] | None:
    if len(argv) < 4 or argv[:2] != ["nix", "develop"]:
        return None
    selector = argv[2].split("#", 1)[0]
    if selector.startswith("path:") or (cwd / selector).resolve() != root:
        return None
    i = 3
    clean = False
    while i < len(argv):
        if argv[i] == "--command":
            ws.require(clean, "Use clean Nix initialization: nix develop . --ignore-environment --keep HOME --keep USER --command ...")
            return argv[i + 1:]
        if argv[i] == "--ignore-environment":
            clean = True
            i += 1
        elif argv[i] == "--offline":
            i += 1
        elif argv[i] == "--keep" and i + 1 < len(argv):
            ws.require(argv[i + 1] in {"HOME", "USER"},
                       "Clean Nix entry retains only HOME/USER; initialize tools and environment inside Nix")
            i += 2
        else:
            return None
    return None


def python_script(root: Path, cwd: Path, argv: list[str]) -> tuple[str, list[str]] | None:
    if not argv or not re.fullmatch(r"python(?:3(?:\.\d+)?)?", Path(argv[0]).name):
        return None
    i = 1
    while i < len(argv) and argv[i] in {"-B", "-I", "-s", "-E"}:
        i += 1
    if i == len(argv) or argv[i].startswith("-"):
        return None
    try:
        name = relative_path(root, cwd, argv[i])
    except ws.WorkflowError:
        return None
    return name, argv[i + 1:]


def read_argv(argv: list[str]) -> bool:
    if not argv or any("$" in word or "`" in word for word in argv):
        return False
    name, args = argv[0], argv[1:]
    if name in {"pwd", "ls", "cat", "head", "tail", "wc", "stat", "readlink", "realpath"}:
        return True
    if name in {"rg", "grep"}:
        return not any(a.startswith(("--pre", "--hostname-bin")) for a in args)
    if name == "find":
        return not any(a.startswith(("-exec", "-ok", "-delete", "-fprint", "-fls")) for a in args)
    if name == "sed":
        return len(args) >= 2 and args[0] == "-n" and bool(re.fullmatch(r"\d+(?:,\d+|,\$)?p", args[1])) and all(not a.startswith("-") for a in args[2:])
    if name == "git":
        if args[:1] == ["--no-pager"]:
            args = args[1:]
        return bool(args) and args[0] in {"status", "diff", "show", "log", "rev-parse", "ls-files", "ls-tree", "grep", "check-ignore", "diff-tree", "merge-base"} and not any(a.startswith(("--output", "--ext-diff", "--textconv", "--open-files-in-pager", "-O")) for a in args[1:])
    return False


def shell_operation(root: Path, cwd: Path, command: str, *, nix: bool = False) -> Operation:
    rows = segments(command)
    if not rows:
        return Operation("shell", nix=nix)
    if len(rows) > 1:
        if all(read_argv(row) for row in rows) and (nix or all(row[0] == "git" for row in rows)):
            return Operation("read", nix=nix)
        return Operation("shell", nix=nix)
    argv = rows[0]
    inner = nix_inner(root, cwd, argv)
    if inner:
        return argv_operation(root, cwd, inner, nix=True)
    return argv_operation(root, cwd, argv, nix=nix)


def checked_script_root(root: Path, cwd: Path, args: list[str]) -> list[str]:
    """Validate one explicit parser root without granting a shell exception."""
    remaining = list(args)
    ws.require(not any(a.startswith("--root=") for a in remaining) and remaining.count("--root") <= 1,
               "Use one explicit --root path for the repository controller")
    if "--root" in remaining:
        index = remaining.index("--root")
        ws.require(index + 1 < len(remaining) and (cwd / remaining[index + 1]).resolve() == root,
                   "Repository controller command targets another root")
        del remaining[index:index + 2]
    else:
        ws.require(cwd == root, "Repository controller commands from a subdirectory need --root")
    return remaining


def argv_operation(root: Path, cwd: Path, argv: list[str], *, nix: bool) -> Operation:
    if argv[:2] == ["bash", "-c"] and len(argv) == 3:
        operation = shell_operation(root, cwd, argv[2], nix=nix)
        operation.wrapper = argv
        return operation
    if read_argv(argv):
        return Operation("read", argv=argv, nix=nix)
    script = python_script(root, cwd, argv)
    if script:
        name, args = script
        if name == MAIN_SCRIPT:
            if args[:1] == ["--root"] and len(args) >= 3:
                ws.require((cwd / args[1]).resolve() == root, "$main skill controller command targets another root")
                args = args[2:]
            else:
                ws.require(cwd == root, "$main skill controller commands from a subdirectory need --root")
            action = args[0] if args else ""
            if action in {"inspect", "preflight", "begin", "load-rules", "resume", "rescope", "supersede"}:
                return Operation("controller", argv=argv, action=action, nix=nix)
            if action == "step" and len(args) > 1 and args[1] in EVENT_STAGES:
                return Operation("controller", argv=argv, action=args[1], nix=nix)
        if name == "agent/skills/batch/scripts/batch.py":
            local_args = checked_script_root(root, cwd, args)
            if local_args[:1] in (["check"], ["check-metadata"]):
                return Operation("read", argv=argv, nix=nix)
            if len(local_args) >= 2 and local_args[0] in {"load-rules", "verify", "advance"}:
                return Operation("integration", argv=argv, action=local_args[0],
                                 delivery=str((cwd / local_args[1]).resolve()), nix=nix)
        if name in {"agent/skills/prepare/scripts/detect_agent_tool.py", "agent/skills/prepare/scripts/check_git_topology.py"}:
            return Operation("bootstrap", argv=argv, nix=nix)
        if name == "agent/skills/deliver/scripts/commit_as_agent_tool.py":
            return Operation("commit", argv=argv, nix=nix)
        if name == "agent/skills/publish/scripts/push_repository.py":
            local_args = checked_script_root(root, cwd, args)
            if local_args in (["check"], ["check", "--remote-access"]):
                return Operation("read", argv=argv, nix=nix)
            return Operation("publish", argv=argv, nix=nix)
    if argv[:2] == ["git", "add"]:
        args = argv[2:]
        while args and args[0] in {"-A", "--all", "--"}:
            args = args[1:]
        ws.require(bool(args) and all(not a.startswith("-") for a in args), "Stage concrete declared paths")
        return Operation("stage", argv=argv, paths=args, nix=nix)
    if argv[:2] in (["git", "commit"], ["git", "push"]):
        return Operation("ungoverned-git", argv=argv, nix=nix)
    return Operation("shell", argv=argv, nix=nix)


def classify(root: Path, event: dict[str, Any]) -> tuple[Operation, Path]:
    cwd = Path(event.get("cwd", str(root))).resolve()
    ws.require(cwd.is_relative_to(root), "Hook cwd is outside this repository")
    name, inputs = event.get("tool_name"), event.get("tool_input", {})
    if name in READ_TOOLS:
        return Operation("read"), cwd
    if name in {"Bash", "exec_command", "shell_command"}:
        ws.require(isinstance(inputs, dict), "Bash needs structured tool input")
        commands = [inputs[key] for key in ("command", "cmd") if key in inputs]
        ws.require(bool(commands) and all(isinstance(value, str) for value in commands) and
                   len(set(commands)) == 1, "Bash needs one unambiguous command string")
        workdirs = [inputs[key] for key in ("workdir", "cwd") if inputs.get(key) is not None]
        if workdirs:
            ws.require(all(isinstance(value, str) for value in workdirs), "Bash working directory must be a path")
            resolved = [(cwd / value).resolve() for value in workdirs]
            ws.require(len(set(resolved)) == 1 and resolved[0].is_relative_to(root), "Bash working directory must match this repository")
            cwd = resolved[0]
        return shell_operation(root, cwd, commands[0]), cwd
    if name == "apply_patch":
        # Codex's canonical hook shape is {command: text}; the callable tool
        # itself also exposes a FREEFORM string input in local adapters.
        command = inputs if isinstance(inputs, str) else inputs.get("command") if isinstance(inputs, dict) else None
        return Operation("files", paths=patch_paths(command)), cwd
    if name in FILE_TOOLS:
        ws.require(isinstance(inputs, dict), "Filesystem tool needs structured input")
        return Operation("files", paths=[inputs.get(p) for p in FILE_TOOLS[name]]), cwd
    # Unrecognized tools are not assumed read-only from their name. Their opaque
    # arguments receive workflow checks, not a fictional filesystem sandbox.
    return Operation("opaque"), cwd


def check_paths(root: Path, cwd: Path, paths: list[str], request: dict[str, Any], *, staging: bool = False) -> None:
    allowed = request.get("allowed_paths", [])
    ws.require(bool(allowed), "Current request needs explicit allowed_paths before writes")
    for value in paths:
        name = relative_path(root, cwd, value)
        ws.require(name != "agent/tmp" and not name.startswith("agent/tmp/"), "Controller temporary state is written only by its commands")
        ws.require(any(name == p or (p.endswith("/") and
                       (name.startswith(p) or (staging and name == p[:-1]))) for p in allowed),
                   "Write target is outside the declared file scope: " + name +
                   "; inspect and use $main skill rescope for a necessary same-task companion, then reload rules and prepare")
        if request["kind"] in {"maintenance", "iteration"}:
            ws.require(name != "agent/goal.json", "Goal mutations belong to $batch skill or $epoch skill")


def context_key(event: dict[str, Any]) -> str:
    values = [event.get("session_id"), event.get("turn_id")]
    ws.require(all(isinstance(v, str) and bool(v) for v in values), "Hook needs the current session and turn context before a write")
    # A host-supplied child discriminator is useful when available. Never fall
    # back to a session-only key: child hooks may share the parent's session.
    return ws.digest(["metaflux-rule-context-v1", *values, event.get("agent_id")])


def result(reason: str | None = None, context: str | None = None) -> dict[str, Any]:
    if reason is None and context is None:
        return {}
    output: dict[str, Any] = {"hookEventName": "PreToolUse"}
    if reason is not None:
        output.update(permissionDecision="deny", permissionDecisionReason=reason)
    if context is not None:
        output["additionalContext"] = context
    return {"hookSpecificOutput": output}


def diagnostic(reason: str) -> None:
    emit_diagnostics((task_stop_error(
        code="workflow.tool-precondition", source="main/tool-gate",
        summary="The tool call needs current workflow and loaded-rule evidence.",
        evidence=(reason,), responsibility="current-agent", disposition="fix-and-retry",
        required_action="Read the injected rules or inspect through $main skill, then correct the exact prerequisite before retrying.",
        resume_when="The supported call matches the active scope, stage, baseline, and current rule context.",
    ).diagnostic,), stream=sys.stderr)


def ensure_rules(root: Path, event: dict[str, Any], state: dict[str, Any], action: str, delivery: str | None = None) -> dict[str, Any] | None:
    request = state["request"]
    kind = request["kind"]
    key = context_key(event)
    expected_base = ws.oid(root) if state.get("commit") else request.get("base_revision")
    expected_request = None if state.get("commit") else state["run"]
    paths = () if state.get("commit") else request.get("allowed_paths", ())
    if action == "integration":
        sys.path.insert(0, str(ROOT / "agent/skills/batch/scripts"))
        import batch
        document = ws.read_json(Path(delivery))
        batch.integration_context(root, document)
        kind, expected_base = "integration", document["base_revision"]
        paths = controller.changed_paths(root, expected_base)
    needed = rule_loading.required(root, kind, paths, request.get("skills", ()), action)
    valid = False
    certificate: dict[str, Any] = {}
    try:
        certificate = rule_loading.current(root, kind=kind, action=action)
        valid = certificate["base_revision"] == expected_base and certificate["request"] == expected_request and set(needed) <= set(certificate["skills"])
    except (ws.WorkflowError, OSError, ValueError, KeyError, TypeError):
        pass
    binding_path = ws.local_path(root, "hook-context.json")
    try:
        binding = ws.read_json(binding_path)
    except (OSError, ValueError):
        binding = {}
    expected = {"schema_version": 1, "context": key, "run": state["run"], "rules": certificate.get("digest")}
    if valid and binding == expected:
        return None
    body = ContextBuffer()
    certificate = rule_loading.load(root, kind, expected_base, skills=request.get("skills", ()),
                                    paths=paths, action=action, output=body)
    ws.atomic_json(binding_path, {"schema_version": 1, "context": key, "run": state["run"], "rules": certificate["digest"]})
    return result("Read the full injected rule bodies before retrying this tool call; this invocation has not run.", body.getvalue())


def lifecycle(root: Path, event: dict[str, Any]) -> dict[str, Any]:
    name = event["hook_event_name"]
    if name in {"SessionStart", "SubagentStart"}:
        # Do not record a child receipt under the parent session/turn supplied
        # at SubagentStart. Its first write loads in its own PreToolUse context.
        notice = MAIN_NOTICE.replace("$main skill", f"[$main]({root / 'agent/skills/main/SKILL.md'}) skill")
        return {"hookSpecificOutput": {"hookEventName": name, "additionalContext": notice}}
    if name == "PostCompact":
        state_path = ws.local_path(root, "state.json")
        if state_path.is_file():
            state = ws.read_json(state_path)
            if (state.get("request", {}).get("kind") in {"maintenance", "iteration", "batch", "epoch"}
                    and state.get("stage") in {"preparation", "implementation", "review", "evaluation", "delivery", "publication", "handoff"}):
                binding = ws.local_path(root, "hook-context.json")
                if binding.exists():
                    binding.unlink()
        return {"systemMessage": "MetaFlux context compacted. The next repository write reloads current rules; task and publication state are preserved."}
    return {}


def pre_tool(root: Path, event: dict[str, Any]) -> dict[str, Any]:
    op, cwd = classify(root, event)
    if op.kind == "read":
        if op.argv:
            ws.require(op.nix or op.argv[0] == "git", "Run repository read executables through nix develop at the Git root")
        return {}
    if op.kind in {"bootstrap", "controller", "integration", "shell", "stage", "commit", "publish", "ungoverned-git"}:
        ws.require(op.nix, "Run repository executables through nix develop at the Git root")
    if op.kind == "bootstrap" or (op.kind == "controller" and op.action in {"inspect", "preflight", "begin", "load-rules", "resume", "rescope", "supersede"}):
        # These exact parsers own bootstrap/recovery validation; no general
        # shell compound receives this exception.
        return {}
    if op.kind == "integration" and op.action == "load-rules":
        # This exact parser checks the delivery/context and owns body emission.
        return {}
    ws.require(op.kind != "ungoverned-git", "Use $main skill with main.py step deliver or main.py step publish and the guarded helpers")
    state_path = ws.local_path(root, "state.json")
    ws.require(state_path.is_file(), "No active $main skill operation; begin the bounded request before writing")
    state = ws.read_json(state_path)
    ws.require(state.get("schema_version") == 1 and state.get("stage") not in {None, "complete"}, "No active mutable $main skill operation")
    request, stage = state["request"], state["stage"]
    ws.require(request.get("kind") in {"maintenance", "iteration", "batch", "epoch"}, "A read-only request does not enable writes")
    controller.validate_state(root, state)
    if op.kind == "controller":
        ws.require(stage in EVENT_STAGES[op.action], "Controller operation does not match the current stage")
    elif op.kind in {"stage", "commit"}:
        ws.require(stage == "delivery", "Staging and guarded commits require the delivery stage")
    elif op.kind == "publish":
        ws.require(stage == "publication" and bool(state.get("commit")), "Publication requires the exact committed operation")
        args = op.argv or []
        ws.require("--revision" in args and args[args.index("--revision") + 1:] == [state["commit"]], "Publish only the current full commit revision")
    elif op.kind == "shell" and any(x["argv"] in (op.argv, op.wrapper) for x in request.get("checks", [])):
        ws.require(stage in {"implementation", "review", "evaluation"}, "Planned checks require a pre-delivery working stage")
    else:
        ws.require(stage == "implementation", "Repository mutation requires implementation; use $main skill (main.py step repair) after review")
    if op.paths:
        check_paths(root, cwd, op.paths, request, staging=op.kind == "stage")
    action = stage_rules.EVENT_ACTION.get(op.action, "recover") if op.kind == "controller" else (
        "integration" if op.kind == "integration" else "deliver" if op.kind in {"stage", "commit"} else
        "publish" if op.kind == "publish" else "implement")
    return ensure_rules(root, event, state, action, op.delivery) or {}


def handle(root: Path, event: dict[str, Any]) -> dict[str, Any]:
    root = root.resolve()
    try:
        ws.require(isinstance(event, dict), "Hook input must be one JSON object")
        if event.get("hook_event_name") != "PreToolUse":
            return lifecycle(root, event)
        return pre_tool(root, event)
    except (ws.WorkflowError, OSError, ValueError, KeyError, TypeError, IndexError) as error:
        diagnostic(str(error))
        if isinstance(event, dict) and event.get("hook_event_name") != "PreToolUse":
            return {"systemMessage": "MetaFlux lifecycle inspection needs repair; use $main skill (main.py inspect) before the next write."}
        return result(str(error))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    args = parser.parse_args()
    try:
        event = json.load(sys.stdin)
    except (ValueError, OSError) as error:
        diagnostic(str(error))
        # Exit 2 blocks a PreToolUse call even when its input cannot be parsed.
        return 2
    print(json.dumps(handle(args.root, event), sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
