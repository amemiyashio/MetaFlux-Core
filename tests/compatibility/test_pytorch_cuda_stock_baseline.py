#!/usr/bin/env python3
"""Unit tests for stock PyTorch CUDA baseline evidence parsing."""

from __future__ import annotations

import importlib.util
from pathlib import Path
import unittest


COMPATIBILITY_DIR = Path(__file__).resolve().parent
RUNNER_PATH = COMPATIBILITY_DIR / "run_pytorch_cuda_stock_baseline.py"
SPECIFICATION = importlib.util.spec_from_file_location("stock_baseline", RUNNER_PATH)
assert SPECIFICATION is not None and SPECIFICATION.loader is not None
stock_baseline = importlib.util.module_from_spec(SPECIFICATION)
SPECIFICATION.loader.exec_module(stock_baseline)


class StockBaselineEvidenceTests(unittest.TestCase):
    def test_provider_surface_keeps_direct_calls_separate_from_resolver_probes(self) -> None:
        trace = "\n".join(
            (
                "MF_ENTRY mf_cuda_managed_cuInit",
                "MF_ENTRY mf_cuda_managed_cuInit",
                "MF_ENTRY mf_cuda_managed_cuGetExportTable",
                "MF_STUB_CALL mf_cuda_managed_cuCtxGetApiVersion",
                "MF_PYTORCH_BASELINE_MODULE artifact=1/1 module=2/1",
                "MF_EXPORT_TABLE a094798c-2e74-2e74-93f2-0800200c0a66",
                "MF_LAUNCH f=0x123 grid=1x64",
                "resolver probe: cuMemAlloc_v2",
            )
        )

        surface = stock_baseline.provider_execution_surface(trace)

        self.assertEqual(surface["kind"], "direct-provider-entrypoints")
        self.assertEqual(
            surface["entrypoints"],
            ["mf_cuda_managed_cuGetExportTable", "mf_cuda_managed_cuInit"],
        )
        self.assertEqual(surface["typed_stub_entrypoints"], ["mf_cuda_managed_cuCtxGetApiVersion"])
        self.assertEqual(
            surface["internal_tables"],
            ["a094798c-2e74-2e74-93f2-0800200c0a66"],
        )
        self.assertEqual(surface["unknown_internal_table_requests"], 0)
        self.assertEqual(surface["canonical_artifact_module_loads"], 1)
        self.assertEqual(surface["launches"], 1)
        self.assertEqual(surface["local_semantic_execution_events"], 0)

    def test_baseline_surface_requires_the_exact_pinned_profile(self) -> None:
        surface = {
            "entrypoints": sorted(stock_baseline.BASELINE_DIRECT_PROVIDER_ENTRYPOINTS),
            "typed_stub_entrypoints": sorted(stock_baseline.BASELINE_TYPED_STUB_ENTRYPOINTS),
            "internal_tables": sorted(stock_baseline.BASELINE_INTERNAL_TABLES),
        }

        stock_baseline.require_baseline_execution_surface(surface)

        surface["entrypoints"] = surface["entrypoints"][:-1]
        with self.assertRaisesRegex(RuntimeError, "direct provider surface drifted"):
            stock_baseline.require_baseline_execution_surface(surface)

        surface["entrypoints"] = sorted(stock_baseline.BASELINE_DIRECT_PROVIDER_ENTRYPOINTS)
        surface["typed_stub_entrypoints"] = surface["typed_stub_entrypoints"][:-1]
        with self.assertRaisesRegex(RuntimeError, "typed-stub surface drifted"):
            stock_baseline.require_baseline_execution_surface(surface)

    def test_daemon_statistics_require_one_complete_record(self) -> None:
        output = (
            "metafluxd: cpu-execution mode=interpreter compiler-requests=0 "
            "cache-hits=0 cache-misses=0 loaded-modules=2 "
            "host-address-space-registrations=3\n"
        )

        statistics = stock_baseline.daemon_execution_statistics(output)

        self.assertEqual(
            statistics,
            {
                "mode": "interpreter",
                "compiler_requests": 0,
                "cache_hits": 0,
                "cache_misses": 0,
                "loaded_modules": 2,
            },
        )
        with self.assertRaisesRegex(RuntimeError, "exactly one"):
            stock_baseline.daemon_execution_statistics("")

    def test_baseline_cpu_selection_uses_one_effective_cpu(self) -> None:
        self.assertEqual(stock_baseline.select_baseline_cpu({4, 9, 12}), 4)
        with self.assertRaisesRegex(RuntimeError, "no effective CPU affinity"):
            stock_baseline.select_baseline_cpu(set())


if __name__ == "__main__":
    unittest.main()
