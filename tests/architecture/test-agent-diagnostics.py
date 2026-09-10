#!/usr/bin/env python3
"""Behavioral tests for the shared task-stop diagnostic contract."""

from __future__ import annotations

import io
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

from agent_diagnostics import (
    DiagnosticArgumentParser,
    TaskStopDiagnostic,
    add_diagnostic_format_argument,
    bounded_evidence,
    emit_diagnostics,
    extract_diagnostic_envelopes,
    parse_diagnostic_envelope,
    task_stop_error,
)


def fixture() -> TaskStopDiagnostic:
    return task_stop_error(
        code="git-topology.local-clone",
        source="start-work / Stage Zero",
        summary="The current checkout is a standalone local clone.",
        evidence=("remote origin -> /source",),
        responsibility="user-or-application",
        disposition="preserve-and-report",
        required_action=(
            "Preserve this checkout and supply an existing registered Git context."
        ),
        resume_when="Stage Zero passes in the supplied registered context.",
    ).diagnostic


def expect_value_error(action) -> None:
    try:
        action()
    except ValueError:
        return
    raise AssertionError("expected ValueError")


def test_human_and_json() -> None:
    diagnostic = fixture()
    human = io.StringIO()
    emit_diagnostics((diagnostic,), stream=human)
    rendered = human.getvalue()
    for field in (
        "ERROR [git-topology.local-clone]",
        "responsibility: user-or-application",
        "disposition: preserve-and-report",
        "required_action:",
        "resume_when:",
    ):
        assert field in rendered

    structured = io.StringIO()
    emit_diagnostics((diagnostic,), diagnostic_format="json", stream=structured)
    document = json.loads(structured.getvalue())
    assert document["schema_version"] == 1
    assert document["status"] == "error"
    assert parse_diagnostic_envelope(structured.getvalue()) == (diagnostic,)

    mixed = "compiler output\n" + structured.getvalue() + "more output\n"
    extracted, passthrough = extract_diagnostic_envelopes(mixed)
    assert extracted == (diagnostic,)
    assert passthrough == "compiler output\nmore output\n"


def test_validation_and_bounds() -> None:
    diagnostic = fixture()
    fields = diagnostic.as_dict()
    expect_value_error(
        lambda: TaskStopDiagnostic.from_dict({**fields, "code": "ERROR-0001"})
    )
    expect_value_error(
        lambda: TaskStopDiagnostic.from_dict(
            {**fields, "responsibility": "unknown-owner"}
        )
    )
    expect_value_error(
        lambda: TaskStopDiagnostic.from_dict(
            {**fields, "disposition": "retry-forever"}
        )
    )
    evidence = bounded_evidence(["x" * 800] + [str(index) for index in range(20)])
    assert len(evidence) == 8
    assert len(evidence[0]) == 512


def test_argument_parser() -> None:
    parser = DiagnosticArgumentParser(
        prog="fixture-gate",
        diagnostic_source="fixture-gate",
    )
    add_diagnostic_format_argument(parser)
    parser.add_argument("--required", required=True)
    assert parser.parse_args(["--required", "value"]).required == "value"


def test_external_action_does_not_fan_out() -> None:
    action = fixture().required_action.lower()
    for forbidden in ("create", "clone", "new worktree", "new branch", "new thread"):
        assert forbidden not in action


def main() -> int:
    test_human_and_json()
    test_validation_and_bounds()
    test_argument_parser()
    test_external_action_does_not_fan_out()
    print("agent diagnostic self-test: 4/4 passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
