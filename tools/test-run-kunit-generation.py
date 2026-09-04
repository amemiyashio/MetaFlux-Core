#!/usr/bin/env python3
"""Self-tests for tools/run-kunit-generation.py.

Verifies:
  - exit 77 when METAFLUX_LINUX_SRC is unset
  - exit 77 when METAFLUX_LINUX_SRC points to a missing Makefile/kunit.py
  - exit 77 when METAFLUX_LINUX_SRC points to a broken tree (fake Makefile, no kunit.py)
  - exit non-zero on a fake tree with a fake kunit.py (fail-closed: don't
    report 0 when the "kernel" cannot be configured)

No real kernel is required for these tests.
"""

from __future__ import annotations

import os
import subprocess
import sys
import tempfile
from pathlib import Path

SCRIPT = Path(__file__).with_name("run-kunit-generation.py").resolve()


def run_runner(env: dict[str, str]) -> tuple[int, str, str]:
    cmd = [sys.executable, "-B", str(SCRIPT)]
    result = subprocess.run(cmd, capture_output=True, text=True, env=env)
    return result.returncode, result.stdout, result.stderr


def test_unset_metaflux_linux_src() -> None:
    env = {k: v for k, v in os.environ.items() if k != "METAFLUX_LINUX_SRC"}
    rc, out, err = run_runner(env)
    assert rc == 77, f"expected 77, got {rc}: stdout={out!r} stderr={err!r}"
    assert "SKIP" in err or "METAFLUX_LINUX_SRC" in err, f"expected SKIP message in stderr, got: {err!r}"
    print("PASS: unset_metaflux_linux_src")


def test_missing_src_root() -> None:
    env = {**os.environ, "METAFLUX_LINUX_SRC": "/nonexistent/path/to/kernel"}
    rc, out, err = run_runner(env)
    assert rc == 77, f"expected 77, got {rc}: stdout={out!r} stderr={err!r}"
    print("PASS: missing_src_root")


def test_fake_broken_tree() -> None:
    with tempfile.TemporaryDirectory() as tmpdir:
        fake_root = Path(tmpdir)
        # Create a fake Makefile but no kunit.py
        (fake_root / "Makefile").write_text("obj-m += fake.ko\n")
        env = {**os.environ, "METAFLUX_LINUX_SRC": str(fake_root)}
        rc, out, err = run_runner(env)
        # Should exit 77 — fail-closed, not 0
        assert rc == 77, f"expected 77, got {rc}: stdout={out!r} stderr={err!r}"
        print("PASS: fake_broken_tree")


def test_fake_tree_with_kunit_py() -> None:
    with tempfile.TemporaryDirectory() as tmpdir:
        fake_root = Path(tmpdir)
        (fake_root / "Makefile").write_text("obj-m += fake.ko\n")
        kunit_dir = fake_root / "tools" / "testing" / "kunit"
        kunit_dir.mkdir(parents=True)
        # Fake kunit.py must itself exit non-zero when invoked so the runner
        # does not report a spurious 0 (we want fail-closed behavior).
        (kunit_dir / "kunit.py").write_text(
            "#!/usr/bin/env python3\nimport sys; sys.exit(1)\n"
        )
        env = {**os.environ, "METAFLUX_LINUX_SRC": str(fake_root)}
        rc, out, err = run_runner(env)
        # Should exit non-zero (fake kunit.py failed) or 77 — never 0.
        assert rc != 0, f"expected non-zero, got {rc}: stdout={out!r} stderr={err!r}"
        print(f"PASS: fake_tree_with_kunit_py (rc={rc})")


def main() -> int:
    tests = [
        test_unset_metaflux_linux_src,
        test_missing_src_root,
        test_fake_broken_tree,
        test_fake_tree_with_kunit_py,
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
