#!/usr/bin/env python3
"""Verify byte-preserving generation and real incremental compile/link behavior."""
from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
MODULE = ROOT / "cmake/MetaFluxGeneratedFile.cmake"
OUTPUTS = (
    "plugins/compat/cuda/abi/driver/managed-renames.h",
    "plugins/compat/cuda/abi/driver/pytorch-cuda-cpu-kernels.h",
    "plugins/compat/cuda/abi/driver/exports.map",
    "plugins/compat/cuda/management/nvml/managed-renames.h",
    "plugins/compat/cuda/management/nvml/exports.map",
    "plugins/compat/cuda/libraries/cublas/exports.map",
    "plugins/compat/cuda/passthrough/tests/cuda-full.map",
    "plugins/compat/cuda/passthrough/tests/nvml-full.map",
)


def run(*argv: str, cwd: Path) -> str:
    result = subprocess.run(argv, cwd=cwd, capture_output=True, text=True)
    if result.returncode:
        raise AssertionError(result.stdout + result.stderr)
    return result.stdout


def fingerprints(paths) -> dict:
    return {str(p): (hashlib.sha256(p.read_bytes()).hexdigest(), p.stat().st_mtime_ns) for p in paths}


def compile_link_outputs(build: Path) -> set[Path]:
    outputs = set()
    for line in run("ninja", "-C", str(build), "-t", "targets", "all", cwd=build).splitlines():
        target, separator, rule = line.rpartition(": ")
        if separator and ("_COMPILER__" in rule or "_LINKER__" in rule):
            outputs.add((build / target).resolve())
    if not outputs:
        raise AssertionError("Expected actual CMake compile/link targets")
    return outputs


def build_records(build: Path) -> dict:
    # CMake regeneration records all its metadata outputs, not just build.ninja.
    # Bind the actual compile/link rules instead of excluding guessed filenames.
    products = compile_link_outputs(build)
    records = {}
    for line in (build / ".ninja_log").read_text().splitlines():
        fields = line.split("\t")
        if len(fields) != 5:
            continue
        output = (build / fields[3]).resolve()
        if output not in products:
            continue
        records[output] = fields[2:]
    return records


def verify_dev(build: Path) -> None:
    build = build.resolve()
    if build != (ROOT / "tmp/build/dev").resolve():
        raise AssertionError("Verify the existing dev build at its exact repository path")
    before = fingerprints(build / name for name in OUTPUTS)
    previous_log = build_records(build)
    products = [name for name in previous_log if name.is_file()]
    previous_products = fingerprints(products)
    print(run("cmake", "--preset", "dev", cwd=ROOT), end="")
    print(run("cmake", "--build", "--preset", "dev", "-j4", cwd=ROOT), end="")
    if before != fingerprints(build / name for name in OUTPUTS):
        raise AssertionError("Unchanged configure rewrote a generated product input")
    if previous_log != build_records(build) or previous_products != fingerprints(products):
        raise AssertionError("Unchanged configure/build changed a product build edge or output")
    print("stable dev generation: eight outputs unchanged; zero compile/link edges")


class GeneratedFilesTests(unittest.TestCase):
    def test_literal_generation_and_incremental_dependencies(self):
        scratch = ROOT / "tmp/work"
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="generated-files-", dir=scratch) as temporary:
            source = Path(temporary)
            build = source / "tmp/build/fixture"
            header = '#define VALUE 7\n/* ; \\ " @TOKEN@ $<CONFIG> */\n'
            (source / "kernel.ptx").write_text(header)
            (source / "symbols.def").write_text("FIXTURE_1 { global: answer; local: *; };\n")
            (source / "fixture.c").write_text('#include "generated.h"\nint answer(void) { return VALUE; }\n')
            (source / "CMakeLists.txt").write_text(
                "cmake_minimum_required(VERSION 3.25)\nproject(GeneratedFixture LANGUAGES C)\n"
                f"include([[{MODULE}]])\n"
                'file(GLOB fixture_sources CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/*.c")\n'
                'set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/kernel.ptx" "${CMAKE_SOURCE_DIR}/symbols.def")\n'
                'file(READ "${CMAKE_SOURCE_DIR}/kernel.ptx" header)\n'
                'file(READ "${CMAKE_SOURCE_DIR}/symbols.def" exports)\n'
                'metaflux_write_if_different("${CMAKE_BINARY_DIR}/generated.h" "${header}")\n'
                'metaflux_write_if_different("${CMAKE_BINARY_DIR}/exports.map" "${exports}")\n'
                'add_library(fixture SHARED fixture.c)\n'
                'target_include_directories(fixture PRIVATE "${CMAKE_BINARY_DIR}")\n'
                'target_link_options(fixture PRIVATE "LINKER:--version-script=${CMAKE_BINARY_DIR}/exports.map")\n'
                'set_property(TARGET fixture APPEND PROPERTY LINK_DEPENDS "${CMAKE_BINARY_DIR}/exports.map")\n')
            configure = ("cmake", "-S", str(source), "-B", str(build), "-G", "Ninja", "-DCMAKE_C_COMPILER=clang")
            run(*configure, cwd=source)
            run("cmake", "--build", str(build), cwd=source)
            generated = build / "generated.h"
            self.assertEqual(generated.read_bytes(), header.encode())
            # An old mtime makes a same-content rewrite observable without sleep.
            old = generated.stat().st_mtime_ns - 10_000_000_000
            os.utime(generated, ns=(old, old))
            paths = [generated, build / "exports.map", build / "libfixture.so", *build.rglob("*.o")]
            before = fingerprints(paths)
            log = build_records(build)
            run(*configure, cwd=source)
            run("cmake", "--build", str(build), cwd=source)
            self.assertEqual(before, fingerprints(paths))
            self.assertEqual(log, build_records(build))
            object_path = next(build.rglob("*.o"))
            object_before = object_path.read_bytes()
            (source / "kernel.ptx").write_text(header.replace("VALUE 7", "VALUE 9"))
            run("cmake", "--build", str(build), cwd=source)
            self.assertNotEqual(object_before, object_path.read_bytes())
            self.assertIn("VALUE 9", generated.read_text())
            object_before = fingerprints([object_path])
            library_before = (build / "libfixture.so").read_bytes()
            (source / "symbols.def").write_text("FIXTURE_2 { global: answer; local: *; };\n")
            run("cmake", "--build", str(build), cwd=source)
            self.assertEqual(object_before, fingerprints([object_path]))
            self.assertNotEqual(library_before, (build / "libfixture.so").read_bytes())


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--verify-dev", type=Path)
    args, remaining = parser.parse_known_args()
    if args.verify_dev:
        verify_dev(args.verify_dev)
    else:
        unittest.main(argv=[__file__, *remaining])
