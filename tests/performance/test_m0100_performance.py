#!/usr/bin/env python3

from __future__ import annotations

import copy
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest

import run_m0100_performance as performance


def daemon_counter_line(**overrides: int) -> str:
    values = {
        "host-address-space-registrations": 1,
        "direct-host-source-operations": 7,
        "direct-host-source-bytes": 117_440_512,
        "direct-host-destination-operations": 7,
        "direct-host-destination-bytes": 117_440_512,
        "staged-host-source-operations": 0,
        "staged-host-source-bytes": 0,
        "staged-host-destination-operations": 0,
        "staged-host-destination-bytes": 0,
    }
    values.update(overrides)
    fields = " ".join(f"{name}={value}" for name, value in values.items())
    return f"metafluxd: cpu-execution mode=interpreter compiler-requests=0 {fields}\n"


def direct_metrics() -> dict[str, dict[str, object]]:
    return {
        name: {
            "status": "measured",
            "unit": "ns",
            "sample_count": 4,
            "p50": 1_000_000,
        }
        for name in performance.DIRECT_COPY_METRICS
    }


def direct_processes() -> dict[str, dict[str, object]]:
    return {
        "cuda": {
            "status": "measured",
            "metadata": {
                "direct_copy_bytes": str(performance.MINIMUM_DIRECT_COPY_BYTES),
                "direct_copy_correctness": "1",
                "warmup_count": "2",
                "sample_count": "4",
                "direct_h2d_submit_boundary": "cuMemcpyHtoDAsync_api_return",
                "direct_d2h_submit_boundary": "cuMemcpyDtoHAsync_api_return",
            },
        },
        "daemon": {
            "status": "measured",
            "copy_path_counters": performance.parse_daemon_copy_path_counters(
                daemon_counter_line()
            ),
        },
    }


def fingerprint_fixture() -> dict[str, object]:
    return {
        "run_contract": {
            "canonical_milestone_budget": {
                "status": "measured",
                "budget_status": "binding",
            }
        },
        "cpu": {
            "selected_cpu": 0,
            "worker_cpu": 1,
            "numa_node": 0,
            "worker_numa_node": 0,
            "thread_siblings": "0",
            "identity": {
                "vendor_id": "AuthenticAMD",
                "model_name": "fixture-cpu",
                "microcode": "0x1",
            },
        },
        "native_reference": {
            "status": "measured",
            "device": {
                "uuid": "GPU-fixture",
                "model": "fixture-gpu",
                "bdf": "0000:01:00.0",
                "pcie_sysfs_path": "/sys/devices/pci0000:00/0000:01:00.0",
                "pcie_vendor_id": "0x10de",
                "pcie_device_id": "0x1234",
                "pcie_current_link_speed": "16.0 GT/s PCIe",
                "pcie_current_link_width": "16",
                "pcie_max_link_speed": "16.0 GT/s PCIe",
                "pcie_max_link_width": "16",
                "numa_node": 0,
            },
            "driver": {
                "module_version": "fixture-driver",
                "version_text_sha256": "1" * 64,
            },
            "provider": {
                "path": "/native/libcuda.so.1",
                "size_bytes": 1234,
                "sha256": "2" * 64,
                "build_id": "3" * 40,
            },
            "benchmark": {
                "path": "/native/copy-benchmark",
                "size_bytes": 5678,
                "sha256": "4" * 64,
            },
            "host_kernel": {
                "release": "fixture-kernel",
                "version": "fixture-kernel-build",
            },
        },
    }


