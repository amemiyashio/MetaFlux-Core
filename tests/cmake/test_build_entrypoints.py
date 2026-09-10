#!/usr/bin/env python3
"""Verify build entrypoints without downloads, compilation, or GPU execution."""

import importlib.util
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[2]


def load_module(path: Path):
    spec = importlib.util.spec_from_file_location(path.stem, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class BuildEntrypointTests(unittest.TestCase):
    def run_command(self, command, **kwargs):
        return subprocess.run(command, text=True, capture_output=True, check=False, **kwargs)

    def test_shell_paths_fail_before_downloads_or_overlay_writes(self):
        with tempfile.TemporaryDirectory(dir=ROOT / "tmp" / "work", prefix="build-entrypoints-") as temp:
            work = Path(temp)
            bin_dir = work / "bin"
            bin_dir.mkdir()
            fake_nix = bin_dir / "nix"
            fake_nix.write_text("#!/bin/sh\nprintf 'unexpected materialization' >&2\nexit 99\n")
            fake_nix.chmod(0o755)
            env = dict(os.environ, PATH=f"{bin_dir}:{os.environ['PATH']}")
            env.pop("METAFLUX_LINUX_SRC", None)
            invalid = work / "build"
            for script, option in (
                ("build-generic-release.sh", "--build-dir"),
                ("build-debug-kernel.sh", "--cache-dir"),
            ):
                with self.subTest(script=script):
                    command = ["bash", str(ROOT / "tools" / script)]
                    result = self.run_command([*command, option, str(invalid)], env=env)
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn("Invalid MetaFlux build directory", result.stderr)
                    self.assertNotIn("unexpected materialization", result.stderr)
                    self.assertFalse(invalid.exists())
                    self.assertEqual(self.run_command([*command, "--help"], env=env).returncode, 0)
                    missing = self.run_command([*command, option], env=env)
                    self.assertNotEqual(missing.returncode, 0)
                    self.assertIn("Missing value", missing.stderr)

    def test_kernel_entrypoints_reject_before_resetting_existing_content(self):
        with tempfile.TemporaryDirectory(dir=ROOT / "tmp" / "work", prefix="build-entrypoints-") as temp:
            work = Path(temp)
            cache = work / "cache"
            sentinel = cache / "src" / "keep.txt"
            sentinel.parent.mkdir(parents=True)
            sentinel.write_text("keep")
            fake_source = work / "linux"
            kunit = fake_source / "tools" / "testing" / "kunit" / "kunit.py"
            kunit.parent.mkdir(parents=True)
            kunit.write_text("raise SystemExit(1)\n")
            (fake_source / "Makefile").write_text("# fixture\n")
            env = dict(os.environ, METAFLUX_LINUX_SRC=str(fake_source), METAFLUX_KUNIT_CACHE_DIR=str(cache))
            commands = (
                [sys.executable, "-B", str(ROOT / "tools" / "run-kunit-generation.py")],
                [sys.executable, "-B", str(ROOT / "tools" / "run-debug-kernel-qualification.py"),
                 "--cache-dir", str(cache), "--skip-kernel", "--skip-module",
                 "--skip-qualification-binary", "--skip-guest"],
            )
            for command in commands:
                with self.subTest(command=command[2]):
                    result = self.run_command(command, env=env)
                    self.assertEqual(result.returncode, 1, result.stderr)
                    self.assertIn("Invalid MetaFlux build directory", result.stderr)
                    self.assertNotIn("Traceback", result.stderr)
                    self.assertEqual(sentinel.read_text(), "keep")
                    self.assertFalse((cache / "build").exists())

    def test_kernel_cache_children_cannot_resolve_into_work_material(self):
        with tempfile.TemporaryDirectory(dir=ROOT / "tmp" / "build", prefix="entrypoint-symlink-") as build_temp, \
             tempfile.TemporaryDirectory(dir=ROOT / "tmp" / "work", prefix="build-entrypoints-") as work_temp:
            cache = Path(build_temp)
            work = Path(work_temp)
            sentinel = work / "keep.txt"
            sentinel.write_text("keep")
            fake_source = work / "linux"
            kunit = fake_source / "tools" / "testing" / "kunit" / "kunit.py"
            kunit.parent.mkdir(parents=True)
            kunit.write_text("raise SystemExit(1)\n")
            (fake_source / "Makefile").write_text("# fixture\n")
            env = dict(os.environ, METAFLUX_LINUX_SRC=str(fake_source), METAFLUX_KUNIT_CACHE_DIR=str(cache))
            commands = (
                ["bash", str(ROOT / "tools" / "build-debug-kernel.sh"), "--cache-dir", str(cache)],
                [sys.executable, "-B", str(ROOT / "tools" / "run-kunit-generation.py")],
                [sys.executable, "-B", str(ROOT / "tools" / "run-debug-kernel-qualification.py"),
                 "--cache-dir", str(cache), "--skip-kernel", "--skip-module",
                 "--skip-qualification-binary", "--skip-guest"],
            )
            for child in ("src", "build"):
                link = cache / child
                link.symlink_to(work, target_is_directory=True)
                for command in commands:
                    with self.subTest(child=child, command=command):
                        result = self.run_command(command, env=env)
                        self.assertNotEqual(result.returncode, 0)
                        self.assertIn("Invalid MetaFlux build directory", result.stderr)
                        self.assertEqual(sentinel.read_text(), "keep")
                        self.assertEqual(set(cache.iterdir()), {link})
                link.unlink()

    def test_vulkan_binary_selection_uses_only_explicit_or_canonical_tree(self):
        profile = load_module(ROOT / "tests" / "performance" / "test_milestone_0_1_3_6_vulkan_profile.py")
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            name = "metaflux_milestone_0_1_3_6_vulkan_stage_profile"
            stale = root / "tmp" / "build" / "old" / name
            stale.parent.mkdir(parents=True)
            stale.touch()
            canonical = root / "tmp" / "build" / "vulkan" / "plugins" / "backend" / "vulkan" / "runtime" / name
            with patch.object(profile, "ROOT", root), patch.dict(os.environ):
                os.environ.pop("METAFLUX_VULKAN_STAGE_BENCHMARK", None)
                with self.assertRaises(AssertionError):
                    profile.find_benchmark()
                canonical.parent.mkdir(parents=True)
                canonical.touch()
                self.assertEqual(profile.find_benchmark(), canonical)
                os.environ["METAFLUX_VULKAN_STAGE_BENCHMARK"] = str(root / "missing")
                with self.assertRaises(AssertionError):
                    profile.find_benchmark()
                os.environ["METAFLUX_VULKAN_STAGE_BENCHMARK"] = str(stale)
                self.assertEqual(profile.find_benchmark(), stale)


if __name__ == "__main__":
    (ROOT / "tmp" / "work").mkdir(parents=True, exist_ok=True)
    (ROOT / "tmp" / "build").mkdir(parents=True, exist_ok=True)
    unittest.main()
