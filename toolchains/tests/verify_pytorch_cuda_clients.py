#!/usr/bin/env python3
"""Statically verify the pinned PyTorch CUDA client wheel closures."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path
from typing import Any
from urllib.parse import unquote, urlparse


EXPECTED_PROFILES = {
    "baseline": {
        "role": "sm70-regression-probe",
        "torch_version": "2.11.0+cu126",
        "cuda_version": "12.6",
        "cuda_toolkit_version": "12.6.3",
        "cuda_generation": 12,
    },
    "frontier": {
        "role": "future-sm80-gap-probe",
        "torch_version": "2.13.0+cu132",
        "cuda_version": "13.2",
        "cuda_toolkit_version": "13.2.1",
        "cuda_generation": 13,
    },
}
EXPECTED_PYTHON = {
    "implementation": "CPython",
    "version": "3.13.15",
    "abi": "cp313",
    "platform": "x86_64-linux",
}
EXPECTED_WHEELS = {
    "baseline": {
        "torch": "2.11.0+cu126",
        "cuda-toolkit": "12.6.3",
        "nvidia-cudnn-cu12": "9.10.2.21",
        "nvidia-cusparselt-cu12": "0.7.1",
        "nvidia-nccl-cu12": "2.28.9",
        "nvidia-nvshmem-cu12": "3.4.5",
        "triton": "3.6.0",
        "cuda-bindings": "12.9.7",
        "cuda-pathfinder": "1.8.0",
        "nvidia-cublas-cu12": "12.6.4.1",
        "nvidia-cuda-cupti-cu12": "12.6.80",
        "nvidia-cuda-nvrtc-cu12": "12.6.85",
        "nvidia-cuda-runtime-cu12": "12.6.77",
        "nvidia-cufft-cu12": "11.3.0.4",
        "nvidia-cufile-cu12": "1.11.1.6",
        "nvidia-curand-cu12": "10.3.7.77",
        "nvidia-cusolver-cu12": "11.7.1.2",
        "nvidia-cusparse-cu12": "12.5.4.2",
        "nvidia-nvjitlink-cu12": "12.6.85",
        "nvidia-nvtx-cu12": "12.6.77",
        "setuptools": "81.0.0",
        "fsspec": "2026.7.0",
        "networkx": "3.6.1",
        "sympy": "1.14.0",
        "mpmath": "1.3.0",
        "typing-extensions": "4.16.0",
        "filelock": "3.32.4",
        "jinja2": "3.1.6",
        "markupsafe": "3.0.3",
    },
    "frontier": {
        "torch": "2.13.0+cu132",
        "cuda-toolkit": "13.2.1",
        "nvidia-cudnn-cu13": "9.20.0.48",
        "nvidia-cusparselt-cu13": "0.8.1",
        "nvidia-nccl-cu13": "2.29.7",
        "nvidia-nvshmem-cu13": "3.4.5",
        "triton": "3.7.1",
        "cuda-bindings": "13.3.1",
        "nvidia-cublas": "13.4.0.1",
        "nvidia-cuda-cupti": "13.2.75",
        "nvidia-cuda-nvrtc": "13.2.78",
        "nvidia-cuda-runtime": "13.2.75",
        "nvidia-cufft": "12.2.0.46",
        "nvidia-cufile": "1.17.1.22",
        "nvidia-curand": "10.4.2.55",
        "nvidia-cusolver": "12.2.0.1",
        "nvidia-cusparse": "12.7.10.1",
        "nvidia-nvjitlink": "13.3.33",
        "nvidia-nvtx": "13.2.75",
        "cuda-pathfinder": "1.8.0",
        "fsspec": "2026.7.0",
        "networkx": "3.6.1",
        "setuptools": "84.0.0",
        "sympy": "1.14.0",
        "mpmath": "1.3.0",
        "typing-extensions": "4.16.0",
        "filelock": "3.32.4",
        "jinja2": "3.1.6",
        "markupsafe": "3.0.3",
    },
}
SHA256_PATTERN = re.compile(r"[0-9a-f]{64}")
CANONICAL_TORCH_HOST = "download-r2.pytorch.org"
CANONICAL_PYPI_HOST = "files.pythonhosted.org"


def fail(message: str) -> None:
    raise ValueError(message)


def parse_arguments() -> argparse.Namespace:
    repository = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--manifest",
        type=Path,
        default=repository / "toolchains/pytorch-cuda-clients-1.json",
    )
    return parser.parse_args()


def load_json(path: Path) -> dict[str, Any]:
    document = json.loads(path.read_text(encoding="ascii"))
    if not isinstance(document, dict):
        fail(f"{path} must contain a JSON object")
    return document


def canonical_name(name: str) -> str:
    return re.sub(r"[-_.]+", "-", name).lower()


def verify_wheel_filename(profile_name: str, wheel: dict[str, Any]) -> None:
    filename = wheel["filename"]
    if not isinstance(filename, str) or not filename.endswith(".whl"):
        fail(f"{profile_name} wheel {wheel['name']} has an invalid filename")
    components = filename[:-4].rsplit("-", 3)
    if len(components) != 4 or "-" not in components[0]:
        fail(f"{profile_name} wheel {wheel['name']} filename is not PEP 427 shaped")
    name_and_version, python_tag, abi_tag, platform_tag = components
    distribution, version = name_and_version.split("-", 1)
    if canonical_name(distribution) != canonical_name(wheel["name"]):
        fail(f"{profile_name} wheel {wheel['name']} filename distribution differs")
    if version != wheel["version"]:
        fail(f"{profile_name} wheel {wheel['name']} filename version differs")

    allowed_python_abis = {
        ("cp313", "cp313"),
        ("py3", "none"),
        ("py2.py3", "none"),
    }
    if (python_tag, abi_tag) not in allowed_python_abis:
        fail(f"{profile_name} wheel {wheel['name']} does not support CPython 3.13")

    platforms = platform_tag.split(".")
    if platforms == ["any"]:
        if abi_tag != "none":
            fail(f"{profile_name} wheel {wheel['name']} has an ABI-specific any tag")
        return
    manylinux_pattern = re.compile(r"manylinux(?:1|2010|2014|_[0-9]+_[0-9]+)_x86_64")
    if not all(manylinux_pattern.fullmatch(platform) for platform in platforms):
        fail(f"{profile_name} wheel {wheel['name']} is not an x86_64 manylinux wheel")


def verify_wheel(profile_name: str, generation: int, wheel: dict[str, Any]) -> None:
    required = {"name", "version", "filename", "urls", "size", "sha256"}
    if set(wheel) != required:
        fail(f"{profile_name} wheel fields differ for {wheel.get('name')!r}")
    if not isinstance(wheel["size"], int) or wheel["size"] <= 0:
        fail(f"{profile_name} wheel {wheel['name']} has an invalid size")
    if SHA256_PATTERN.fullmatch(wheel["sha256"]) is None:
        fail(f"{profile_name} wheel {wheel['name']} has an invalid SHA256")
    verify_wheel_filename(profile_name, wheel)
    urls = wheel["urls"]
    if not isinstance(urls, list) or len(urls) != 3 or len(set(urls)) != len(urls):
        fail(f"{profile_name} wheel {wheel['name']} must have three unique routed URLs")
    parsed_urls = [urlparse(url) for url in urls]
    if any(parsed.scheme != "https" or not parsed.hostname for parsed in parsed_urls):
        fail(f"{profile_name} wheel {wheel['name']} URL must use HTTPS")
    canonical_host = (
        CANONICAL_TORCH_HOST
        if canonical_name(wheel["name"]) == "torch"
        else CANONICAL_PYPI_HOST
    )
    if parsed_urls[0].hostname == canonical_host:
        fail(f"{profile_name} wheel {wheel['name']} does not try a mirror first")
    if parsed_urls[1].hostname in {parsed_urls[0].hostname, canonical_host}:
        fail(f"{profile_name} wheel {wheel['name']} has no distinct adjacent mirror")
    if parsed_urls[-1].hostname != canonical_host:
        fail(f"{profile_name} wheel {wheel['name']} does not end at canonical upstream")
    for parsed in parsed_urls:
        if unquote(Path(parsed.path).name) != wheel["filename"]:
            fail(f"{profile_name} wheel {wheel['name']} URL filename differs")

    normalized = canonical_name(wheel["name"])
    opposite = 13 if generation == 12 else 12
    if f"cu{opposite}" in normalized or f"cu{opposite}" in wheel["version"].lower():
        fail(f"{profile_name} wheel {wheel['name']} crosses CUDA generations")
    if normalized.startswith("nvidia-"):
        explicit_generation = re.search(r"-cu(12|13)$", normalized)
        if explicit_generation and int(explicit_generation.group(1)) != generation:
            fail(f"{profile_name} NVIDIA wheel {wheel['name']} has the wrong CUDA generation")


def main() -> int:
    arguments = parse_arguments()
    manifest_path = arguments.manifest.resolve(strict=True)
    repository = manifest_path.parents[1]
    manifest = load_json(manifest_path)
    if manifest.get("schema_version") != 1 or manifest.get("epoch") != 1:
        fail("unsupported PyTorch CUDA client manifest epoch")
    if manifest.get("python") != EXPECTED_PYTHON:
        fail("PyTorch CUDA clients must use the pinned CPython 3.13.15 identity")
    profiles = manifest.get("profiles")
    if not isinstance(profiles, dict) or set(profiles) != set(EXPECTED_PROFILES):
        fail("PyTorch CUDA client profile set must be baseline/frontier")

    for profile_name, expected in EXPECTED_PROFILES.items():
        profile = profiles[profile_name]
        for field in (
            "role",
            "torch_version",
            "cuda_version",
            "cuda_toolkit_version",
        ):
            if profile.get(field) != expected[field]:
                fail(f"{profile_name} {field} differs from the frozen profile")
        lock_text = profile.get("wheel_lock")
        if not isinstance(lock_text, str) or not lock_text.startswith(
            "toolchains/pytorch-cuda-clients/"
        ):
            fail(f"{profile_name} wheel lock must stay under toolchains/")
        lock_path = (repository / lock_text).resolve(strict=True)
        if repository not in lock_path.parents:
            fail(f"{profile_name} wheel lock escapes the repository")
        lock = load_json(lock_path)
        expected_header = {
            "schema_version": 1,
            "profile": profile_name,
            "python_abi": EXPECTED_PYTHON["abi"],
            "platform": EXPECTED_PYTHON["platform"],
            "cuda_generation": expected["cuda_generation"],
        }
        for field, value in expected_header.items():
            if lock.get(field) != value:
                fail(f"{profile_name} wheel lock {field} mismatch")
        wheels = lock.get("wheels")
        if not isinstance(wheels, list) or not wheels:
            fail(f"{profile_name} wheel lock is empty")
        names = [canonical_name(wheel.get("name", "")) for wheel in wheels]
        if len(set(names)) != len(names):
            fail(f"{profile_name} wheel lock contains duplicate names")
        for wheel in wheels:
            verify_wheel(profile_name, expected["cuda_generation"], wheel)
        versions = {
            canonical_name(wheel["name"]): wheel["version"] for wheel in wheels
        }
        if versions != EXPECTED_WHEELS[profile_name]:
            fail(f"{profile_name} wheel closure differs from the frozen resolution")
        if versions["torch"] != expected["torch_version"]:
            fail(f"{profile_name} torch wheel does not match the profile")
        if versions["cuda-toolkit"] != expected["cuda_toolkit_version"]:
            fail(f"{profile_name} cuda-toolkit wheel does not match the profile")

    print("PyTorch CUDA client manifests: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