def baseline_descriptor(direction: str) -> dict[str, object]:
    native_reference = fingerprint_fixture()["native_reference"]
    return {
        "schema_version": performance.NATIVE_COPY_BASELINE_SCHEMA_VERSION,
        "workload": "same_path_native_copy",
        "direction": direction,
        "api": performance.NATIVE_COPY_APIS[direction],
        "completion_boundary": "cuStreamSynchronize_return",
        "provider_mode": "native",
        "host_allocation": "malloc_pageable",
        "clock": "CLOCK_MONOTONIC_RAW",
        "copy_bytes": performance.MINIMUM_DIRECT_COPY_BYTES,
        "warmup_count": 2,
        "sample_count": 30,
        "stopping_rule": "exact_fixed_sample_count_no_deletion",
        "selected_cpu": 0,
        "worker_cpu": 1,
        "numa_node": 0,
        "cpu_vendor": "AuthenticAMD",
        "cpu_model": "fixture-cpu",
        "microcode": "0x1",
        "device": native_reference["device"],
        "driver": native_reference["driver"],
        "provider": native_reference["provider"],
        "host_kernel": native_reference["host_kernel"],
        "benchmark": {
            **native_reference["benchmark"],
            "command": performance.canonical_native_copy_command(
                native_reference,
                direction,
                2,
                30,
                performance.MINIMUM_DIRECT_COPY_BYTES,
            ),
        },
        "environment": performance.canonical_native_copy_environment(
            native_reference
        ),
        "samples_ns": list(range(1_000_000, 1_000_030)),
    }


