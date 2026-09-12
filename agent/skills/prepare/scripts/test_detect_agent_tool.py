#!/usr/bin/env python3
"""Deterministic tests for conversation-emitted harness-name detection."""

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


def expect_error(action, code: str):
    try:
        action()
    except DETECTOR.DetectionError as error:
        diagnostic = error.diagnostic
        assert diagnostic.code == code
        assert diagnostic.evidence
        assert diagnostic.required_action
        assert diagnostic.resume_when
        return diagnostic
    raise AssertionError(f"expected diagnostic {code}")


def test_declared_name() -> None:
    info = DETECTOR.detect_agent_tool(explicit="zcode", environment={})
    assert info.subject == "zcode"
    assert info.interface == "cli"
    assert info.source == "declared"
    fields = set(info.__dataclass_fields__)
    assert fields == {"schema_version", "subject", "interface", "source"}
    assert not fields & {"model", "provider", "backend", "template", "session"}
    assert "executable" not in fields
    assert "version" not in fields

    from_env = DETECTOR.detect_agent_tool(
        environment={DETECTOR.TOOL_NAME_ENV: "Codex"}
    )
    assert from_env.subject == "codex"
    assert from_env.source == "declared"

    from_path = DETECTOR.detect_agent_tool(
        explicit="/tmp/.mount_ZCode-abcd/zcode", environment={}
    )
    assert from_path.subject == "zcode"


def test_missing_and_contamination() -> None:
    expect_error(
        lambda: DETECTOR.detect_agent_tool(environment={}),
        "agent-tool.missing-declaration",
    )
    expect_error(
        lambda: DETECTOR.detect_agent_tool(explicit="   ", environment={}),
        "agent-tool.empty-declaration",
    )
    expect_error(
        lambda: DETECTOR.detect_agent_tool(
            explicit="fixture-model-derived-agent", environment={}
        ),
        "agent-tool.prohibited-subject-input",
    )
    expect_error(
        lambda: DETECTOR.detect_agent_tool(explicit="***", environment={}),
        "agent-tool.invalid-subject",
    )


def test_cli() -> None:
    result = subprocess.run(
        [sys.executable, str(SCRIPT), "--agent-tool", "claude", "--json"],
        check=False,
        capture_output=True,
        text=True,
    )
    assert result.returncode == 0, result.stderr
    document = json.loads(result.stdout)
    assert document["subject"] == "claude"
    assert document["source"] == "declared"
    serialized = json.dumps(document).lower()
    assert "model" not in serialized
    assert "backend" not in serialized
    assert "executable" not in document
    assert "version" not in document

    rejected = subprocess.run(
        [
            sys.executable,
            str(SCRIPT),
            "--json",
            "--diagnostic-format",
            "json",
        ],
        check=False,
        capture_output=True,
        text=True,
        env={"PATH": ""},
    )
    assert rejected.returncode == 2
    assert rejected.stdout == ""
    failure = json.loads(rejected.stderr)
    assert failure["status"] == "error"
    diagnostic = failure["errors"][0]
    assert diagnostic["code"] == "agent-tool.missing-declaration"
    assert diagnostic["responsibility"] == "user-or-application"
    assert diagnostic["required_action"]


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="metaflux-agent-tool-"):
        test_declared_name()
        test_missing_and_contamination()
        test_cli()
    print("agent-tool detector tests: 3 groups passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
