#!/usr/bin/env python3

from __future__ import annotations

import json
from pathlib import Path
import subprocess
import tempfile
import unittest

import run_m0001_optimization as optimization


class OptimizationQualificationTests(unittest.TestCase):
    def test_git_source_identity_requires_clean_head_and_tree(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repository = Path(temporary)
            subprocess.run(["git", "init", "-q", repository], check=True)
            source = repository / "source.c"
            source.write_text("int value = 1;\n", encoding="utf-8")
            subprocess.run(["git", "-C", repository, "add", "source.c"], check=True)
            subprocess.run(
                [
                    "git",
                    "-C",
                    repository,
                    "-c",
                    "user.name=MetaFlux Test",
                    "-c",
                    "user.email=metaflux-test@example.invalid",
                    "commit",
                    "-q",
                    "-m",
                    "fixture",
                ],
                check=True,
            )
            identity = optimization.git_source_identity(repository)
            self.assertEqual(identity["kind"], "clean-git-head-tree")
            self.assertTrue(identity["clean"])
            self.assertEqual(len(identity["git_revision"]), 40)
            self.assertEqual(len(identity["git_tree"]), 40)

            source.write_text("int value = 2;\n", encoding="utf-8")
            with self.assertRaisesRegex(
                optimization.QualificationError, "clean Git worktree"
            ):
                optimization.git_source_identity(repository)

            source.write_text("int value = 1;\n", encoding="utf-8")
            (repository / "untracked.txt").write_text("new\n", encoding="utf-8")
            with self.assertRaisesRegex(
                optimization.QualificationError, "clean Git worktree"
            ):
                optimization.git_source_identity(repository)

    def test_fresh_output_rejects_preexisting_content(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "evidence"
            optimization.ensure_fresh_output(output)
            (output / "old.profraw").write_bytes(b"old")
            with self.assertRaises(optimization.QualificationError):
                optimization.ensure_fresh_output(output)

    def test_work_directory_is_removed_unless_explicitly_kept(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            requested = Path(temporary) / "work"
            work = optimization.prepare_work_directory(requested)
            (work / "build-artifact").write_bytes(b"fixture")
            self.assertTrue(optimization.finalize_work_directory(work, keep=True))
            self.assertTrue(work.is_dir())
            self.assertFalse(optimization.finalize_work_directory(work, keep=False))
            self.assertFalse(work.exists())

    def test_work_cleanup_rejects_an_unmarked_directory(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary) / "unmarked"
            work.mkdir()
            with self.assertRaisesRegex(
                optimization.QualificationError, "unmarked work directory"
            ):
                optimization.finalize_work_directory(work, keep=False)

    def test_fuzz_generation_is_deterministic_and_covers_operations(self) -> None:
        corpus = (("a.ptx", b".version 9.0\nret;\n"), ("b.ptx", b"broken"))
        total = len(corpus) + len(optimization.FUZZ_OPERATIONS)
        first = optimization.generate_fuzz_cases(corpus, total, 0x1234, 128)
        second = optimization.generate_fuzz_cases(corpus, total, 0x1234, 128)
        self.assertEqual(first, second)
        operations = {case["operation"] for case in first}
        self.assertTrue(set(optimization.FUZZ_OPERATIONS).issubset(operations))
        self.assertTrue(all(len(case["data"]) <= 128 for case in first))

    def test_fuzz_generation_rejects_shallow_case_count(self) -> None:
        corpus = (("a.ptx", b"ret;"),)
        with self.assertRaises(optimization.QualificationError):
            optimization.generate_fuzz_cases(corpus, 1, 1, 64)

    def test_profile_summary_requires_executed_counters(self) -> None:
        summary = optimization.parse_profile_summary(
            "Instrumentation level: Front-end\n"
            "Total functions: 8\n"
            "Maximum function count: 17\n"
            "Total count: 41\n"
        )
        self.assertEqual(summary["maximum_function_count"], 17)
        with self.assertRaises(optimization.QualificationError):
            optimization.parse_profile_summary(
                "Total functions: 8\nMaximum function count: 0\nTotal count: 41\n"
            )

    def test_profile_roles_require_every_owning_component(self) -> None:
        names = (
            "mf_cuda_managed_cuInit",
            "mf_nvml_managed_nvmlInit_v2",
            "_ZN8metaflux8compiler14compile_kernelEv",
        )
        roles = optimization.profile_roles(names)
        self.assertTrue(all(role["matched_count"] > 0 for role in roles.values()))
        with self.assertRaises(optimization.QualificationError):
            optimization.profile_roles(names[:2])

    def test_compile_commands_require_exact_optimization_and_thin_lto(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "compile_commands.json"
            source = "/src/plugins/compat/cuda/abi/driver/src/provider.c"
            path.write_text(
                json.dumps(
                    [
                        {
                            "file": source,
                            "command": "clang -O2 -flto=thin -c " + source,
                        }
                    ]
                ),
                encoding="utf-8",
            )
            result = optimization.check_variant_compile_commands(path, "O2")
            self.assertEqual(result["count"], 1)
            path.write_text(
                json.dumps(
                    [
                        {
                            "file": source,
                            "command": "clang -O2 -O3 -flto=thin -c " + source,
                        }
                    ]
                ),
                encoding="utf-8",
            )
            with self.assertRaises(optimization.QualificationError):
                optimization.check_variant_compile_commands(path, "O2")

    def test_fuzz_outcome_must_be_explicit(self) -> None:
        self.assertEqual(
            optimization.parse_fuzz_outcome("outcome=rejected input_bytes=4\n"),
            "rejected",
        )
        with self.assertRaises(optimization.QualificationError):
            optimization.parse_fuzz_outcome("ok\n")

    def test_partial_stage_evidence_preserves_progress(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            progress = output / "hardening" / "progress.json"
            progress.parent.mkdir()
            progress.write_text(
                json.dumps(
                    {
                        "status": "fail",
                        "fuzz": {"status": "pass", "case_count": 64},
                        "soak": {"status": "fail", "completed_runs": 5},
                    }
                ),
                encoding="utf-8",
            )
            evidence = optimization.partial_stage_evidence(output, "hardening")
            self.assertEqual(evidence["partial"]["fuzz"]["status"], "pass")
            self.assertEqual(evidence["partial"]["soak"]["completed_runs"], 5)
            self.assertEqual(evidence["progress_evidence"]["path"], "hardening/progress.json")
            self.assertEqual(len(evidence["progress_evidence"]["sha256"]), 64)

    def test_pgo_use_suppresses_only_unprofiled_translation_units(self) -> None:
        class Recorder:
            argv: list[str]

            def run(self, _label: str, argv: object, **_kwargs: object) -> dict[str, object]:
                self.argv = [str(value) for value in argv]
                return {}

        recorder = Recorder()
        tools = {
            "cmake": Path("/tools/cmake"),
            "ninja": Path("/tools/ninja"),
            "clang": Path("/tools/clang"),
            "clangxx": Path("/tools/clang++"),
        }
        optimization.configure_full_build(
            recorder,
            "fixture",
            Path("/source"),
            Path("/build"),
            tools,
            Path("/toolchain"),
            Path("/headers"),
            build_type="Release",
            lto=True,
            sanitizers=False,
            pgo_mode="USE",
            profile=Path("/profile.profdata"),
        )
        expected = " ".join(optimization.PGO_USE_DIAGNOSTIC_FLAGS)
        self.assertIn(f"-DCMAKE_C_FLAGS={expected}", recorder.argv)
        self.assertIn(f"-DCMAKE_CXX_FLAGS={expected}", recorder.argv)
        self.assertIn("-Wno-profile-instr-unprofiled", expected)


if __name__ == "__main__":
    unittest.main()