class PerformanceRunnerTests(unittest.TestCase):
    def test_executable_artifact_does_not_require_a_nix_store_path(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            tool = Path(temporary) / "tool"
            tool.write_text("#!/bin/sh\nexit 0\n", encoding="utf-8")
            tool.chmod(0o755)
            identity = performance.artifact_fingerprint(tool)
            self.assertTrue(performance.artifact_is_executable(identity))
            self.assertFalse(str(tool).startswith("/nix/store/"))

    def test_binding_build_policy_rejects_debug_lto_and_pgo_drift(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            build_root = Path(temporary)
            binary_names = (
                "ring_benchmark",
                "cuda_benchmark",
                "nvml_benchmark",
                "daemon",
                "cuda_provider",
                "nvml_provider",
            )
            fingerprint = {
                "toolchain": {
                    "build_manifest": {
                        "status": "measured",
                        "path": str(build_root / "metaflux-build-manifest.json"),
                        "descriptor": {
                            "build_type": "Release",
                            "lto": "ON",
                            "pgo_mode": "OFF",
                            "pgo_profile_sha256": "none",
                        },
                    },
                    "binaries": {
                        name: {
                            "status": "measured",
                            "path": str(build_root / "bin" / name),
                        }
                        for name in binary_names
                    },
                }
            }
            args = SimpleNamespace(
                binding_pgo_mode="OFF", binding_pgo_profile_sha256=None
            )
            result = performance.binding_build_policy_check(args, fingerprint)
            self.assertEqual(result["status"], "pass")

            fingerprint["toolchain"]["build_manifest"]["descriptor"].update(
                {"build_type": "Debug", "lto": "OFF", "pgo_mode": "USE"}
            )
            result = performance.binding_build_policy_check(args, fingerprint)
            self.assertEqual(result["status"], "fail")
            self.assertIn("build_type", result["mismatches"])
            self.assertIn("lto", result["mismatches"])
            self.assertIn("pgo_mode", result["mismatches"])

    def test_ring_audit_trace_and_machine_counters_are_both_required(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            trace_path = Path(temporary) / "ring.strace"
            trace_path.write_text(
                '100 write(2, "METAFLUX_RING_AUDIT_BEGIN_V1\\n", 29) = 29\n'
                '100 write(2, "METAFLUX_RING_AUDIT_END_V1\\n", 27) = 27\n',
                encoding="utf-8",
            )
            trace = performance.parse_ring_audit_trace(trace_path)
            self.assertEqual(trace["status"], "measured")
            self.assertEqual(trace["syscall_count"], 0)

            audit = {
                "status": "measured",
                "effective_affinity": [2],
                "trace": trace,
                "metadata": {
                    "status": "measured",
                    "values": {
                        "audit_dispatches": 32,
                        "audit_heap_allocation_attempts": 0,
                        "audit_global_lock_acquisitions": 0,
                        "audit_consumer_doorbells": 0,
                        "audit_producer_doorbells": 0,
                    },
                },
            }
            result = performance.active_ring_audit_check({"ring_audit": audit})
            self.assertEqual(result["status"], "pass")
            self.assertTrue(all(result["requirements"].values()))

            trace_path.write_text(
                '100 write(2, "METAFLUX_RING_AUDIT_BEGIN_V1\\n", 29) = 29\n'
                "100 futex(0x1, FUTEX_WAKE, 1) = 0\n"
                '100 write(2, "METAFLUX_RING_AUDIT_END_V1\\n", 27) = 27\n',
                encoding="utf-8",
            )
            audit["trace"] = performance.parse_ring_audit_trace(trace_path)
            audit["metadata"]["values"]["audit_heap_allocation_attempts"] = 1
            result = performance.active_ring_audit_check({"ring_audit": audit})
            self.assertEqual(result["status"], "fail")
            self.assertFalse(result["requirements"]["no_syscalls_in_active_window"])
            self.assertFalse(result["requirements"]["no_heap_allocation_attempts"])

            trace_path.write_text(
                'write(2, "METAFLUX_RING_AUDIT_BEGIN_V1", 28) = 28\n'
                'write(2, "METAFLUX_RING_AUDIT_BEGIN_V1", 28) = 28\n'
                'write(2, "METAFLUX_RING_AUDIT_END_V1", 26) = 26\n',
                encoding="utf-8",
            )
            invalid = performance.parse_ring_audit_trace(trace_path)
            self.assertEqual(invalid["status"], "invalid")
            self.assertEqual(invalid["reason"], "ring_audit_trace_marker_mismatch")

    def test_daemon_copy_path_counters_require_one_complete_machine_line(self) -> None:
        parsed = performance.parse_daemon_copy_path_counters(daemon_counter_line())
        self.assertEqual(parsed["status"], "measured")
        self.assertEqual(parsed["values"]["host-address-space-registrations"], 1)
        self.assertEqual(parsed["values"]["staged-host-destination-bytes"], 0)

        missing = performance.parse_daemon_copy_path_counters(
            "metafluxd: direct-host-source-operations=1\n"
        )
        self.assertEqual(missing["status"], "invalid")
        self.assertEqual(missing["reason"], "daemon_copy_path_counter_missing")

        duplicate = performance.parse_daemon_copy_path_counters(
            daemon_counter_line() + daemon_counter_line()
        )
        self.assertEqual(duplicate["status"], "invalid")
        self.assertEqual(duplicate["reason"], "daemon_copy_path_counter_line_count")

    def test_direct_host_copy_path_passes_only_from_observed_evidence(self) -> None:
        result = performance.direct_host_copy_path_check(direct_metrics(), direct_processes())
        self.assertEqual(result["status"], "pass")
        self.assertTrue(all(result["requirements"].values()))
        self.assertEqual(result["required_large_copy_operations_per_direction"], 7)
        self.assertEqual(
            result["required_direct_bytes_per_direction"],
            7 * performance.MINIMUM_DIRECT_COPY_BYTES,
        )

    def test_direct_host_copy_path_rejects_staging_and_short_or_incorrect_copy(self) -> None:
        staged = direct_processes()
        staged["daemon"]["copy_path_counters"] = performance.parse_daemon_copy_path_counters(
            daemon_counter_line(**{"staged-host-source-operations": 1})
        )
        staged_result = performance.direct_host_copy_path_check(direct_metrics(), staged)
        self.assertEqual(staged_result["status"], "fail")
        self.assertFalse(staged_result["requirements"]["no_staged_host_source"])

        for field, value, requirement in (
            ("direct_copy_bytes", "4096", "copy_size_at_least_16_mib"),
            ("direct_copy_correctness", "0", "benchmark_correctness"),
        ):
            with self.subTest(field=field):
                processes = copy.deepcopy(direct_processes())
                processes["cuda"]["metadata"][field] = value
                result = performance.direct_host_copy_path_check(direct_metrics(), processes)
                self.assertEqual(result["status"], "fail")
                self.assertFalse(result["requirements"][requirement])

        below_workload = direct_processes()
        below_workload["daemon"]["copy_path_counters"] = (
            performance.parse_daemon_copy_path_counters(
                daemon_counter_line(
                    **{
                        "direct-host-source-bytes": 7
                        * performance.MINIMUM_DIRECT_COPY_BYTES
                        - 1
                    }
                )
            )
        )
        result = performance.direct_host_copy_path_check(
            direct_metrics(), below_workload
        )
        self.assertEqual(result["status"], "fail")
        self.assertFalse(
            result["requirements"]["direct_host_source_bytes_cover_workload"]
        )

    def test_direct_host_copy_path_rejects_missing_counter_or_metric(self) -> None:
        processes = direct_processes()
        processes["daemon"]["copy_path_counters"] = {
            "status": "invalid",
            "reason": "fixture",
        }
        result = performance.direct_host_copy_path_check(direct_metrics(), processes)
        self.assertEqual(result["status"], "fail")
        self.assertFalse(result["requirements"]["daemon_counters_measured"])

        metrics = direct_metrics()
        del metrics[performance.DIRECT_COPY_METRICS[0]]
        result = performance.direct_host_copy_path_check(metrics, direct_processes())
        self.assertEqual(result["status"], "fail")
        self.assertFalse(result["requirements"]["metrics_measured_in_ns"])

    def test_direct_sample_protocol_is_summarized_without_special_cases(self) -> None:
        lines = []
        for metric in performance.DIRECT_COPY_METRICS:
            lines.extend(
                (
                    f"METAFLUX_SAMPLE\t{metric}\t0\t11\tns",
                    f"METAFLUX_SAMPLE\t{metric}\t1\t17\tns",
                )
            )
        lines.extend(
            (
                f"METAFLUX_METADATA\tdirect_copy_bytes\t{performance.MINIMUM_DIRECT_COPY_BYTES}",
                "METAFLUX_METADATA\tdirect_copy_correctness\t1",
            )
        )
        rows, metadata, unparsed = performance.parse_benchmark_output(
            "cuda", "\n".join(lines) + "\n"
        )
        summaries = performance.summarize_metrics(rows, 2)
        self.assertEqual(set(summaries), set(performance.DIRECT_COPY_METRICS))
        self.assertEqual(metadata["direct_copy_correctness"], "1")
        self.assertEqual(unparsed, [])

    def test_native_baseline_schema_binds_direction_and_raw_samples(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            baseline_path = Path(temporary) / "h2d.json"
            baseline_path.write_text(
                json.dumps(baseline_descriptor("h2d")), encoding="utf-8"
            )
            measured = performance.load_native_copy_baseline(
                baseline_path,
                fingerprint_fixture(),
                performance.MINIMUM_DIRECT_COPY_BYTES,
                "h2d",
                2,
                30,
            )
            self.assertEqual(measured["status"], "measured")
            self.assertEqual(measured["direction"], "h2d")
            self.assertEqual(measured["sample_count"], 30)
            self.assertEqual(len(measured["samples_ns_sha256"]), 64)
            self.assertEqual(measured["p50_ns"], 1_000_014)

            mismatch = performance.load_native_copy_baseline(
                baseline_path,
                fingerprint_fixture(),
                performance.MINIMUM_DIRECT_COPY_BYTES,
                "d2h",
                2,
                30,
            )
            self.assertEqual(mismatch["status"], "skipped")
            self.assertIn("direction", mismatch["mismatches"])
            self.assertIn("api", mismatch["mismatches"])

            pinned = baseline_descriptor("h2d")
            pinned["host_allocation"] = "cuda_host_alloc_pinned"
            baseline_path.write_text(json.dumps(pinned), encoding="utf-8")
            mismatch = performance.load_native_copy_baseline(
                baseline_path,
                fingerprint_fixture(),
                performance.MINIMUM_DIRECT_COPY_BYTES,
                "h2d",
                2,
                30,
            )
            self.assertEqual(mismatch["status"], "skipped")
            self.assertIn("host_allocation", mismatch["mismatches"])

            wrong_identity = baseline_descriptor("h2d")
            wrong_identity["device"] = {
                **wrong_identity["device"],
                "uuid": "GPU-other",
            }
            wrong_identity["benchmark"]["command"][0] = "/other/benchmark"
            wrong_identity["environment"]["CUDA_VISIBLE_DEVICES"] = "GPU-other"
            baseline_path.write_text(
                json.dumps(wrong_identity), encoding="utf-8"
            )
            mismatch = performance.load_native_copy_baseline(
                baseline_path,
                fingerprint_fixture(),
                performance.MINIMUM_DIRECT_COPY_BYTES,
                "h2d",
                2,
                30,
            )
            self.assertEqual(mismatch["status"], "skipped")
            self.assertIn("device", mismatch["mismatches"])
            self.assertIn("benchmark_command", mismatch["mismatches"])
            self.assertIn("environment", mismatch["mismatches"])

            unavailable_fingerprint = fingerprint_fixture()
            unavailable_fingerprint["native_reference"] = {
                "status": "skipped",
                "reason": "native_reference_fixture_missing",
            }
            baseline_path.write_text(
                json.dumps(baseline_descriptor("h2d")), encoding="utf-8"
            )
            unavailable = performance.load_native_copy_baseline(
                baseline_path,
                unavailable_fingerprint,
                performance.MINIMUM_DIRECT_COPY_BYTES,
                "h2d",
                2,
                30,
            )
            self.assertEqual(unavailable["status"], "skipped")
            self.assertEqual(
                unavailable["reason"], "native_reference_identity_unavailable"
            )

    def test_each_copy_direction_has_an_independent_throughput_gate(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            for direction in ("h2d", "d2h", "d2d"):
                with self.subTest(direction=direction):
                    baseline_path = Path(temporary) / f"{direction}.json"
                    baseline_path.write_text(
                        json.dumps(baseline_descriptor(direction)), encoding="utf-8"
                    )
                    metric = performance.COPY_COMPLETION_METRICS[direction]
                    result = performance.copy_throughput_check(
                        metrics={
                            metric: {
                                "status": "measured",
                                "unit": "ns",
                                "sample_count": 30,
                                "p50": 1_000_000,
                            }
                        },
                        fingerprint=fingerprint_fixture(),
                        direction=direction,
                        copy_bytes=performance.MINIMUM_DIRECT_COPY_BYTES,
                        warmup_count=2,
                        sample_count=30,
                        baseline_path=baseline_path,
                        eligible=True,
                        ineligible_reason="eligible",
                    )
                    self.assertEqual(result["status"], "pass")
                    self.assertEqual(result["native_baseline"]["direction"], direction)
                    self.assertGreaterEqual(result["ratio"], result["limit"])

    def test_binding_gate_requires_both_directional_native_baselines(self) -> None:
        args = SimpleNamespace(
            mode="binding-reference",
            budget_status="binding",
            reference_host_role="amd",
            controlled_host=True,
            native_h2d_baseline_json=None,
            native_d2h_baseline_json=None,
            native_d2d_baseline_json=None,
        )
        metrics = direct_metrics()
        metrics["monotonic_raw_pair_overhead_ns"] = {
            "status": "measured",
            "unit": "ns",
            "p99": 50,
        }
        result = performance.qualify(
            args, fingerprint_fixture(), metrics, direct_processes()
        )
        self.assertEqual(
            result["checks"]["direct_h2d_copy_throughput"]["reason"],
            "same_path_native_baseline_not_configured",
        )
        self.assertEqual(
            result["checks"]["direct_d2h_copy_throughput"]["reason"],
            "same_path_native_baseline_not_configured",
        )
        self.assertNotEqual(result["m0100_performance_gate_status"], "pass")

    def test_binding_gate_rejects_unavailable_physical_identity(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            h2d = Path(temporary) / "h2d.json"
            d2h = Path(temporary) / "d2h.json"
            h2d.write_text(json.dumps(baseline_descriptor("h2d")), encoding="utf-8")
            d2h.write_text(json.dumps(baseline_descriptor("d2h")), encoding="utf-8")
            args = SimpleNamespace(
                mode="binding-reference",
                budget_status="binding",
                reference_host_role="amd",
                controlled_host=True,
                native_h2d_baseline_json=h2d,
                native_d2h_baseline_json=d2h,
                native_d2d_baseline_json=None,
            )
            fingerprint = fingerprint_fixture()
            fingerprint["native_reference"] = {
                "status": "skipped",
                "reason": "physical_identity_fixture_missing",
            }
            metrics = direct_metrics()
            metrics["monotonic_raw_pair_overhead_ns"] = {
                "status": "measured",
                "unit": "ns",
                "p99": 50,
            }
            result = performance.qualify(
                args, fingerprint, metrics, direct_processes()
            )
            for direction in ("h2d", "d2h"):
                check = result["checks"][f"direct_{direction}_copy_throughput"]
                self.assertEqual(check["status"], "skipped")
                self.assertEqual(
                    check["reason"], "native_reference_identity_unavailable"
                )
            self.assertNotEqual(result["m0100_performance_gate_status"], "pass")


if __name__ == "__main__":
    unittest.main()
