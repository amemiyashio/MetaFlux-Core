#!/usr/bin/env python3
"""Deterministic tests for bounded agent-tool detection."""

from __future__ import annotations

import importlib.util
import json
import subprocess
import sys
import tempfile
from pathlib import Path


SCRIPT = Path(__file__).with_name("detect_agent_tool.py").resolve()


def load_module():
    spec = importlib.util.spec_from_file_location("metaflux_agent_tool", SCRIPT)
    if spec is None or spec.loader is None:
        raise AssertionError("agent-tool detector cannot be loaded")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


DETECTOR = load_module()


def make_tool(root: Path, name: str, version_line: str = "fixture-cli 1.2.3") -> Path:
    root.mkdir(parents=True, exist_ok=True)
    path = root / name
    path.write_text(
        "#!/bin/sh\n"
        "if [ \"$1\" = \"--version\" ]; then\n"
        f"  printf '%s\\n' '{version_line}'\n"
        "  exit 0\n"
        "fi\n"
        "if [ \"$1\" = \"--help\" ]; then\n"
        "  printf '%s\\n' 'fixture help'\n"
        "  exit 0\n"
        "fi\n"
        "exit 2\n",
        encoding="utf-8",
    )
    path.chmod(0o755)
    return path


def expect_error(action, fragment: str) -> None:
    try:
        action()
    except DETECTOR.DetectionError as error:
        if fragment not in str(error):
            raise AssertionError(f"expected {fragment!r}, got {error!r}") from error
        return
    raise AssertionError(f"expected error containing {fragment!r}")


def test_explicit(root: Path) -> None:
    tool = make_tool(root, "fixture-agent")
    info = DETECTOR.detect_agent_tool(
        explicit=str(tool), environment={"PATH": ""}, inspect_ancestors=False
    )
    assert info.subject == "fixture-agent"
    assert info.interface == "cli"
    assert info.version == "1.2.3"
    assert info.source == "explicit"
    assert info.help_available
    assert len(info.executable_sha256) == 64
    fields = set(info.__dataclass_fields__)
    assert fields == {
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
    assert not fields & {"model", "provider", "backend", "template", "session"}

    target = make_tool(root, "runtime-entry")
    alias = root / "fixture-cli-alias"
    alias.symlink_to(target.name)
    aliased = DETECTOR.detect_agent_tool(
        explicit=str(alias), environment={"PATH": ""}, inspect_ancestors=False
    )
    assert aliased.subject == "fixture-cli-alias"
    assert aliased.executable == str(alias.absolute())


def test_launcher_and_path(root: Path) -> None:
    tool = make_tool(root, "fixture-cli")
    info = DETECTOR.detect_agent_tool(
        environment={
            "PATH": str(root),
            DETECTOR.TOOL_EXECUTABLE_ENV: str(tool),
        },
        candidate_names=("fixture-cli",),
        inspect_ancestors=False,
    )
    assert info.source == "launcher-environment"

    info = DETECTOR.detect_agent_tool(
        environment={"PATH": str(root)},
        candidate_names=("fixture-cli",),
        inspect_ancestors=False,
    )
    assert info.source == "path-singleton"

    original = DETECTOR.process_ancestor_candidates
    DETECTOR.process_ancestor_candidates = lambda: [tool]
    try:
        info = DETECTOR.detect_agent_tool(
            environment={"PATH": ""},
            candidate_names=(),
            inspect_ancestors=True,
        )
    finally:
        DETECTOR.process_ancestor_candidates = original
    assert info.source == "process-ancestor"


def test_ambiguity_and_contamination(root: Path) -> None:
    make_tool(root, "tool-one")
    make_tool(root, "tool-two")
    expect_error(
        lambda: DETECTOR.detect_agent_tool(
            environment={"PATH": str(root)},
            candidate_names=("tool-one", "tool-two"),
            inspect_ancestors=False,
        ),
        "multiple agent tools",
    )
    model_name = make_tool(root, "fixture-model-derived-agent")
    expect_error(
        lambda: DETECTOR.detect_agent_tool(
            explicit=str(model_name),
            environment={"PATH": ""},
            inspect_ancestors=False,
        ),
        "model or runtime metadata token",
    )
    contaminated = make_tool(
        root, "clean-tool", "clean-tool 1.2.3 model=MODEL_VALUE"
    )
    expect_error(
        lambda: DETECTOR.detect_agent_tool(
            explicit=str(contaminated),
            environment={"PATH": ""},
            inspect_ancestors=False,
        ),
        "contains model metadata",
    )


def test_cli(root: Path) -> None:
    tool = make_tool(root, "command-agent", "command-agent v4.5.6")
    result = subprocess.run(
        [sys.executable, str(SCRIPT), "--executable", str(tool), "--json"],
        check=False,
        capture_output=True,
        text=True,
    )
    assert result.returncode == 0, result.stderr
    document = json.loads(result.stdout)
    assert document["subject"] == "command-agent"
    assert document["version"] == "4.5.6"
    serialized = json.dumps(document).lower()
    assert "model" not in serialized
    assert "backend" not in serialized


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="metaflux-agent-tool-") as temp:
        root = Path(temp)
        test_explicit(root / "explicit")
        test_launcher_and_path(root / "launcher")
        test_ambiguity_and_contamination(root / "ambiguous")
        test_cli(root / "cli")
    print("agent-tool detector tests: 4 groups passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
