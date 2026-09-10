#!/usr/bin/env python3
"""Exercise the configure boundary with disposable, minimal source fixtures."""

import argparse
from pathlib import Path
import subprocess
import tempfile
import unittest


MODULE = Path(__file__).resolve().parents[2] / "cmake" / "MetaFluxBuildDirectory.cmake"
CMAKE = "cmake"


class BuildDirectoryTests(unittest.TestCase):
    def setUp(self) -> None:
        temporary = tempfile.TemporaryDirectory(prefix="metaflux build directory ")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.source = self.root / "source tree"
        self.source.mkdir()
        (self.source / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.25)\n"
            f'include([[{MODULE}]])\n'
            'metaflux_validate_build_directory("${CMAKE_CURRENT_SOURCE_DIR}" '
            '"${CMAKE_CURRENT_BINARY_DIR}")\n'
            'file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/project-entered" "entered")\n'
            "project(BuildDirectoryFixture LANGUAGES NONE)\n",
            encoding="utf-8",
        )

    def validate(self, binary: Path | str) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [CMAKE, f"-DMETAFLUX_SOURCE_DIR={self.source}",
             f"-DMETAFLUX_BINARY_DIR={binary}", "-P", str(MODULE)],
            cwd=self.source, text=True, capture_output=True, check=False,
        )

    def assert_rejected(self, result: subprocess.CompletedProcess[str]) -> None:
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("tmp/build/<name>", result.stderr)
        self.assertIn(str(self.source), " ".join(result.stderr.split()))

    def test_standalone_paths_leave_no_files(self) -> None:
        before = set(self.root.rglob("*"))
        allowed = (
            "tmp/build/dev",
            "tmp/build/debug-kernel/qualification-static",
            "tmp/build/first/../named tree/future/child",
            "tmp/build/named;tree/future",
            self.root / "external build" / "future",
            self.root / "external;build" / "future",
        )
        denied = (
            ".", "build", ".cache/build", "outputs/build", "tmp/build",
            "tmp/build-dev-vk", "tmp/work/build",
            "tmp/build;pollution",
            "tmp/build/dev/../../../source-pollution",
        )
        for binary in allowed:
            with self.subTest(binary=binary):
                result = self.validate(binary)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        for binary in denied:
            with self.subTest(binary=binary):
                self.assert_rejected(self.validate(binary))
        self.assertEqual(set(self.root.rglob("*")), before)

    def test_symlink_parents_and_future_children(self) -> None:
        build = self.source / "tmp" / "build"
        build.mkdir(parents=True)
        (build / "into-source").symlink_to(self.source, target_is_directory=True)
        (build / "future-source").symlink_to(
            self.source / "not-created", target_is_directory=True
        )
        external = self.root / "external alias"
        external.symlink_to(self.source, target_is_directory=True)
        # Resolve the link before '..': this returns to source/tmp, not tmp/build.
        (build / "parent-test").symlink_to(build, target_is_directory=True)
        before = set(self.root.rglob("*"))
        for binary in (
            build / "into-source" / "child",
            build / "future-source" / "child",
            external / "build" / "future",
            build / "parent-test" / ".." / "child",
        ):
            with self.subTest(binary=binary):
                self.assert_rejected(self.validate(binary))
        result = self.validate(external / "tmp" / "build" / "future")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(set(self.root.rglob("*")), before)

    def test_configure_accepts_named_and_external_trees(self) -> None:
        for binary in (
            self.source / "tmp" / "build" / "dev",
            self.source / "tmp" / "build" / "debug-kernel" / "qualification",
            self.root / "external build",
        ):
            with self.subTest(binary=binary):
                result = subprocess.run(
                    [CMAKE, "-S", str(self.source), "-B", str(binary)],
                    text=True, capture_output=True, check=False,
                )
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertTrue((binary / "project-entered").is_file())

    def test_configure_rejects_before_project(self) -> None:
        fixture = self.source / "CMakeLists.txt"
        fixture.write_text(
            fixture.read_text(encoding="utf-8").replace("LANGUAGES NONE", "LANGUAGES C"),
            encoding="utf-8",
        )
        for binary in (self.source, self.source / "build", self.source / "tmp" / "work" / "build"):
            with self.subTest(binary=binary):
                result = subprocess.run(
                    [CMAKE, "-S", str(self.source), "-B", str(binary),
                     "-DCMAKE_C_COMPILER=metaflux-no-compiler-probe-expected"],
                    text=True, capture_output=True, check=False,
                )
                self.assert_rejected(result)
                self.assertNotIn("CMAKE_C_COMPILER", result.stderr)
                self.assertFalse((binary / "project-entered").exists())
                self.assertFalse(list(binary.glob("CMakeFiles/*/CompilerId*")))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cmake", default=CMAKE)
    arguments, remaining = parser.parse_known_args()
    CMAKE = arguments.cmake
    unittest.main(argv=[__file__, *remaining])
