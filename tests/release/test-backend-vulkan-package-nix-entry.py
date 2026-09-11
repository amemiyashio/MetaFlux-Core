#!/usr/bin/env python3
"""Regression test for the release-package Nix fallback boundary."""

from __future__ import annotations

import importlib.util
from pathlib import Path
from types import SimpleNamespace
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[2]
TARGET = ROOT / "tests" / "release" / "test-backend-vulkan-package.py"
SPEC = importlib.util.spec_from_file_location("backend_vulkan_package", TARGET)
assert SPEC is not None and SPEC.loader is not None
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class ReleaseNixEntryTests(unittest.TestCase):
    def test_rpmbuild_fallback_clears_host_environment(self) -> None:
        def which(name: str) -> str | None:
            return "/nix/bin/nix" if name == "nix" else None

        with (
            patch.object(MODULE.shutil, "which", side_effect=which),
            patch.object(
                MODULE.subprocess,
                "run",
                return_value=SimpleNamespace(returncode=0),
            ) as run,
            patch.object(MODULE.sys, "argv", [str(TARGET), "--output", "result.json"]),
        ):
            self.assertEqual(MODULE.main(), 0)

        command = run.call_args.args[0]
        self.assertEqual(
            command[:10],
            [
                "/nix/bin/nix",
                "develop",
                ".#release",
                "--ignore-environment",
                "--keep",
                "HOME",
                "--keep",
                "USER",
                "--command",
                "python3",
            ],
        )


if __name__ == "__main__":
    unittest.main()
