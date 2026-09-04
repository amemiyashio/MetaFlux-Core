#!/usr/bin/env python3
"""Self-tests for tools/probe-debug-kernel.py.

Uses synthetic config text written to temporary files — no real kernel is
required. Tests cover:
  - Full qualification (all y)
  - Mixed modules (m counts as qualified)
  - Missing configs reported as absent
  - --require-qualification exits 77 when any config is not y
  - JSON output shape
"""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path

SCRIPT = Path(__file__).with_name("probe-debug-kernel.py").resolve()

REQUIRED = [
    "CONFIG_KASAN",
    "CONFIG_KCSAN",
    "CONFIG_PROVE_LOCKING",
    "CONFIG_DEBUG_KMEMLEAK",
    "CONFIG_KUNIT",
]


def make_config(pairs: dict[str, str]) -> str:
    lines = [f"# comment line", ""]
    for key, val in sorted(pairs.items()):
        lines.append(f"{key}={val}")
    lines.append("")
    return "\n".join(lines)


def run_probe(config_text: str, extra_args: list[str] | None = None) -> tuple[int, dict]:
    with tempfile.NamedTemporaryFile(mode="w", suffix=".config", delete=False) as fh:
        fh.write(config_text)
        path = fh.name
    try:
        env = {**__import__("os").environ, "METAFLUX_KERNEL_CONFIG": path}
        result = subprocess.run(
            [sys.executable, "-B", str(SCRIPT), *(extra_args or [])],
            capture_output=True,
            text=True,
            env=env,
        )
        output = json.loads(result.stdout)
        return result.returncode, output
    finally:
        Path(path).unlink(missing_ok=True)


def test_all_qualified() -> None:
    cfg = make_config({k: "y" for k in REQUIRED})
    rc, out = run_probe(cfg)
    assert rc == 0, f"unexpected rc {rc}"
    assert out["qualified"] is True
    for k in REQUIRED:
        assert out["kernel_config"][k] == "y", f"{k} expected y, got {out['kernel_config'][k]}"
    print("PASS: all_qualified")


def test_modules_count_as_qualified() -> None:
    cfg = make_config({k: "m" for k in REQUIRED})
    rc, out = run_probe(cfg)
    assert rc == 0
    # m is treated as qualified (same as y for this gate)
    assert out["qualified"] is True
    for k in REQUIRED:
        assert out["kernel_config"][k] == "y", f"{k} expected y (m converted), got {out['kernel_config'][k]}"
    print("PASS: modules_count_as_qualified")


def test_missing_configs_are_absent() -> None:
    cfg = make_config({})
    rc, out = run_probe(cfg)
    assert rc == 0
    assert out["qualified"] is False
    for k in REQUIRED:
        assert out["kernel_config"][k] == "absent", f"{k} expected absent, got {out['kernel_config'][k]}"
    print("PASS: missing_configs_are_absent")


def test_require_qualification_exits_77() -> None:
    cfg = make_config({k: "n" for k in REQUIRED})
    rc, out = run_probe(cfg, extra_args=["--require-qualification"])
    assert rc == 77, f"expected 77, got {rc}"
    assert out["qualified"] is False
    print("PASS: require_qualification_exits_77")


def test_partial_qualification_exits_77() -> None:
    cfg = make_config({
        "CONFIG_KASAN": "y",
        "CONFIG_KCSAN": "n",
        "CONFIG_PROVE_LOCKING": "y",
        "CONFIG_DEBUG_KMEMLEAK": "y",
        "CONFIG_KUNIT": "y",
    })
    rc, out = run_probe(cfg, extra_args=["--require-qualification"])
    assert rc == 77
    assert out["qualified"] is False
    assert out["kernel_config"]["CONFIG_KCSAN"] == "n"
    print("PASS: partial_qualification_exits_77")


def test_json_shape() -> None:
    cfg = make_config({k: "y" for k in REQUIRED})
    rc, out = run_probe(cfg)
    assert rc == 0
    assert isinstance(out, dict)
    assert "kernel_config" in out
    assert "qualified" in out
    assert isinstance(out["kernel_config"], dict)
    assert isinstance(out["qualified"], bool)
    print("PASS: json_shape")


def main() -> int:
    tests = [
        test_all_qualified,
        test_modules_count_as_qualified,
        test_missing_configs_are_absent,
        test_require_qualification_exits_77,
        test_partial_qualification_exits_77,
        test_json_shape,
    ]
    failed = 0
    for test in tests:
        try:
            test()
        except Exception as exc:
            print(f"FAIL: {test.__name__}: {exc}", file=sys.stderr)
            failed += 1
    if failed:
        print(f"{failed} self-test(s) failed", file=sys.stderr)
        return 1
    print("All self-tests passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
