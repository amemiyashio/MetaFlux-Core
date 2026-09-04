#!/usr/bin/env python3
"""Self-test the milestone-0.1.1.5 live cdev profile archiver with synthetic data."""

from __future__ import annotations

import csv
import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ARCHIVER = ROOT / "tests" / "performance" / "archive_live_cdev_profile.py"

REQUIRED_METRICS = [
    "cdev_poll_online_ns",
    "cdev_block_wait_empty_ns",
    "cdev_batch_submit_ns",
    "cdev_irq_eventfd_roundtrip_ns",
]
SAMPLE_COUNT = 10000
WARMUP_COUNT = 1000


def build_live_stdout() -> str:
    lines: list[str] = []
    lines.append(f"METAFLUX_METADATA\tsample_count\t{SAMPLE_COUNT}")
    lines.append(f"METAFLUX_METADATA\twarmup_count\t{WARMUP_COUNT}")
    for metric in REQUIRED_METRICS:
        for index in range(SAMPLE_COUNT):
            lines.append(f"METAFLUX_SAMPLE\t{metric}\t{index}\t{100 + index}\tns")
    lines.append("")
    lines.append("cdev qualification: PASS")
    lines.append("")
    return "\n".join(lines)


def read_raw_csv(csv_path: Path) -> list[dict[str, str]]:
    with csv_path.open(encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle))


def main() -> int:
    live_text = build_live_stdout()
    with tempfile.TemporaryDirectory(prefix="metaflux-live-cdev-selftest-") as directory:
        live_path = Path(directory) / "live-stdout.txt"
        live_path.write_text(live_text, encoding="utf-8")
        output_dir = Path(directory) / "output"

        result = subprocess.run(
            [
                sys.executable,
                "-B",
                str(ARCHIVER),
                "--live-stdout",
                str(live_path),
                "--output-dir",
                str(output_dir),
            ],
            capture_output=True,
            text=True,
        )
        if result.returncode != 0:
            raise AssertionError(
                f"archive_live_cdev_profile exited {result.returncode}\n"
                f"stdout: {result.stdout}\nstderr: {result.stderr}"
            )

        summary = json.loads(result.stdout)
        assert summary["status"] == "measured-live-cdev"
        assert summary["sample_count"] == SAMPLE_COUNT

        report = json.loads(
            (output_dir / "live-cdev-profile-report.json").read_text(encoding="utf-8")
        )
        assert report["status"] == "measured-live-cdev"
        assert report["contract"]["sample_count"] == SAMPLE_COUNT
        assert report["contract"]["warmup_count"] == WARMUP_COUNT
        for metric in REQUIRED_METRICS:
            assert metric in report["metrics"], f"missing metric: {metric}"
            assert report["metrics"][metric]["count"] == SAMPLE_COUNT

        raw = read_raw_csv(output_dir / "raw-samples.csv")
        assert len(raw) == SAMPLE_COUNT * len(REQUIRED_METRICS)
        fields = set(raw[0].keys())
        assert fields == {"metric", "sample_index", "value", "unit"}

        assert report["artifacts"]["live_stdout_sha256"]
        assert report["artifacts"]["raw_samples_sha256"]
        assert report["modes"]["poll"]["status"] == "measured"
        assert report["modes"]["block"]["status"] == "measured"
        assert report["modes"]["batch"]["status"] == "measured"
        assert report["modes"]["irq"]["status"] == "measured"
        assert "guest-vfio-user-add-copy under pinned QEMU/libvfio-user" in report[
            "still_host_pending"
        ]
    print("live-cdev-archive self-test: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
