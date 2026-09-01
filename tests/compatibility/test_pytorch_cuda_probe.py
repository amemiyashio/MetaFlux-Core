#!/usr/bin/env python3
"""Self-tests for the PyTorch/CUDA gap probe; no PyTorch installation is required."""

from __future__ import annotations

import json
import os
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest


COMPATIBILITY_DIR = Path(__file__).resolve().parent
REPOSITORY_ROOT = COMPATIBILITY_DIR.parent.parent
sys.path.insert(0, str(COMPATIBILITY_DIR))

import pytorch_cuda_probe as probe  # noqa: E402


class FakeTensor:
    def __init__(self, values: list[int], dtype: object, device: str):
        self.values = list(values)
        self.dtype = dtype
        self.device = device
        self.shape = (len(values),)

    def copy_(self, source: "FakeTensor", non_blocking: bool = False) -> "FakeTensor":
        del non_blocking
        self.values = list(source.values)
        return self

    def to(self, device: str) -> "FakeTensor":
        return FakeTensor(self.values, self.dtype, device)

    def tolist(self) -> list[int]:
        return list(self.values)


class FakeCuda:
    def __init__(self, torch: "FakeTorch", major: int, minor: int, architectures: list[str]):
        self.torch = torch
        self.properties = SimpleNamespace(
            name="MetaFlux fixture",
            major=major,
            minor=minor,
            total_memory=8 * 1024 * 1024 * 1024,
        )
        self.architectures = architectures
        self.selected = 0

    def get_device_properties(self, index: int) -> SimpleNamespace:
        if index != 0:
            raise IndexError(index)
        self.torch.calls.append("get-device-properties")
        return self.properties

    def set_device(self, index: int) -> None:
        self.selected = index
        self.torch.calls.append("set-device")

    def synchronize(self, device: str) -> None:
        self.torch.calls.append(f"synchronize:{device}")

    def get_arch_list(self) -> list[str]:
        self.torch.calls.append("get-arch-list")
        return list(self.architectures)

    def _sleep(self, cycles: int) -> None:
        self.torch.calls.append(f"sleep:{cycles}")
        if self.torch.fail_sleep:
            raise RuntimeError("fixture artifact rejection")


class FakeTorch:
    int32 = "int32"

    def __init__(
        self,
        torch_version: str,
        cuda_version: str,
        *,
        major: int = 7,
        minor: int = 0,
        architectures: list[str] | None = None,
        device_count: int = 1,
    ):
        self.__version__ = torch_version
        self.version = SimpleNamespace(cuda=cuda_version)
        self.calls: list[str] = []
        self.fail_copy = False
        self.fail_sleep = False
        self.fail_add = False
        self._C = SimpleNamespace(_cuda_getDeviceCount=lambda: device_count)
        self.cuda = FakeCuda(self, major, minor, architectures or [f"sm_{major}{minor}"])

    def tensor(self, values: list[int], *, dtype: object, device: str) -> FakeTensor:
        self.calls.append(f"tensor:{device}")
        return FakeTensor(values, dtype, device)

    def empty_like(self, source: FakeTensor, *, device: str) -> FakeTensor:
        self.calls.append(f"empty-like:{device}")
        if self.fail_copy:
            raise RuntimeError("fixture runtime copy rejection")
        return FakeTensor([0] * len(source.values), source.dtype, device)

    def add(self, left: FakeTensor, right: FakeTensor) -> FakeTensor:
        self.calls.append("add")
        if self.fail_add:
            raise RuntimeError("fixture eager add rejection")
        return FakeTensor(
            [
                left_value + right_value
                for left_value, right_value in zip(left.values, right.values)
            ],
            left.dtype,
            left.device,
        )


class PytorchCudaProbeTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory(prefix="metaflux-pytorch-probe-selftest-")
        self.root = Path(self.temporary.name)
        self.client_manifest = self.root / "pytorch-cuda-clients-1.json"
        self.write_client_manifest()

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def write_client_manifest(self) -> None:
        self.client_manifest.write_text(
            json.dumps(
                {
                    "schema_version": 1,
                    "python": {
                        "version": "3.13.15",
                        "abi": "cp313",
                        "platform": "x86_64-linux",
                    },
                    "profiles": {
                        "baseline": {
                            "role": "current-sm70-gap-probe",
                            "torch_version": "2.11.0+cu126",
                            "cuda_version": "12.6",
                        },
                        "frontier": {
                            "role": "future-sm80-gap-probe",
                            "torch": {"version": "2.13.0+cu132"},
                            "cuda_version": "13.2",
                        },
                    },
                }
            )
            + "\n",
            encoding="ascii",
        )

    def run_fake(self, fake: FakeTorch, profile_name: str = "baseline") -> dict[str, object]:
        return probe.run_probe(
            profile_name,
            self.client_manifest,
            probe.DEFAULT_PTX_MANIFEST,
            importer=lambda: fake,
            python_version="3.13.15",
        )

    def test_complete_baseline_reports_exact_identity_and_decision_0017_bundle(self) -> None:
        fake = FakeTorch("2.11.0+cu126", "12.6")
        report = self.run_fake(fake)

        self.assertEqual(report["result"], "complete")
        self.assertIsNone(report["first_gap"])
        self.assertEqual(report["reached_stage"], "eager-add")
        self.assertEqual(
            report["advertised_compute_capability"],
            {"decision-0017": "7.0", "observed": "7.0"},
        )
        self.assertEqual(report["scope"], "diagnostic-only-not-milestone-0.1.0.0-compatibility-evidence")
        self.assertEqual(len(report["decision-0017"]["bundle_sha256"]), 64)
        self.assertEqual(
            set(report["decision-0017"]["entries"]),
            {"capabilities", "forms", "corpus_index"},
        )
        self.assertEqual(probe.required_stage_exit_code(report, None), 0)
        self.assertEqual(probe.required_stage_exit_code(report, "eager-add"), 0)
        self.assertIn("sleep:1", fake.calls)
        self.assertIn("add", fake.calls)

    def test_frontier_profile_is_selected_independently(self) -> None:
        fake = FakeTorch("2.13.0+cu132", "13.2", major=8, minor=0)
        report = self.run_fake(fake, "frontier")

        self.assertEqual(report["result"], "complete")
        self.assertEqual(report["expected_client"]["role"], "future-sm80-gap-probe")
        self.assertEqual(report["advertised_compute_capability"]["observed"], "8.0")

    def test_canonical_manifest_exposes_both_probe_identities(self) -> None:
        baseline = probe.load_client_profile(probe.DEFAULT_CLIENT_MANIFEST, "baseline")
        frontier = probe.load_client_profile(probe.DEFAULT_CLIENT_MANIFEST, "frontier")

        self.assertEqual(
            {key: baseline[key] for key in ("role", "python", "torch", "cuda")},
            {
                "role": "sm70-regression-probe",
                "python": "3.13.15",
                "torch": "2.11.0+cu126",
                "cuda": "12.6",
            },
        )
        self.assertEqual(
            {key: frontier[key] for key in ("role", "python", "torch", "cuda")},
            {
                "role": "future-sm80-gap-probe",
                "python": "3.13.15",
                "torch": "2.13.0+cu132",
                "cuda": "13.2",
            },
        )

    def test_identity_mismatch_is_a_default_success_and_blocks_later_stages(self) -> None:
        fake = FakeTorch("2.11.1+cu126", "12.6")
        report = self.run_fake(fake)

        self.assertEqual(report["first_gap"]["stage"], "import")
        self.assertEqual(report["first_gap"]["kind"], "client-identity-mismatch")
        self.assertEqual([stage["status"] for stage in report["stages"]], ["gap"] + ["blocked"] * 4)
        self.assertEqual(probe.required_stage_exit_code(report, None), 0)
        self.assertEqual(probe.required_stage_exit_code(report, "import"), 1)

    def test_driver_enumeration_disables_and_restores_nvml_check(self) -> None:
        fake = FakeTorch("2.11.0+cu126", "12.6")
        fake._C._cuda_getDeviceCount = lambda: 0
        previous = os.environ.get(probe.NVML_CHECK_ENV)
        os.environ[probe.NVML_CHECK_ENV] = "1"
        importer_saw_nvml = True

        def importer() -> FakeTorch:
            nonlocal importer_saw_nvml
            importer_saw_nvml = probe.NVML_CHECK_ENV in os.environ
            return fake

        try:
            report = probe.run_probe(
                "baseline",
                self.client_manifest,
                probe.DEFAULT_PTX_MANIFEST,
                importer=importer,
                python_version="3.13.15",
            )
            self.assertFalse(importer_saw_nvml)
            self.assertEqual(os.environ[probe.NVML_CHECK_ENV], "1")
        finally:
            if previous is None:
                os.environ.pop(probe.NVML_CHECK_ENV, None)
            else:
                os.environ[probe.NVML_CHECK_ENV] = previous

        self.assertEqual(report["first_gap"]["stage"], "driver-enumeration")
        self.assertEqual(report["first_gap"]["kind"], "no-cuda-device")
        self.assertEqual(probe.required_stage_exit_code(report, "import"), 0)
        self.assertEqual(probe.required_stage_exit_code(report, "runtime-copy"), 1)

    def test_runtime_artifact_and_eager_gaps_remain_distinct(self) -> None:
        runtime_fake = FakeTorch("2.11.0+cu126", "12.6")
        runtime_fake.fail_copy = True
        runtime_report = self.run_fake(runtime_fake)
        self.assertEqual(runtime_report["first_gap"]["kind"], "runtime-copy-failed")

        artifact_fake = FakeTorch("2.11.0+cu126", "12.6", architectures=["sm_80"])
        artifact_report = self.run_fake(artifact_fake)
        self.assertEqual(
            artifact_report["first_gap"]["kind"], "artifact-for-advertised-cc-missing"
        )
        self.assertNotIn("sleep:1", artifact_fake.calls)
        self.assertNotIn("add", artifact_fake.calls)

        eager_fake = FakeTorch("2.11.0+cu126", "12.6")
        eager_fake.fail_add = True
        eager_report = self.run_fake(eager_fake)
        self.assertEqual(eager_report["first_gap"]["kind"], "eager-add-failed")
        self.assertEqual(probe.required_stage_exit_code(eager_report, "artifact-intake"), 0)
        self.assertEqual(probe.required_stage_exit_code(eager_report, "eager-add"), 1)

    def test_invalid_client_manifest_is_fatal_configuration_input(self) -> None:
        manifest = json.loads(self.client_manifest.read_text(encoding="ascii"))
        del manifest["profiles"]["baseline"]["torch_version"]
        self.client_manifest.write_text(json.dumps(manifest), encoding="ascii")
        with self.assertRaisesRegex(probe.ManifestError, "torch_version"):
            probe.load_client_profile(self.client_manifest, "baseline")


if __name__ == "__main__":
    unittest.main()
